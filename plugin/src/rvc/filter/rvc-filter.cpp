#include "rvc-filter.h"

#include <obs-module.h>
#include <obs.h>
#include <obs-data.h>
#include <obs-properties.h>
#include <plugin-support.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace
{
constexpr char kModel[] = "model";
constexpr char kModelDirectory[] = "model_dir";
constexpr char kHubertPath[] = "hubert_path";
constexpr char kRmvpeRoot[] = "rmvpe_root";
constexpr char kSpeaker[] = "speaker";
constexpr char kF0UpKey[] = "f0_up_key";
constexpr char kF0Method[] = "f0_method";
constexpr char kIndexFile[] = "index_file";
constexpr char kIndexRate[] = "index_rate";
constexpr char kFilterRadius[] = "filter_radius";
constexpr char kResampleSr[] = "resample_sr";
constexpr char kRmsMixRate[] = "rms_mix_rate";
constexpr char kProtect[] = "protect";

rvc_ipc_t *g_ipc = nullptr;

struct RvcFilterData
{
	std::mutex mutex;
	std::string model;
	std::string index_file;
	std::string f0_method;
	int32_t speaker = 0;
	int32_t f0_up_key = 0;
	float index_rate = 0.0F;
	int32_t filter_radius = 3;
	int32_t resample_sr = 0;
	float rms_mix_rate = 1.0F;
	float protect = 0.33F;
	bool configured = false;
};

template<size_t Size>
void copy_string(char (&destination)[Size], const char *source)
{
	std::memset(destination, 0, Size);
	if (source != nullptr)
		std::strncpy(destination, source, Size - 1U);
}

void write_u16(std::vector<uint8_t> &buffer, size_t offset, uint16_t value)
{
	buffer[offset] = static_cast<uint8_t>(value & 0xFFU);
	buffer[offset + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

void write_u32(std::vector<uint8_t> &buffer, size_t offset, uint32_t value)
{
	buffer[offset] = static_cast<uint8_t>(value & 0xFFU);
	buffer[offset + 1U] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
	buffer[offset + 2U] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
	buffer[offset + 3U] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

uint16_t read_u16(const uint8_t *data)
{
	return static_cast<uint16_t>(data[0]) | static_cast<uint16_t>(data[1] << 8U);
}

uint32_t read_u32(const uint8_t *data)
{
	return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8U) |
	       (static_cast<uint32_t>(data[2]) << 16U) | (static_cast<uint32_t>(data[3]) << 24U);
}

bool encode_wav(const obs_audio_data *audio, uint32_t sample_rate, uint16_t channels, std::vector<uint8_t> &wav)
{
	if (audio == nullptr || audio->frames == 0U || channels == 0U || channels > MAX_AV_PLANES)
		return false;

	for (uint16_t channel = 0U; channel < channels; ++channel)
	{
		if (audio->data[channel] == nullptr)
			return false;
	}

	const uint64_t sample_count = static_cast<uint64_t>(audio->frames) * channels;
	const uint64_t data_size = sample_count * sizeof(int16_t);
	if (data_size > UINT32_MAX - 44U)
		return false;

	wav.assign(static_cast<size_t>(44U + data_size), 0U);
	std::memcpy(wav.data(), "RIFF", 4U);
	write_u32(wav, 4U, static_cast<uint32_t>(36U + data_size));
	std::memcpy(wav.data() + 8U, "WAVEfmt ", 8U);
	write_u32(wav, 16U, 16U);
	write_u16(wav, 20U, 1U);
	write_u16(wav, 22U, channels);
	write_u32(wav, 24U, sample_rate);
	write_u32(wav, 28U, sample_rate * channels * sizeof(int16_t));
	write_u16(wav, 32U, static_cast<uint16_t>(channels * sizeof(int16_t)));
	write_u16(wav, 34U, 16U);
	std::memcpy(wav.data() + 36U, "data", 4U);
	write_u32(wav, 40U, static_cast<uint32_t>(data_size));

	auto *samples = wav.data() + 44U;
	for (uint32_t frame = 0U; frame < audio->frames; ++frame)
	{
		for (uint16_t channel = 0U; channel < channels; ++channel)
		{
			const auto *input = reinterpret_cast<const float *>(audio->data[channel]);
			const float value = std::max(-1.0F, std::min(1.0F, input[frame]));
			const int16_t sample = static_cast<int16_t>(std::lrint(value * 32767.0F));
			const size_t offset = (static_cast<size_t>(frame) * channels + channel) * sizeof(int16_t);
			samples[offset] = static_cast<uint8_t>(sample & 0xFF);
			samples[offset + 1U] = static_cast<uint8_t>((sample >> 8) & 0xFF);
		}
	}
	return true;
}

bool decode_wav(const uint8_t *data, uint32_t size, uint32_t &sample_rate, uint16_t &channels,
		       std::vector<int16_t> &samples)
{
	if (data == nullptr || size < 44U || std::memcmp(data, "RIFF", 4U) != 0 || std::memcmp(data + 8U, "WAVE", 4U) != 0)
		return false;

	uint16_t format = 0U;
	uint16_t bits_per_sample = 0U;
	const uint8_t *sample_data = nullptr;
	uint32_t sample_data_size = 0U;
	uint32_t offset = 12U;
	while (offset + 8U <= size)
	{
		const uint8_t *chunk = data + offset;
		const uint32_t chunk_size = read_u32(chunk + 4U);
		offset += 8U;
		if (chunk_size > size - offset)
			return false;

		if (std::memcmp(chunk, "fmt ", 4U) == 0 && chunk_size >= 16U)
		{
			format = read_u16(data + offset);
			channels = read_u16(data + offset + 2U);
			sample_rate = read_u32(data + offset + 4U);
			bits_per_sample = read_u16(data + offset + 14U);
		}
		else if (std::memcmp(chunk, "data", 4U) == 0)
		{
			sample_data = data + offset;
			sample_data_size = chunk_size;
		}

		offset += chunk_size + (chunk_size & 1U);
	}

	if (format != 1U || channels == 0U || bits_per_sample != 16U || sample_data == nullptr ||
	    sample_data_size % sizeof(int16_t) != 0U)
		return false;

	samples.resize(sample_data_size / sizeof(int16_t));
	for (size_t index = 0U; index < samples.size(); ++index)
		samples[index] = static_cast<int16_t>(read_u16(sample_data + index * sizeof(int16_t)));
	return true;
}

const char *rvc_filter_name(void *)
{
	return "RVC Audio Filter";
}

void rvc_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, kF0Method, "rmvpe");
	obs_data_set_default_int(settings, kSpeaker, 0);
	obs_data_set_default_int(settings, kF0UpKey, 0);
	obs_data_set_default_double(settings, kIndexRate, 0.0);
	obs_data_set_default_int(settings, kFilterRadius, 3);
	obs_data_set_default_int(settings, kResampleSr, 0);
	obs_data_set_default_double(settings, kRmsMixRate, 1.0);
	obs_data_set_default_double(settings, kProtect, 0.33);
}

obs_properties_t *rvc_filter_properties(void *)
{
	obs_properties_t *properties = obs_properties_create();
	obs_properties_add_path(properties, kModel, "Model", OBS_PATH_FILE, "RVC model (*.pth)", nullptr);
	obs_properties_add_path(properties, kModelDirectory, "Model directory", OBS_PATH_DIRECTORY, nullptr, nullptr);
	obs_properties_add_path(properties, kHubertPath, "HuBERT model", OBS_PATH_FILE, "HuBERT model (*.pt)", nullptr);
	obs_properties_add_path(properties, kRmvpeRoot, "RMVPE directory", OBS_PATH_DIRECTORY, nullptr, nullptr);

	obs_property_t *f0_method = obs_properties_add_list(properties, kF0Method, "F0 method", OBS_COMBO_TYPE_LIST,
									OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(f0_method, "RMVPE", "rmvpe");
	obs_property_list_add_string(f0_method, "Harvest", "harvest");
	obs_property_list_add_string(f0_method, "Crepe", "crepe");
	obs_property_list_add_string(f0_method, "PM", "pm");

	obs_properties_add_path(properties, kIndexFile, "Index file", OBS_PATH_FILE, "RVC index (*.index)", nullptr);
	obs_properties_add_int(properties, kSpeaker, "Speaker", 0, 32, 1);
	obs_properties_add_int(properties, kF0UpKey, "F0 transpose", -24, 24, 1);
	obs_properties_add_float_slider(properties, kIndexRate, "Index rate", 0.0, 1.0, 0.01);
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
	const char *model = obs_data_get_string(settings, kModel);
	const char *index_file = obs_data_get_string(settings, kIndexFile);
	const char *f0_method = obs_data_get_string(settings, kF0Method);
	copy_string(request.model, model);
	copy_string(request.model_dir, obs_data_get_string(settings, kModelDirectory));
	copy_string(request.hubert_path, obs_data_get_string(settings, kHubertPath));
	copy_string(request.rmvpe_root, obs_data_get_string(settings, kRmvpeRoot));

	const enum rvc_ipc_status status = rvc_ipc_configure(g_ipc, &request, &response, 1000U);
	if (status != RVC_IPC_STATUS_OK)
	{
		obs_log(LOG_ERROR, "Unable to configure RVC worker: status=%d", status);
		data->configured = false;
		return;
	}

	data->model = model != nullptr ? model : "";
	data->index_file = index_file != nullptr ? index_file : "";
	data->f0_method = f0_method != nullptr ? f0_method : "rmvpe";
	data->speaker = static_cast<int32_t>(obs_data_get_int(settings, kSpeaker));
	data->f0_up_key = static_cast<int32_t>(obs_data_get_int(settings, kF0UpKey));
	data->index_rate = static_cast<float>(obs_data_get_double(settings, kIndexRate));
	data->filter_radius = static_cast<int32_t>(obs_data_get_int(settings, kFilterRadius));
	data->resample_sr = static_cast<int32_t>(obs_data_get_int(settings, kResampleSr));
	data->rms_mix_rate = static_cast<float>(obs_data_get_double(settings, kRmsMixRate));
	data->protect = static_cast<float>(obs_data_get_double(settings, kProtect));
	data->configured = true;
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
	if (!encode_wav(audio, audio_info.samples_per_sec, channels, input_wav))
		return audio;

	auto request = std::make_unique<rvc_audio_request_t>();
	auto response = std::make_unique<rvc_audio_response_t>();
	request->audio_size = static_cast<uint32_t>(input_wav.size());
	copy_string(request->model, data->model.c_str());
	copy_string(request->input_format, "wav");
	copy_string(request->f0_method, data->f0_method.c_str());
	copy_string(request->index_file, data->index_file.c_str());
	request->speaker = data->speaker;
	request->f0_up_key = data->f0_up_key;
	request->index_rate = data->index_rate;
	request->filter_radius = data->filter_radius;
	request->resample_sr = data->resample_sr;
	request->rms_mix_rate = data->rms_mix_rate;
	request->protect = data->protect;
	std::memcpy(request->audio, input_wav.data(), input_wav.size());

	const enum rvc_ipc_status status = rvc_ipc_convert(g_ipc, request.get(), response.get(), 1000U);
	if (status != RVC_IPC_STATUS_OK || response->audio_size == 0U)
		return audio;

	uint32_t output_sample_rate = 0U;
	uint16_t output_channels = 0U;
	std::vector<int16_t> output_samples;
	if (!decode_wav(response->audio, response->audio_size, output_sample_rate, output_channels, output_samples))
		return audio;

	const size_t output_frames = output_samples.size() / output_channels;
	if (output_sample_rate != audio_info.samples_per_sec || output_frames != audio->frames || output_channels == 0U ||
	    output_channels > MAX_AV_PLANES)
		return audio;

	for (uint16_t channel = 0U; channel < channels; ++channel)
	{
		auto *output = reinterpret_cast<float *>(audio->data[channel]);
		if (output == nullptr)
			return audio;
		const uint16_t source_channel = std::min<uint16_t>(channel, output_channels - 1U);
		for (uint32_t frame = 0U; frame < audio->frames; ++frame)
		{
			const int16_t sample = output_samples[static_cast<size_t>(frame) * output_channels + source_channel];
			output[frame] = static_cast<float>(sample) / 32768.0F;
		}
	}

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
	g_ipc = nullptr;
}
