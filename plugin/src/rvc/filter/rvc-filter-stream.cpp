#include "rvc-filter-stream.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace rvc::filter {
size_t AudioSampleQueue::size() const
{
	return count;
}

void AudioSampleQueue::clear()
{
	head = 0;
	count = 0;
}

void AudioSampleQueue::append(const std::vector<float> &values)
{
	if (values.empty())
		return;

	ensure_capacity(count + values.size());
	const size_t tail_index = (head + count) % storage.size();
	const size_t first = std::min(values.size(), storage.size() - tail_index);
	std::memcpy(storage.data() + tail_index, values.data(), first * sizeof(float));
	if (first < values.size())
		std::memcpy(storage.data(), values.data() + first, (values.size() - first) * sizeof(float));
	count += values.size();
}

void AudioSampleQueue::copy_front(std::vector<float> &destination, size_t requested) const
{
	requested = std::min(requested, count);
	destination.resize(requested);
	if (requested == 0)
		return;

	const size_t first = std::min(requested, storage.size() - head);
	std::memcpy(destination.data(), storage.data() + head, first * sizeof(float));
	if (first < requested)
		std::memcpy(destination.data() + first, storage.data(), (requested - first) * sizeof(float));
}

void AudioSampleQueue::discard(size_t requested)
{
	const size_t discarded = std::min(requested, count);
	head = storage.empty() ? 0 : (head + discarded) % storage.size();
	count -= discarded;
}

void AudioSampleQueue::ensure_capacity(size_t required)
{
	if (required <= storage.size())
		return;

	size_t capacity = std::max<size_t>(storage.size(), 4096);
	while (capacity < required)
		capacity *= 2;

	std::vector<float> expanded(capacity);
	if (count != 0) {
		const size_t first = std::min(count, storage.size() - head);
		std::memcpy(expanded.data(), storage.data() + head, first * sizeof(float));
		if (first < count)
			std::memcpy(expanded.data() + first, storage.data(), (count - first) * sizeof(float));
	}

	storage.swap(expanded);
	head = 0;
}

void StreamAssembler::reset()
{
	history.clear();
	tail.clear();
}

void StreamAssembler::window(const std::vector<float> &input, size_t hop, size_t context, size_t channels,
			     std::vector<float> &result)
{
	if (history.size() != context * channels)
		history.assign(context * channels, 0.0F);

	result.resize(history.size() + input.size());
	std::memcpy(result.data(), history.data(), history.size() * sizeof(float));
	std::memcpy(result.data() + history.size(), input.data(), input.size() * sizeof(float));
	std::memcpy(history.data(), result.data() + hop * channels, history.size() * sizeof(float));
}

bool StreamAssembler::stitch(const std::vector<float> &converted, size_t hop, size_t context, size_t overlap,
			     size_t search, size_t channels, std::vector<float> &result)
{
	if (channels == 0 || overlap < 2 || converted.size() < (context + hop + overlap + search) * channels)
		return false;

	size_t offset = 0;
	if (tail.size() == overlap * channels) {
		double best = -2.0;
		for (size_t shift = 0; shift <= search; ++shift) {
			double dot = 0.0, energy = 1e-12, previous_energy = 1e-12;
			for (size_t i = 0; i < tail.size(); ++i) {
				const float value = converted[(context + shift) * channels + i];
				dot += value * tail[i];
				energy += value * value;
				previous_energy += tail[i] * tail[i];
			}
			const double score = dot / std::sqrt(energy * previous_energy);
			if (score > best) {
				best = score;
				offset = shift;
			}
		}
	}

	const auto start = converted.begin() + (context + offset) * channels;
	result.assign(start, start + hop * channels);
	constexpr float pi = 3.14159265358979323846F;
	for (size_t frame = 0; frame < overlap; ++frame) {
		const float position = static_cast<float>(frame) / static_cast<float>(overlap - 1);
		const float fade = 0.5F - 0.5F * std::cos(pi * position);
		for (size_t channel = 0; channel < channels; ++channel) {
			const size_t i = frame * channels + channel;
			const float previous = tail.empty() ? 0.0F : tail[i];
			result[i] = previous * (1.0F - fade) + result[i] * fade;
		}
	}

	tail.assign(start + hop * channels, start + (hop + overlap) * channels);
	return true;
}
}
