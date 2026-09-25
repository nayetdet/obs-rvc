#include "rvc-filter-conversion.hpp"

#include "utils/string-utils.hpp"
#include "utils/wav-utils.hpp"

#include <obs-module.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace rvc::filter {
namespace {
constexpr uint32_t kMaximumQueuedDurationMs = 30000U;
constexpr uint32_t kWarmupConversionTimeoutMs = 180000U;
constexpr uint32_t kConversionTimeoutMs = 60000U;

std::mutex workers_mutex;
std::set<ConversionWorker *> workers;
bool workers_stopping = false;

size_t frame_count_for_duration(uint32_t sample_rate, uint32_t duration_ms)
{
	return static_cast<size_t>(sample_rate) * duration_ms / 1000U;
}

size_t maximum_request_frames(uint16_t channels)
{
	if (channels == 0U)
		return 0U;
	return (RVC_AUDIO_MAX_INPUT_BYTES - 44U) / (static_cast<size_t>(channels) * sizeof(int16_t));
}

size_t conversion_frame_count(uint32_t sample_rate, int32_t duration_ms, uint16_t channels)
{
	if (duration_ms <= 0)
		return 0U;
	return std::min(frame_count_for_duration(sample_rate, static_cast<uint32_t>(duration_ms)),
			maximum_request_frames(channels));
}

bool copy_audio_to_interleaved(const obs_audio_data &audio, uint16_t channels, std::vector<float> &samples)
{
	if (channels == 0U || channels > MAX_AV_PLANES)
		return false;

	samples.resize(static_cast<size_t>(audio.frames) * channels);
	for (uint32_t frame = 0U; frame < audio.frames; ++frame) {
		for (uint16_t channel = 0U; channel < channels; ++channel) {
			if (audio.data[channel] == nullptr)
				return false;
			const float value = reinterpret_cast<const float *>(audio.data[channel])[frame];
			samples[static_cast<size_t>(frame) * channels + channel] = std::isfinite(value) ? value : 0.0F;
		}
	}
	return true;
}

void make_planar_audio(const std::vector<float> &interleaved, uint16_t channels,
		       std::array<std::vector<float>, MAX_AV_PLANES> &planes, obs_audio_data &audio)
{
	const uint32_t frames = static_cast<uint32_t>(interleaved.size() / channels);
	for (uint16_t channel = 0U; channel < channels; ++channel) {
		planes[channel].resize(frames);
		for (uint32_t frame = 0U; frame < frames; ++frame)
			planes[channel][frame] = interleaved[static_cast<size_t>(frame) * channels + channel];
		audio.data[channel] = reinterpret_cast<uint8_t *>(planes[channel].data());
	}
	audio.frames = frames;
}

bool decode_to_input_format(const rvc_audio_response_t &response, uint32_t input_sample_rate, uint16_t input_channels,
			    uint32_t input_frames, std::vector<float> &output)
{
	uint32_t output_sample_rate = 0U;
	uint16_t output_channels = 0U;
	std::vector<int16_t> decoded;
	if (response.audio_size > RVC_AUDIO_MAX_OUTPUT_BYTES ||
	    !decode_wav(response.audio, response.audio_size, output_sample_rate, output_channels, decoded) ||
	    output_sample_rate == 0U || output_channels == 0U || decoded.empty())
		return false;

	const size_t decoded_frames = decoded.size() / output_channels;
	if (decoded_frames == 0U)
		return false;

	output.resize(static_cast<size_t>(input_frames) * input_channels);
	for (uint32_t frame = 0U; frame < input_frames; ++frame) {
		const double position = static_cast<double>(frame) * output_sample_rate / input_sample_rate;
		const size_t first = std::min(static_cast<size_t>(position), decoded_frames - 1U);
		const size_t second = std::min(first + 1U, decoded_frames - 1U);
		const float fraction = static_cast<float>(position - first);
		for (uint16_t channel = 0U; channel < input_channels; ++channel) {
			const uint16_t source_channel = std::min<uint16_t>(channel, output_channels - 1U);
			const float a =
				static_cast<float>(decoded[first * output_channels + source_channel]) / 32768.0F;
			const float b =
				static_cast<float>(decoded[second * output_channels + source_channel]) / 32768.0F;
			output[static_cast<size_t>(frame) * input_channels + channel] = a + (b - a) * fraction;
		}
	}
	return true;
}
} // namespace

struct ConversionWorker::Impl {
	explicit Impl(rvc_ipc_t *ipc_context) : ipc(ipc_context), thread(&Impl::run, this) {}
	~Impl() { stop(); }

	rvc_ipc_t *ipc;
	std::mutex mutex;
	std::condition_variable work_ready;
	ConversionOptions options{};
	std::deque<float> input;
	std::deque<float> output;
	uint32_t sample_rate = 0U;
	uint16_t channels = 0U;
	uint64_t generation = 0U;
	bool configured = false;
	bool has_completed_conversion = false;
	bool stopping = false;
	std::thread thread;

	void stop()
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			stopping = true;
		}
		work_ready.notify_one();
		if (thread.joinable())
			thread.join();
	}

	void run();
};

ConversionWorker::ConversionWorker(rvc_ipc_t *ipc) : impl(std::make_unique<Impl>(ipc))
{
	std::lock_guard<std::mutex> lock(workers_mutex);
	workers.insert(this);
	if (workers_stopping)
		impl->stop();
}

ConversionWorker::~ConversionWorker()
{
	std::lock_guard<std::mutex> lock(workers_mutex);
	impl->stop();
	workers.erase(this);
}

void ConversionWorker::stop()
{
	impl->stop();
}

void ConversionWorker::stop_all()
{
	std::lock_guard<std::mutex> lock(workers_mutex);
	workers_stopping = true;
	for (ConversionWorker *worker : workers)
		worker->stop();
}

void ConversionWorker::reset(const ConversionOptions &options)
{
	std::lock_guard<std::mutex> lock(impl->mutex);
	if (impl->stopping)
		return;
	impl->options = options;
	impl->input.clear();
	impl->output.clear();
	++impl->generation;
	impl->configured = true;
	impl->has_completed_conversion = false;
	impl->work_ready.notify_one();
}

void ConversionWorker::submit(const obs_audio_data &audio, uint32_t sample_rate, uint16_t channels)
{
	std::vector<float> samples;
	if (sample_rate == 0U || !copy_audio_to_interleaved(audio, channels, samples))
		return;

	std::lock_guard<std::mutex> lock(impl->mutex);
	if (!impl->configured || impl->stopping)
		return;
	if (impl->sample_rate != sample_rate || impl->channels != channels) {
		impl->sample_rate = sample_rate;
		impl->channels = channels;
		impl->input.clear();
		impl->output.clear();
		++impl->generation;
	}

	impl->input.insert(impl->input.end(), samples.begin(), samples.end());
	const size_t maximum_samples = frame_count_for_duration(sample_rate, kMaximumQueuedDurationMs) * channels;
	while (impl->input.size() > maximum_samples)
		impl->input.pop_front();
	impl->work_ready.notify_one();
}

bool ConversionWorker::receive(obs_audio_data &audio, uint16_t channels)
{
	std::lock_guard<std::mutex> lock(impl->mutex);
	const size_t sample_count = static_cast<size_t>(audio.frames) * channels;
	if (impl->output.size() < sample_count)
		return false;

	for (uint32_t frame = 0U; frame < audio.frames; ++frame) {
		for (uint16_t channel = 0U; channel < channels; ++channel) {
			reinterpret_cast<float *>(audio.data[channel])[frame] = impl->output.front();
			impl->output.pop_front();
		}
	}
	return true;
}

void ConversionWorker::Impl::run()
{
	for (;;) {
		std::vector<float> input_samples;
		ConversionOptions conversion_options;
		uint32_t input_sample_rate = 0U;
		uint16_t input_channels = 0U;
		uint64_t input_generation = 0U;
		bool first_conversion = false;

		{
			std::unique_lock<std::mutex> lock(mutex);
			work_ready.wait(lock, [this] {
				if (stopping || !configured || sample_rate == 0U || channels == 0U)
					return stopping;
				const size_t frames =
					conversion_frame_count(sample_rate, options.chunk_duration_ms, channels);
				return frames > 0U && input.size() >= frames * channels;
			});
			if (stopping)
				return;

			input_sample_rate = sample_rate;
			input_channels = channels;
			conversion_options = options;
			input_generation = generation;
			first_conversion = !has_completed_conversion;
			const size_t frames =
				conversion_frame_count(sample_rate, conversion_options.chunk_duration_ms, channels);
			const size_t sample_count = frames * channels;
			input_samples.assign(input.begin(), input.begin() + sample_count);
			input.erase(input.begin(), input.begin() + sample_count);
		}

		std::array<std::vector<float>, MAX_AV_PLANES> planes;
		obs_audio_data audio{};
		make_planar_audio(input_samples, input_channels, planes, audio);
		std::vector<uint8_t> wav;
		if (!encode_wav(&audio, input_sample_rate, input_channels, wav)) {
			blog(LOG_ERROR, "[obs-rvc] Unable to encode audio for conversion.");
			continue;
		}

		auto request = std::make_unique<rvc_audio_request_t>();
		auto response = std::make_unique<rvc_audio_response_t>();
		request->audio_size = static_cast<uint32_t>(wav.size());
		if (!utils::copy_string(request->model, conversion_options.model.c_str()) ||
		    !utils::copy_string(request->input_format, "wav") ||
		    !utils::copy_string(request->f0_method, conversion_options.f0_method.c_str())) {
			blog(LOG_ERROR, "[obs-rvc] Conversion settings exceed IPC limits.");
			continue;
		}

		request->speaker = conversion_options.speaker;
		request->f0_up_key = conversion_options.f0_up_key;
		request->filter_radius = conversion_options.filter_radius;
		request->resample_sr = conversion_options.resample_sr > 0 ? conversion_options.resample_sr
									  : static_cast<int32_t>(input_sample_rate);
		request->rms_mix_rate = conversion_options.rms_mix_rate;
		request->protect = conversion_options.protect;
		std::memcpy(request->audio, wav.data(), wav.size());

		const auto started_at = std::chrono::steady_clock::now();
		const uint32_t timeout_ms = first_conversion ? kWarmupConversionTimeoutMs : kConversionTimeoutMs;
		const rvc_ipc_status status = rvc_ipc_convert(ipc, request.get(), response.get(), timeout_ms);
		if (status != RVC_IPC_STATUS_OK || response->audio_size == 0U) {
			blog(LOG_ERROR, "[obs-rvc] Background conversion failed (status=%d).", status);
			continue;
		}

		const uint32_t input_frames = static_cast<uint32_t>(input_samples.size() / input_channels);
		std::vector<float> converted;
		if (!decode_to_input_format(*response, input_sample_rate, input_channels, input_frames, converted)) {
			blog(LOG_ERROR, "[obs-rvc] Worker returned invalid converted audio.");
			continue;
		}

		{
			std::lock_guard<std::mutex> lock(mutex);
			if (stopping || generation != input_generation || sample_rate != input_sample_rate ||
			    channels != input_channels)
				continue;
			output.insert(output.end(), converted.begin(), converted.end());
			has_completed_conversion = true;
		}

		const double duration = static_cast<double>(input_frames) / input_sample_rate;
		const double realtime_factor =
			std::chrono::duration<double>(std::chrono::steady_clock::now() - started_at).count() / duration;
		blog(LOG_DEBUG, "[obs-rvc] Converted %.0f ms (RTF %.2f).", duration * 1000.0, realtime_factor);
	}
}
} // namespace rvc::filter
