#pragma once

#include <cstddef>
#include <vector>

namespace rvc::filter {
constexpr unsigned kStreamHistoryMs = 40;
constexpr unsigned kStreamOverlapMs = 30;
constexpr unsigned kStreamSearchMs = 10;
constexpr unsigned kStreamLookaheadMs = kStreamOverlapMs + kStreamSearchMs + 20;

class AudioSampleQueue {
public:
	size_t size() const;
	void clear();
	void append(const std::vector<float> &values);
	void copy_front(std::vector<float> &destination, size_t requested) const;
	const float *front_data() const;
	size_t front_size() const;
	void discard(size_t requested);

private:
	void ensure_capacity(size_t required);

	std::vector<float> storage;
	size_t head = 0;
	size_t count = 0;
};

class StreamAssembler {
public:
	void reset();
	std::vector<float> window(const std::vector<float> &input, size_t hop, size_t context, size_t channels);
	bool stitch(const std::vector<float> &converted, size_t hop, size_t context, size_t overlap, size_t search,
		    size_t channels, std::vector<float> &result);

private:
	std::vector<float> history;
	std::vector<float> tail;
};
}
