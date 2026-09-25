#include "rvc-filter.h"
#include "rvc-filter-validation.hpp"
#include "utils/file-utils.hpp"
#include "utils/string-utils.hpp"
#include "utils/wav-utils.hpp"

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
#include <vector>

namespace {
constexpr char kSpeaker[] = "speaker";
constexpr char kF0UpKey[] = "f0_up_key";
constexpr char kF0Method[] = "f0_method";
constexpr char kFilterRadius[] = "filter_radius";
constexpr char kResampleSr[] = "resample_sr";
constexpr char kRmsMixRate[] = "rms_mix_rate";
constexpr char kProtect[] = "protect";

rvc_ipc_t *g_ipc = nullptr;

const char *rvc_filter_name(void *)
{
	return "RVC Audio Filter";
}

void rvc_filter_defaults(obs_data_t *settings)
{
	rvc::utils::set_default_module_file(settings, rvc::filter::kModel, "models/rvc/miku_default_rvc.pth");
	rvc::utils::set_default_module_file(settings, rvc::filter::kHubertPath, "models/hubert/hubert_base.pt");
	rvc::utils::set_default_module_file(settings, rvc::filter::kRmvpePath, "models/rmvpe/rmvpe.pt");
	obs_data_set_default_string(settings, kF0Method, "rmvpe");
	obs_data_set_default_int(settings, kSpeaker, 0);
	obs_data_set_default_int(settings, kF0UpKey, 0);
	obs_data_set_default_int(settings, kFilterRadius, 3);
	obs_data_set_default_int(settings, kResampleSr, 0);
	obs_data_set_default_double(settings, kRmsMixRate, 0.25);
	obs_data_set_default_double(settings, kProtect, 0.33);
}

obs_properties_t *rvc_filter_properties(void *)
{
	obs_properties_t *properties = obs_properties_create();
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
	return properties;
}

void rvc_filter_update(void *raw_data, obs_data_t *settings)
{
	auto *data = static_cast<RvcFilterData *>(raw_data);
	if (data == nullptr || g_ipc == nullptr)
		return;

	std::lock_guard<std::mutex> lock(data->mutex);
	rvc_settings_request_t request{};
	rvc_settings_response_t response{};
	const char *model = obs_data_get_string(settings, rvc::filter::kModel);
	const char *f0_method = obs_data_get_string(settings, kF0Method);
	const rvc::filter::ModelValidation validation = rvc::filter::validate_models(settings);
	if (!validation.valid) {
		data->configured = false;
		obs_log(LOG_ERROR, "Unable to configure RVC filter: %s", validation.message.c_str());
		return;
	}

	if (!rvc::utils::copy_string(request.model, model) ||
	    !rvc::utils::copy_string(request.hubert_path, obs_data_get_string(settings, rvc::filter::kHubertPath)) ||
	    !rvc::utils::copy_string(request.rmvpe_path, obs_data_get_string(settings, rvc::filter::kRmvpePath))) {
		data->configured = false;
		obs_log(LOG_ERROR, "Unable to configure RVC filter: model path exceeds IPC limits.");
		return;
	}

	const enum rvc_ipc_status status = rvc_ipc_configure(g_ipc, &request, &response, 1000U);
	if (status != RVC_IPC_STATUS_OK) {
		obs_log(LOG_ERROR, "Unable to configure RVC worker: status=%d", status);
		data->configured = false;
		return;
	}

	data->model = model != nullptr ? model : "";
	data->f0_method = f0_method != nullptr ? f0_method : "rmvpe";
	data->speaker = static_cast<int32_t>(obs_data_get_int(settings, kSpeaker));
	data->f0_up_key = static_cast<int32_t>(obs_data_get_int(settings, kF0UpKey));
	data->filter_radius = static_cast<int32_t>(obs_data_get_int(settings, kFilterRadius));
	data->resample_sr = static_cast<int32_t>(obs_data_get_int(settings, kResampleSr));
	data->rms_mix_rate = static_cast<float>(obs_data_get_double(settings, kRmsMixRate));
	data->protect = static_cast<float>(obs_data_get_double(settings, kProtect));
	data->configured = true;
	data->conversion_error_logged = false;
}

void *rvc_filter_create(obs_data_t *settings, obs_source_t *)
{
	rvc_filter_defaults(settings);
	auto *data = new RvcFilterData{};
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
	if (!data->configured)
		return audio;

	obs_audio_info audio_info{};
	if (!obs_get_audio_info(&audio_info))
		return audio;

	const uint16_t channels = static_cast<uint16_t>(get_audio_channels(audio_info.speakers));
	std::vector<uint8_t> input_wav;
	if (!rvc::filter::encode_wav(audio, audio_info.samples_per_sec, channels, input_wav))
		return audio;

	auto request = std::make_unique<rvc_audio_request_t>();
	auto response = std::make_unique<rvc_audio_response_t>();
	request->audio_size = static_cast<uint32_t>(input_wav.size());
	if (!rvc::utils::copy_string(request->model, data->model.c_str()) ||
	    !rvc::utils::copy_string(request->input_format, "wav") ||
	    !rvc::utils::copy_string(request->f0_method, data->f0_method.c_str()))
		return audio;

	request->speaker = data->speaker;
	request->f0_up_key = data->f0_up_key;
	request->filter_radius = data->filter_radius;
	request->resample_sr = data->resample_sr;
	request->rms_mix_rate = data->rms_mix_rate;
	request->protect = data->protect;
	std::memcpy(request->audio, input_wav.data(), input_wav.size());

	const enum rvc_ipc_status status = rvc_ipc_convert(g_ipc, request.get(), response.get(), 1000U);
	if (status != RVC_IPC_STATUS_OK || response->audio_size == 0U) {
		if (!data->conversion_error_logged) {
			const size_t error_size = std::min<size_t>(response->error_size, sizeof(response->error));
			const std::string error = error_size > 0U ? std::string(response->error, error_size)
								  : "RVC conversion failed.";
			obs_log(LOG_ERROR, "%s", error.c_str());
			data->conversion_error_logged = true;
		}
		return audio;
	}

	data->conversion_error_logged = false;

	uint32_t output_sample_rate = 0U;
	uint16_t output_channels = 0U;
	std::vector<int16_t> output_samples;
	if (response->audio_size > RVC_AUDIO_MAX_OUTPUT_BYTES ||
	    !rvc::filter::decode_wav(response->audio, response->audio_size, output_sample_rate, output_channels,
				     output_samples))
		return audio;

	if (output_channels == 0U || output_channels > MAX_AV_PLANES || output_channels > channels)
		return audio;

	const size_t output_frames = output_samples.size() / output_channels;
	if (output_sample_rate != audio_info.samples_per_sec || output_frames != audio->frames)
		return audio;

	for (uint16_t channel = 0U; channel < channels; ++channel) {
		if (audio->data[channel] == nullptr)
			return audio;
	}

	for (uint16_t channel = 0U; channel < channels; ++channel) {
		auto *output = reinterpret_cast<float *>(audio->data[channel]);
		const uint16_t source_channel = std::min<uint16_t>(channel, output_channels - 1U);
		for (uint32_t frame = 0U; frame < audio->frames; ++frame) {
			const int16_t sample =
				output_samples[static_cast<size_t>(frame) * output_channels + source_channel];
			output[frame] = static_cast<float>(sample) / 32768.0F;
		}
	}

	return audio;
}

struct obs_source_info rvc_filter_info{};
} // namespace

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
	g_ipc = nullptr;
}
