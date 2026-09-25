#pragma once

#include "rvc/ipc/audio/rvc-audio-transport.h"

#include <obs.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace rvc::filter {
namespace {

template<typename T> void write_le(std::vector<uint8_t> &buffer, size_t offset, T value)
{
	for (size_t byte = 0; byte < sizeof(T); ++byte)
		buffer[offset + byte] = static_cast<uint8_t>(value >> (byte * 8U));
}

template<typename T> T read_le(const uint8_t *data)
{
	T value = 0U;
	for (size_t byte = 0; byte < sizeof(T); ++byte)
		value |= static_cast<T>(static_cast<T>(data[byte]) << (byte * 8U));
	return value;
}

} // namespace

inline bool encode_wav(const obs_audio_data *audio, uint32_t sample_rate, uint16_t channels, std::vector<uint8_t> &wav)
{
	if (audio == nullptr || audio->frames == 0U || sample_rate == 0U || channels == 0U || channels > MAX_AV_PLANES)
		return false;
	for (uint16_t channel = 0U; channel < channels; ++channel) {
		if (audio->data[channel] == nullptr)
			return false;
	}

	const uint64_t data_size = static_cast<uint64_t>(audio->frames) * channels * sizeof(int16_t);
	const uint64_t byte_rate = static_cast<uint64_t>(sample_rate) * channels * sizeof(int16_t);
	if (data_size + 44U > RVC_AUDIO_MAX_INPUT_BYTES || byte_rate > UINT32_MAX)
		return false;

	wav.assign(static_cast<size_t>(44U + data_size), 0U);
	std::memcpy(wav.data(), "RIFF", 4U);
	write_le(wav, 4U, static_cast<uint32_t>(36U + data_size));
	std::memcpy(wav.data() + 8U, "WAVEfmt ", 8U);
	write_le(wav, 16U, uint32_t{16U});
	write_le(wav, 20U, uint16_t{1U});
	write_le(wav, 22U, channels);
	write_le(wav, 24U, sample_rate);
	write_le(wav, 28U, static_cast<uint32_t>(byte_rate));
	write_le(wav, 32U, static_cast<uint16_t>(channels * sizeof(int16_t)));
	write_le(wav, 34U, uint16_t{16U});
	std::memcpy(wav.data() + 36U, "data", 4U);
	write_le(wav, 40U, static_cast<uint32_t>(data_size));

	auto *samples = wav.data() + 44U;
	for (uint32_t frame = 0U; frame < audio->frames; ++frame) {
		for (uint16_t channel = 0U; channel < channels; ++channel) {
			const auto *input = reinterpret_cast<const float *>(audio->data[channel]);
			const float value = std::isfinite(input[frame]) ? std::clamp(input[frame], -1.0F, 1.0F) : 0.0F;
			const int16_t sample = static_cast<int16_t>(std::lrint(value * 32767.0F));
			const size_t offset = (static_cast<size_t>(frame) * channels + channel) * sizeof(int16_t);
			write_le(wav, 44U + offset, static_cast<uint16_t>(sample));
		}
	}
	return true;
}

inline bool decode_wav(const uint8_t *data, uint32_t size, uint32_t &sample_rate, uint16_t &channels,
		       std::vector<int16_t> &samples)
{
	sample_rate = 0U;
	channels = 0U;
	samples.clear();
	if (data == nullptr || size < 44U || std::memcmp(data, "RIFF", 4U) != 0 ||
	    std::memcmp(data + 8U, "WAVE", 4U) != 0)
		return false;

	const uint32_t riff_size = read_le<uint32_t>(data + 4U);
	if (riff_size < 4U || riff_size > size - 8U)
		return false;
	const uint32_t riff_end = riff_size + 8U;
	uint16_t format = 0U;
	uint16_t bits_per_sample = 0U;
	uint16_t block_align = 0U;
	const uint8_t *sample_data = nullptr;
	uint32_t sample_data_size = 0U;
	uint32_t offset = 12U;
	while (riff_end - offset >= 8U) {
		const uint8_t *chunk = data + offset;
		const uint32_t chunk_size = read_le<uint32_t>(chunk + 4U);
		offset += 8U;
		if (chunk_size > riff_end - offset)
			return false;

		if (std::memcmp(chunk, "fmt ", 4U) == 0 && chunk_size >= 16U) {
			format = read_le<uint16_t>(data + offset);
			channels = read_le<uint16_t>(data + offset + 2U);
			sample_rate = read_le<uint32_t>(data + offset + 4U);
			block_align = read_le<uint16_t>(data + offset + 12U);
			bits_per_sample = read_le<uint16_t>(data + offset + 14U);
		} else if (std::memcmp(chunk, "data", 4U) == 0) {
			sample_data = data + offset;
			sample_data_size = chunk_size;
		}

		const uint32_t padded_chunk_size = chunk_size + (chunk_size & 1U);
		if (padded_chunk_size > riff_end - offset)
			return false;
		offset += padded_chunk_size;
	}

	if (format != 1U || channels == 0U || sample_rate == 0U || bits_per_sample != 16U ||
	    block_align != channels * sizeof(int16_t) || sample_data == nullptr || sample_data_size % block_align != 0U)
		return false;

	samples.resize(sample_data_size / sizeof(int16_t));
	for (size_t index = 0U; index < samples.size(); ++index)
		samples[index] = static_cast<int16_t>(read_le<uint16_t>(sample_data + index * sizeof(int16_t)));
	return true;
}

} // namespace rvc::filter
