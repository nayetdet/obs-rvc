#include "rvc-filter.h"
#include "rvc-filter-conversion.hpp"
#include "rvc-filter-validation.hpp"
#include "utils/hardware-utils.hpp"
#include "utils/file-utils.hpp"
#include "utils/string-utils.hpp"

#include <obs-module.h>
#include <obs.h>
#include <obs-data.h>
#include <obs-properties.h>
#include <plugin-support.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>

namespace {
constexpr char kSpeaker[] = "speaker";
constexpr char kF0UpKey[] = "f0_up_key";
constexpr char kF0Method[] = "f0_method";
constexpr char kFilterRadius[] = "filter_radius";
constexpr char kResampleSr[] = "resample_sr";
constexpr char kRmsMixRate[] = "rms_mix_rate";
constexpr char kProtect[] = "protect";
constexpr char kIndexRate[] = "index_rate";
constexpr char kChunkDurationMs[] = "chunk_duration_ms";
constexpr char kInitialChunkDurationMs[] = "initial_chunk_duration_ms";
constexpr char kMaximumChunkDurationMs[] = "maximum_chunk_duration_ms";
constexpr char kInferenceThreads[] = "inference_threads";
constexpr char kObsReservedThreads[] = "obs_reserved_threads";
constexpr char kRenderingWarning[] = "rendering_warning";
constexpr uint32_t kWorkerStartupTimeoutMs = 30000U;
constexpr uint32_t kWorkerConfigureAttempts = 2U;
rvc_ipc_t *g_ipc = nullptr;

bool cpu_thread_allocation_modified(obs_properties_t *properties, obs_property_t *property, obs_data_t *settings)
{
	const int32_t total_threads =
		static_cast<int32_t>(std::clamp<int64_t>(rvc::utils::physical_cpu_count(), 1, 256));
	const int32_t changed_threads = static_cast<int32_t>(obs_data_get_int(settings, obs_property_name(property)));
	int32_t inference_threads = static_cast<int32_t>(obs_data_get_int(settings, kInferenceThreads));
	int32_t obs_reserved_threads = static_cast<int32_t>(obs_data_get_int(settings, kObsReservedThreads));
	if (std::strcmp(obs_property_name(property), kInferenceThreads) == 0) {
		inference_threads = std::clamp(changed_threads, 1, total_threads);
		obs_reserved_threads = std::clamp(obs_reserved_threads, 0, total_threads - inference_threads);
	} else {
		obs_reserved_threads = std::clamp(changed_threads, 0, total_threads - 1);
		inference_threads = std::clamp(inference_threads, 1, total_threads - obs_reserved_threads);
	}

	obs_data_set_int(settings, kInferenceThreads, inference_threads);
	obs_data_set_int(settings, kObsReservedThreads, obs_reserved_threads);
	obs_property_int_set_limits(obs_properties_get(properties, kInferenceThreads), 1,
				    total_threads - obs_reserved_threads, 1);
	obs_property_int_set_limits(obs_properties_get(properties, kObsReservedThreads), 0,
				    total_threads - inference_threads, 1);
	return true;
}

const char *rvc_filter_name(void *)
{
	return "Retrieval-based Voice Conversion";
}

void rvc_filter_defaults(obs_data_t *settings)
{
	rvc::utils::set_default_module_file(settings, rvc::filter::kModel, "models/rvc/miku_default_rvc.pth");
	rvc::utils::set_default_module_file(settings, rvc::filter::kHubertPath, "models/hubert/hubert_base.pt");
	rvc::utils::set_default_module_file(settings, rvc::filter::kRmvpePath, "models/rmvpe/rmvpe.pt");
	obs_data_set_default_string(settings, kF0Method, "rmvpe");
	obs_data_set_default_int(settings, kSpeaker, 0);
	obs_data_set_default_int(settings, kF0UpKey, 6);
	obs_data_set_default_int(settings, kFilterRadius, 3);
	obs_data_set_default_int(settings, kResampleSr, 0);
	obs_data_set_default_double(settings, kRmsMixRate, 0.25);
	obs_data_set_default_double(settings, kProtect, 0.33);
	obs_data_set_default_double(settings, kIndexRate, 1.0);
	obs_data_set_default_int(settings, kChunkDurationMs, 500);
	obs_data_set_default_int(settings, kInitialChunkDurationMs, 500);
	obs_data_set_default_int(settings, kMaximumChunkDurationMs, 2000);
	const int64_t total_threads = rvc::utils::physical_cpu_count();
	const int64_t default_obs_threads = total_threads >= 4 ? 2 : 0;
	obs_data_set_default_int(settings, kObsReservedThreads, default_obs_threads);
	const int64_t reserved_threads =
		std::clamp<int64_t>(obs_data_get_int(settings, kObsReservedThreads), 0, total_threads - 1);
	obs_data_set_default_int(settings, kInferenceThreads, std::max<int64_t>(1, total_threads - reserved_threads));
}

obs_properties_t *rvc_filter_properties(void *)
{
	obs_properties_t *properties = obs_properties_create();
	const std::string rendering_warning = rvc::utils::software_rendering_warning();
	if (!rendering_warning.empty()) {
		obs_property_t *warning = obs_properties_add_text(properties, kRenderingWarning,
								  rendering_warning.c_str(), OBS_TEXT_INFO);
		if (warning != nullptr)
			obs_property_text_set_info_type(warning, OBS_TEXT_INFO_WARNING);
	}

	obs_properties_add_path(properties, rvc::filter::kModel, "Model", OBS_PATH_FILE, "RVC model (*.pth)", nullptr);
	obs_properties_add_path(properties, rvc::filter::kHubertPath, "HuBERT model", OBS_PATH_FILE,
				"HuBERT model (*.pt)", nullptr);
	obs_properties_add_path(properties, rvc::filter::kRmvpePath, "RMVPE model", OBS_PATH_FILE, "RMVPE model (*.pt)",
				nullptr);
	obs_property_t *status = obs_properties_add_text(properties, rvc::filter::kModelStatus,
							 "All model files are valid.", OBS_TEXT_INFO);

	if (status != nullptr)
		obs_property_text_set_info_type(status, OBS_TEXT_INFO_NORMAL);

	obs_property_set_modified_callback(obs_properties_get(properties, rvc::filter::kModel),
					   rvc::filter::model_path_modified);
	obs_property_set_modified_callback(obs_properties_get(properties, rvc::filter::kHubertPath),
					   rvc::filter::model_path_modified);
	obs_property_set_modified_callback(obs_properties_get(properties, rvc::filter::kRmvpePath),
					   rvc::filter::model_path_modified);
	obs_property_t *f0_method = obs_properties_add_list(properties, kF0Method, "F0 method", OBS_COMBO_TYPE_LIST,
							    OBS_COMBO_FORMAT_STRING);

	obs_property_list_add_string(f0_method, "RMVPE", "rmvpe");
	obs_property_list_add_string(f0_method, "Harvest", "harvest");
	obs_property_list_add_string(f0_method, "Crepe", "crepe");
	obs_property_list_add_string(f0_method, "PM", "pm");
	obs_properties_add_int(properties, kSpeaker, "Speaker", 0, 32, 1);
	obs_properties_add_int(properties, kF0UpKey, "F0 transpose", -12, 12, 1);
	obs_properties_add_int(properties, kFilterRadius, "Filter radius", 0, 7, 1);
	obs_properties_add_int(properties, kResampleSr, "Resample rate", 0, 192000, 1000);
	obs_properties_add_float_slider(properties, kRmsMixRate, "RMS mix rate", 0.0, 1.0, 0.01);
	obs_properties_add_float_slider(properties, kProtect, "Protect", 0.0, 0.5, 0.01);
	obs_properties_add_float_slider(properties, kIndexRate, "Index rate", 0.0, 1.0, 0.01);
	obs_properties_add_int(properties, kInitialChunkDurationMs, "Initial conversion block (ms)", 500, 2000, 10);
	obs_properties_add_int(properties, kMaximumChunkDurationMs, "Maximum conversion block (ms)", 500, 2000, 10);
	const int32_t total_threads =
		static_cast<int32_t>(std::clamp<int64_t>(rvc::utils::physical_cpu_count(), 1, 256));
	obs_property_t *inference_threads = obs_properties_add_int(
		properties, kInferenceThreads, "CPU threads for RVC inference", 1, total_threads, 1);
	obs_property_set_modified_callback(inference_threads, cpu_thread_allocation_modified);
	obs_property_t *obs_reserved_threads = obs_properties_add_int(
		properties, kObsReservedThreads, "CPU threads reserved for OBS", 0, total_threads - 1, 1);
	obs_property_set_long_description(
		obs_reserved_threads,
		"These cores are excluded from the RVC inference thread pool so OBS and other tasks have more CPU time.");
	obs_property_set_modified_callback(obs_reserved_threads, cpu_thread_allocation_modified);

	return properties;
}

void rvc_filter_update(void *raw_data, obs_data_t *settings)
{
	auto *data = static_cast<RvcFilterData *>(raw_data);
	if (data == nullptr || g_ipc == nullptr)
		return;

	std::lock_guard<std::mutex> update_lock(data->update_mutex);
	rvc_settings_request_t request{};
	rvc_settings_response_t response{};
	const char *model = obs_data_get_string(settings, rvc::filter::kModel);
	const char *f0_method = obs_data_get_string(settings, kF0Method);
	const rvc::filter::ModelValidation validation = rvc::filter::validate_models(settings);
	if (!validation.valid) {
		std::lock_guard<std::mutex> lock(data->mutex);
		data->configured = false;
		obs_log(LOG_ERROR, "Unable to configure RVC filter: %s", validation.message.c_str());
		return;
	}

	if (!rvc::utils::copy_string(request.model, model) ||
	    !rvc::utils::copy_string(request.hubert_path, obs_data_get_string(settings, rvc::filter::kHubertPath)) ||
	    !rvc::utils::copy_string(request.rmvpe_path, obs_data_get_string(settings, rvc::filter::kRmvpePath))) {
		std::lock_guard<std::mutex> lock(data->mutex);
		data->configured = false;
		obs_log(LOG_ERROR, "Unable to configure RVC filter: model path exceeds IPC limits.");
		return;
	}

	const int32_t total_threads =
		static_cast<int32_t>(std::clamp<int64_t>(rvc::utils::physical_cpu_count(), 1, 256));
	const int32_t obs_reserved_threads = static_cast<int32_t>(
		std::clamp<int64_t>(obs_data_get_int(settings, kObsReservedThreads), 0, total_threads - 1));
	const int32_t inference_threads = static_cast<int32_t>(std::clamp<int64_t>(
		obs_data_get_int(settings, kInferenceThreads), 1, total_threads - obs_reserved_threads));
	obs_data_set_int(settings, kInferenceThreads, inference_threads);
	obs_data_set_int(settings, kObsReservedThreads, obs_reserved_threads);
	request.inference_threads = static_cast<uint32_t>(inference_threads);
	request.obs_reserved_threads = static_cast<uint32_t>(obs_reserved_threads);

	enum rvc_ipc_status status = RVC_IPC_STATUS_TRANSPORT_ERROR;
	for (uint32_t attempt = 0U; attempt < kWorkerConfigureAttempts; ++attempt) {
		response = {};
		status = rvc_ipc_configure(g_ipc, &request, &response, kWorkerStartupTimeoutMs);
		if (status == RVC_IPC_STATUS_OK)
			break;
	}

	if (status != RVC_IPC_STATUS_OK) {
		std::lock_guard<std::mutex> lock(data->mutex);
		const size_t error_size = std::min<size_t>(response.error_size, sizeof(response.error));
		if (status == RVC_IPC_STATUS_WORKER_ERROR && error_size > 0U)
			obs_log(LOG_ERROR, "Unable to configure RVC worker: %.*s", static_cast<int>(error_size),
				response.error);
		else
			obs_log(LOG_ERROR, "Unable to configure RVC worker after %u attempts: status=%d",
				kWorkerConfigureAttempts, status);
		data->configured = false;
		return;
	}

	std::lock_guard<std::mutex> lock(data->mutex);
	data->inference_threads = inference_threads;
	data->obs_reserved_threads = obs_reserved_threads;
	data->model = model != nullptr ? model : "";
	data->f0_method = f0_method != nullptr ? f0_method : "rmvpe";
	data->speaker = static_cast<int32_t>(obs_data_get_int(settings, kSpeaker));
	data->f0_up_key = static_cast<int32_t>(obs_data_get_int(settings, kF0UpKey));
	data->filter_radius = static_cast<int32_t>(obs_data_get_int(settings, kFilterRadius));
	data->resample_sr = static_cast<int32_t>(obs_data_get_int(settings, kResampleSr));
	data->rms_mix_rate = static_cast<float>(obs_data_get_double(settings, kRmsMixRate));
	data->protect = static_cast<float>(obs_data_get_double(settings, kProtect));
	data->index_rate = static_cast<float>(obs_data_get_double(settings, kIndexRate));
	const int64_t saved_initial_duration = obs_data_has_user_value(settings, kInitialChunkDurationMs)
						       ? obs_data_get_int(settings, kInitialChunkDurationMs)
						       : obs_data_get_int(settings, kChunkDurationMs);
	data->initial_chunk_duration_ms =
		saved_initial_duration > 2000
			? 500
			: static_cast<int32_t>(std::clamp<int64_t>(saved_initial_duration, 500, 2000));
	const int64_t saved_maximum_duration = obs_data_get_int(settings, kMaximumChunkDurationMs);
	data->maximum_chunk_duration_ms = static_cast<int32_t>(
		std::clamp<int64_t>(saved_maximum_duration, data->initial_chunk_duration_ms, 2000));
	obs_data_set_int(settings, kInitialChunkDurationMs, data->initial_chunk_duration_ms);
	obs_data_set_int(settings, kMaximumChunkDurationMs, data->maximum_chunk_duration_ms);
	data->configured = true;
	if (data->conversion_worker) {
		data->conversion_worker->reset({data->model, data->f0_method, data->speaker, data->f0_up_key,
						data->filter_radius, data->resample_sr, data->rms_mix_rate,
						data->protect, data->index_rate, data->initial_chunk_duration_ms,
						data->maximum_chunk_duration_ms});
	}
	obs_log(LOG_INFO, "RVC filter configured; conversion starts at %d ms and can grow to %d ms.",
		data->initial_chunk_duration_ms, data->maximum_chunk_duration_ms);
	obs_log(LOG_INFO, "RVC worker configured to use %d CPU threads; %d reserved for OBS.", data->inference_threads,
		data->obs_reserved_threads);
}

void *rvc_filter_create(obs_data_t *settings, obs_source_t *)
{
	rvc_filter_defaults(settings);
	auto *data = new RvcFilterData{};
	data->conversion_worker = std::make_unique<rvc::filter::ConversionWorker>(g_ipc);
	rvc_filter_update(data, settings);
	return data;
}

void rvc_filter_destroy(void *raw_data)
{
	delete static_cast<RvcFilterData *>(raw_data);
}

struct obs_audio_data *rvc_filter_audio(void *raw_data, struct obs_audio_data *audio)
{
	auto *data = static_cast<RvcFilterData *>(raw_data);
	if (data == nullptr || audio == nullptr || g_ipc == nullptr)
		return audio;

	std::lock_guard<std::mutex> lock(data->mutex);
	if (!data->configured || !data->conversion_worker)
		return audio;

	obs_audio_info audio_info{};
	if (!obs_get_audio_info(&audio_info))
		return audio;

	const uint16_t channels = static_cast<uint16_t>(get_audio_channels(audio_info.speakers));
	if (channels == 0U || channels > MAX_AV_PLANES)
		return audio;
	for (uint16_t channel = 0U; channel < channels; ++channel) {
		if (audio->data[channel] == nullptr)
			return audio;
	}

	data->conversion_worker->submit(*audio, audio_info.samples_per_sec, channels);
	if (!data->conversion_worker->receive(*audio, channels))
		for (uint16_t channel = 0U; channel < channels; ++channel)
			std::memset(audio->data[channel], 0, static_cast<size_t>(audio->frames) * sizeof(float));

	return audio;
}

struct obs_source_info rvc_filter_info{};
}

extern "C" void rvc_filter_register(rvc_ipc_t *ipc)
{
	g_ipc = ipc;
	rvc_filter_info.id = "rvc_audio_filter";
	rvc_filter_info.type = OBS_SOURCE_TYPE_FILTER;
	rvc_filter_info.output_flags = OBS_SOURCE_AUDIO;
	rvc_filter_info.get_name = rvc_filter_name;
	rvc_filter_info.create = rvc_filter_create;
	rvc_filter_info.destroy = rvc_filter_destroy;
	rvc_filter_info.get_defaults = rvc_filter_defaults;
	rvc_filter_info.get_properties = rvc_filter_properties;
	rvc_filter_info.update = rvc_filter_update;
	rvc_filter_info.filter_audio = rvc_filter_audio;
	obs_register_source(&rvc_filter_info);
}

extern "C" void rvc_filter_shutdown(void)
{
	rvc::filter::ConversionWorker::stop_all();
	g_ipc = nullptr;
}
