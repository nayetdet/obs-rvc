#include "rvc-filter.h"
#include "rvc-filter-conversion.hpp"
#include "rvc-filter-validation.hpp"
#include "utils/file-utils.hpp"
#include "utils/string-utils.hpp"
#include "utils/wav-utils.hpp"

#include <obs-module.h>
#include <obs.h>
#include <obs-data.h>
#include <obs-properties.h>
#include <graphics/graphics.h>
#include <plugin-support.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <system_error>
#include <set>
#include <string>
#include <thread>

#if defined(__linux__)
#include <sched.h>
#endif

namespace {
constexpr char kSpeaker[] = "speaker";
constexpr char kF0UpKey[] = "f0_up_key";
constexpr char kF0Method[] = "f0_method";
constexpr char kFilterRadius[] = "filter_radius";
constexpr char kResampleSr[] = "resample_sr";
constexpr char kRmsMixRate[] = "rms_mix_rate";
constexpr char kProtect[] = "protect";
constexpr char kChunkDurationMs[] = "chunk_duration_ms";
constexpr char kInitialChunkDurationMs[] = "initial_chunk_duration_ms";
constexpr char kMaximumChunkDurationMs[] = "maximum_chunk_duration_ms";
constexpr char kInferenceThreads[] = "inference_threads";
constexpr char kRenderingWarning[] = "rendering_warning";
constexpr uint32_t kWorkerStartupTimeoutMs = 30000U;
constexpr uint32_t kWorkerConfigureAttempts = 2U;
rvc_ipc_t *g_ipc = nullptr;

bool is_software_renderer(std::string device_name)
{
	std::transform(device_name.begin(), device_name.end(), device_name.begin(),
		       [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
	return device_name.find("llvmpipe") != std::string::npos || device_name.find("softpipe") != std::string::npos ||
	       device_name.find("swiftshader") != std::string::npos ||
	       device_name.find("basic render driver") != std::string::npos ||
	       device_name.find("microsoft warp") != std::string::npos;
}

std::string software_rendering_warning()
{
	obs_enter_graphics();
	const char *device_name = gs_get_device_name();
	const std::string device = device_name != nullptr ? device_name : "";
	obs_leave_graphics();

	if (!is_software_renderer(device))
		return {};

	return "OBS is rendering on the CPU (" + device +
	       "). This competes with RVC inference and can cause audio underflows. Enable GPU rendering.";
}

int64_t default_inference_threads()
{
#if defined(__linux__)
	std::set<std::pair<std::string, std::string>> physical_cores;
	cpu_set_t affinity;
	CPU_ZERO(&affinity);
	const bool has_affinity = sched_getaffinity(0, sizeof(affinity), &affinity) == 0;
	const std::filesystem::path cpu_root("/sys/devices/system/cpu");
	std::error_code error;
	for (auto entry = std::filesystem::directory_iterator(cpu_root, error);
	     !error && entry != std::filesystem::directory_iterator(); entry.increment(error)) {
		const std::string name = entry->path().filename().string();
		if (name.rfind("cpu", 0) != 0 || name.size() <= 3 ||
		    !std::all_of(name.begin() + 3, name.end(), [](unsigned char value) { return std::isdigit(value); }))
			continue;

		const unsigned long cpu = std::stoul(name.substr(3));
		if (has_affinity && (cpu >= CPU_SETSIZE || !CPU_ISSET(static_cast<int>(cpu), &affinity)))
			continue;

		std::ifstream package_file(entry->path() / "topology/physical_package_id");
		std::ifstream core_file(entry->path() / "topology/core_id");
		std::string package, core;
		if (package_file >> package && core_file >> core)
			physical_cores.emplace(package, core);
	}

	if (!physical_cores.empty())
		return std::max<int64_t>(1, static_cast<int64_t>(physical_cores.size()) -
						    (physical_cores.size() >= 4U ? 2 : 0));
#endif
	const uint32_t detected = std::thread::hardware_concurrency();
	const int64_t cores = std::clamp<int64_t>(detected == 0U ? 1U : detected, 1, 256);
	return cores >= 4 ? cores - 2 : cores;
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
	obs_data_set_default_string(settings, kF0Method, "pm");
	obs_data_set_default_int(settings, kSpeaker, 0);
	obs_data_set_default_int(settings, kF0UpKey, 0);
	obs_data_set_default_int(settings, kFilterRadius, 3);
	obs_data_set_default_int(settings, kResampleSr, 0);
	obs_data_set_default_double(settings, kRmsMixRate, 1.0);
	obs_data_set_default_double(settings, kProtect, 0.33);
	obs_data_set_default_int(settings, kChunkDurationMs, 500);
	obs_data_set_default_int(settings, kInitialChunkDurationMs, 500);
	obs_data_set_default_int(settings, kMaximumChunkDurationMs, 2000);
	obs_data_set_default_int(settings, kInferenceThreads, default_inference_threads());
}

obs_properties_t *rvc_filter_properties(void *)
{
	obs_properties_t *properties = obs_properties_create();
	const std::string rendering_warning = software_rendering_warning();
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
	obs_properties_add_int(properties, kF0UpKey, "F0 transpose", -24, 24, 1);
	obs_properties_add_int(properties, kFilterRadius, "Filter radius", 0, 7, 1);
	obs_properties_add_int(properties, kResampleSr, "Resample rate", 0, 192000, 1000);
	obs_properties_add_float_slider(properties, kRmsMixRate, "RMS mix rate", 0.0, 1.0, 0.01);
	obs_properties_add_float_slider(properties, kProtect, "Protect", 0.0, 0.5, 0.01);
	obs_properties_add_int(properties, kInitialChunkDurationMs, "Initial conversion block (ms)", 500, 2000, 10);
	obs_properties_add_int(properties, kMaximumChunkDurationMs, "Maximum conversion block (ms)", 500, 2000, 10);
	obs_properties_add_int(properties, kInferenceThreads, "CPU cores for inference (two reserved for OBS)", 1, 256,
			       1);

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

	const int32_t inference_threads =
		static_cast<int32_t>(std::clamp<int64_t>(obs_data_get_int(settings, kInferenceThreads), 1, 256));
	request.inference_threads = static_cast<uint32_t>(inference_threads);

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
	data->model = model != nullptr ? model : "";
	data->f0_method = f0_method != nullptr ? f0_method : "rmvpe";
	data->speaker = static_cast<int32_t>(obs_data_get_int(settings, kSpeaker));
	data->f0_up_key = static_cast<int32_t>(obs_data_get_int(settings, kF0UpKey));
	data->filter_radius = static_cast<int32_t>(obs_data_get_int(settings, kFilterRadius));
	data->resample_sr = static_cast<int32_t>(obs_data_get_int(settings, kResampleSr));
	data->rms_mix_rate = static_cast<float>(obs_data_get_double(settings, kRmsMixRate));
	data->protect = static_cast<float>(obs_data_get_double(settings, kProtect));
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
						data->protect, data->initial_chunk_duration_ms,
						data->maximum_chunk_duration_ms});
	}
	obs_log(LOG_INFO, "RVC filter configured; conversion starts at %d ms and can grow to %d ms.",
		data->initial_chunk_duration_ms, data->maximum_chunk_duration_ms);
	obs_log(LOG_INFO, "RVC worker configured to use %d CPU threads.", data->inference_threads);
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
