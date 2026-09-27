#include "rvc-filter-conversion.hpp"
#include "rvc-filter-stream.hpp"

#include "utils/audio-utils.hpp"
#include "utils/string-utils.hpp"

#include <obs-module.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

namespace rvc::filter {
namespace {
constexpr uint32_t kWarmupConversionTimeoutMs = 180000U;
constexpr uint32_t kConversionTimeoutMs = 60000U;
constexpr size_t kInitialOutputBlocks = 3U;
constexpr size_t kOutputLowWaterBlocks = kInitialOutputBlocks - 1U;

std::mutex workers_mutex;
std::set<ConversionWorker *> workers;
bool workers_stopping = false;

size_t frame_count_for_duration(uint32_t sample_rate, uint32_t duration_ms)
{
	return static_cast<size_t>(sample_rate) * duration_ms / 1000U;
}

size_t conversion_frame_count(uint32_t sample_rate, int32_t duration_ms, uint16_t channels)
{
	if (duration_ms <= 0 || channels == 0U)
		return 0U;
	const size_t context = frame_count_for_duration(sample_rate, kStreamHistoryMs + kStreamLookaheadMs);
	const size_t capacity = (RVC_AUDIO_MAX_INPUT_BYTES - 44U) / (static_cast<size_t>(channels) * sizeof(int16_t));
	if (capacity <= context)
		return 0U;
	return std::min(frame_count_for_duration(sample_rate, static_cast<uint32_t>(duration_ms)), capacity - context);
}

bool copy_audio_to_mono(const obs_audio_data &audio, uint16_t channels, std::vector<float> &samples)
{
	if (channels == 0U || channels > MAX_AV_PLANES)
		return false;

	samples.resize(audio.frames);
	for (uint32_t frame = 0U; frame < audio.frames; ++frame) {
		double mixed = 0.0;
		for (uint16_t channel = 0U; channel < channels; ++channel) {
			if (audio.data[channel] == nullptr)
				return false;
			const float value = reinterpret_cast<const float *>(audio.data[channel])[frame];
			mixed += std::isfinite(value) ? value : 0.0F;
		}
		samples[frame] = static_cast<float>(mixed / channels);
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
	    !rvc::utils::decode_wav(response.audio, response.audio_size, output_sample_rate, output_channels,
				    decoded) ||
	    output_sample_rate == 0U || output_channels == 0U || decoded.empty())
		return false;

	const size_t decoded_frames = decoded.size() / output_channels;
	if (decoded_frames == 0U)
		return false;

	const size_t available_frames = std::min<size_t>(
		input_frames, decoded_frames * static_cast<uint64_t>(input_sample_rate) / output_sample_rate);

	output.resize(available_frames * input_channels);
	for (size_t frame = 0U; frame < available_frames; ++frame) {
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
}

struct ConversionWorker::Impl {
	explicit Impl(rvc_ipc_t *ipc_context) : ipc(ipc_context), thread(&Impl::run, this) {}
	~Impl() { stop(); }

	rvc_ipc_t *ipc;
	std::mutex mutex;
	std::condition_variable work_ready;
	ConversionOptions options{};
	AudioSampleQueue input;
	AudioSampleQueue output;
	uint32_t sample_rate = 0U;
	uint16_t channels = 0U;
	uint64_t generation = 0U;
	int32_t active_chunk_duration_ms = 500;
	int32_t startup_chunk_duration_ms = 500;
	int32_t maximum_chunk_duration_ms = 2000;
	bool configured = false;
	bool has_completed_conversion = false;
	bool stopping = false;
	bool discontinuity = true;
	bool playback_started = false;
	float playback_gain = 0.0F;
	uint64_t output_underflows = 0U;
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
	impl->active_chunk_duration_ms = options.initial_chunk_duration_ms;
	impl->startup_chunk_duration_ms = options.initial_chunk_duration_ms;
	impl->maximum_chunk_duration_ms = options.maximum_chunk_duration_ms;
	impl->input.clear();
	impl->output.clear();
	impl->playback_gain = 0.0F;
	impl->playback_started = false;
	impl->output_underflows = 0U;
	++impl->generation;
	impl->configured = true;
	impl->has_completed_conversion = false;
	impl->discontinuity = true;
	impl->work_ready.notify_one();
}

void ConversionWorker::submit(const obs_audio_data &audio, uint32_t sample_rate, uint16_t channels)
{
	thread_local std::vector<float> samples;
	if (sample_rate == 0U || !copy_audio_to_mono(audio, channels, samples))
		return;

	std::lock_guard<std::mutex> lock(impl->mutex);
	if (!impl->configured || impl->stopping)
		return;

	if (impl->sample_rate != sample_rate || impl->channels != channels) {
		impl->sample_rate = sample_rate;
		impl->channels = channels;
		impl->input.clear();
		impl->output.clear();
		impl->playback_gain = 0.0F;
		impl->playback_started = false;
		impl->output_underflows = 0U;
		++impl->generation;
		impl->discontinuity = true;
	}

	impl->input.append(samples);
	const size_t maximum_samples =
		2U * conversion_frame_count(sample_rate, impl->active_chunk_duration_ms, channels) +
		frame_count_for_duration(sample_rate, kStreamLookaheadMs);

	if (impl->input.size() > maximum_samples) {
		const size_t hop_frames = conversion_frame_count(sample_rate, impl->active_chunk_duration_ms, channels);
		const size_t excess_frames = impl->input.size() - maximum_samples;
		const size_t frames_to_discard =
			hop_frames == 0U ? excess_frames
					 : ((excess_frames + hop_frames - 1U) / hop_frames) * hop_frames;
		impl->input.discard(frames_to_discard);
		impl->discontinuity = true;
	}

	impl->work_ready.notify_one();
}

bool ConversionWorker::receive(obs_audio_data &audio, uint16_t channels)
{
	std::lock_guard<std::mutex> lock(impl->mutex);
	if (channels == 0 || channels != impl->channels)
		return false;

	if (!impl->playback_started) {
		const size_t initial_buffer_frames =
			conversion_frame_count(impl->sample_rate, impl->startup_chunk_duration_ms, channels) *
			kInitialOutputBlocks;
		if (impl->output.size() < initial_buffer_frames * channels)
			return false;
		impl->playback_started = true;
	}

	const float fade_frames = static_cast<float>(std::max<size_t>(1, impl->sample_rate / 200));
	const size_t sample_count = static_cast<size_t>(audio.frames) * channels;
	if (impl->output.size() < sample_count) {
		impl->playback_gain = 0.0F;
		++impl->output_underflows;
		return false;
	}

	for (uint32_t frame = 0U; frame < audio.frames; ++frame) {
		impl->playback_gain = std::min(1.0F, impl->playback_gain + 1.0F / fade_frames);
		for (uint16_t channel = 0U; channel < channels; ++channel) {
			const float converted = impl->output.at(static_cast<size_t>(frame) * channels + channel);
			reinterpret_cast<float *>(audio.data[channel])[frame] = converted * impl->playback_gain;
		}
	}

	impl->output.discard(sample_count);
	impl->work_ready.notify_one();
	return true;
}

void ConversionWorker::Impl::run()
{
	StreamAssembler stream;
	auto request = std::unique_ptr<rvc_audio_request_t>(new rvc_audio_request_t);
	auto response = std::unique_ptr<rvc_audio_response_t>(new rvc_audio_response_t);
	auto last_warning = std::chrono::steady_clock::time_point{};
	uint64_t reported_underflows = 0U;
	std::vector<float> input_samples;
	std::vector<float> converted;
	std::vector<float> stitched;
	std::vector<uint8_t> wav;
	std::array<std::vector<float>, MAX_AV_PLANES> planes;
	for (;;) {
		ConversionOptions conversion_options;
		uint32_t input_sample_rate = 0U;
		uint16_t input_channels = 0U;
		uint64_t input_generation = 0U;
		bool first_conversion = false;
		size_t hop_frames = 0U;
		size_t context_frames = 0U;

		{
			std::unique_lock<std::mutex> lock(mutex);
			work_ready.wait(lock, [this] {
				if (stopping || !configured || sample_rate == 0U || channels == 0U)
					return stopping;

				const size_t frames =
					conversion_frame_count(sample_rate, active_chunk_duration_ms, channels);

				return frames > 0U &&
				       input.size() >=
					       frames + frame_count_for_duration(sample_rate, kStreamLookaheadMs) &&
				       output.size() <= kOutputLowWaterBlocks * frames * channels;
			});

			if (stopping)
				return;

			input_sample_rate = sample_rate;
			input_channels = channels;
			conversion_options = options;
			conversion_options.initial_chunk_duration_ms = active_chunk_duration_ms;
			input_generation = generation;
			first_conversion = !has_completed_conversion;
			const size_t frames = conversion_frame_count(
				sample_rate, conversion_options.initial_chunk_duration_ms, channels);

			hop_frames = frames;
			context_frames = frame_count_for_duration(sample_rate, kStreamHistoryMs);
			const size_t sample_count = frames + frame_count_for_duration(sample_rate, kStreamLookaheadMs);

			input.copy_front(input_samples, sample_count);
			input.discard(frames);
			if (discontinuity) {
				stream.reset();
				discontinuity = false;
			}
		}

		input_samples = stream.window(input_samples, hop_frames, context_frames, 1U);
		obs_audio_data audio{};
		make_planar_audio(input_samples, 1U, planes, audio);
		if (!rvc::utils::encode_wav(&audio, input_sample_rate, 1U, wav)) {
			stream.reset();
			blog(LOG_ERROR, "[obs-rvc] Unable to encode audio for conversion.");
			continue;
		}

		request->audio_size = static_cast<uint32_t>(wav.size());
		if (!utils::copy_string(request->model, conversion_options.model.c_str()) ||
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
		request->index_rate = conversion_options.index_rate;
		std::memcpy(request->audio, wav.data(), wav.size());

		const auto started_at = std::chrono::steady_clock::now();
		const uint32_t timeout_ms = first_conversion ? kWarmupConversionTimeoutMs : kConversionTimeoutMs;
		const rvc_ipc_status status = rvc_ipc_convert(ipc, request.get(), response.get(), timeout_ms);
		if (status != RVC_IPC_STATUS_OK || response->audio_size == 0U) {
			stream.reset();
			blog(LOG_ERROR, "[obs-rvc] Background conversion failed (status=%d).", status);
			continue;
		}

		const uint32_t input_frames = static_cast<uint32_t>(input_samples.size());
		if (!decode_to_input_format(*response, input_sample_rate, input_channels, input_frames, converted)) {
			stream.reset();
			blog(LOG_ERROR, "[obs-rvc] Worker returned invalid converted audio.");
			continue;
		}

		if (!stream.stitch(converted, hop_frames, context_frames,
				   frame_count_for_duration(input_sample_rate, kStreamOverlapMs),
				   frame_count_for_duration(input_sample_rate, kStreamSearchMs), input_channels,
				   stitched)) {
			stream.reset();
			blog(LOG_ERROR, "[obs-rvc] Converted window is too short for streaming.");
			continue;
		}

		uint64_t underflows = 0U;
		{
			std::lock_guard<std::mutex> lock(mutex);
			if (stopping || generation != input_generation || sample_rate != input_sample_rate ||
			    channels != input_channels)
				continue;

			const size_t maximum_output = 4U * hop_frames * input_channels;
			if (output.size() + stitched.size() > maximum_output)
				output.discard(output.size() + stitched.size() - maximum_output);

			output.append(stitched);
			has_completed_conversion = true;
			underflows = output_underflows;
		}

		if (underflows != reported_underflows) {
			blog(LOG_WARNING,
			     "[obs-rvc] Output underflow detected (%llu total); audio was unavailable in time.",
			     static_cast<unsigned long long>(underflows));
			reported_underflows = underflows;
		}

		const double duration = static_cast<double>(hop_frames) / input_sample_rate;
		const double realtime_factor =
			std::chrono::duration<double>(std::chrono::steady_clock::now() - started_at).count() / duration;

		blog(LOG_DEBUG, "[obs-rvc] Converted %.0f ms (RTF %.2f).", duration * 1000.0, realtime_factor);
		const auto now = std::chrono::steady_clock::now();
		int32_t increased_duration = 0;
		if (realtime_factor > 1.0 && !first_conversion) {
			std::lock_guard<std::mutex> lock(mutex);
			if (generation == input_generation && active_chunk_duration_ms < maximum_chunk_duration_ms) {
				active_chunk_duration_ms =
					std::min(maximum_chunk_duration_ms, active_chunk_duration_ms + 250);
				increased_duration = active_chunk_duration_ms;
			}
		}

		if (increased_duration > 0) {
			blog(LOG_WARNING,
			     "[obs-rvc] Inference slower than realtime (RTF %.2f); increasing conversion block to %d ms.",
			     realtime_factor, increased_duration);
			last_warning = now;
		} else if (!first_conversion && realtime_factor > 1.0 && now - last_warning > std::chrono::seconds(5)) {
			blog(LOG_WARNING,
			     "[obs-rvc] Inference slower than realtime (RTF %.2f); use an accelerated worker or increase the block size.",
			     realtime_factor);
			last_warning = now;
		}
	}
}
}
