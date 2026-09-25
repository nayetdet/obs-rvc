#pragma once

#include "rvc/ipc/rvc-ipc.h"

#include <cstdint>
#include <memory>
#include <string>

struct obs_audio_data;

namespace rvc::filter {
struct ConversionOptions {
	std::string model, f0_method;
	int32_t speaker, f0_up_key, filter_radius, resample_sr;
	float rms_mix_rate, protect;
	int32_t chunk_duration_ms;
};
class ConversionWorker {
public:
	explicit ConversionWorker(rvc_ipc_t *ipc);
	~ConversionWorker();
	ConversionWorker(const ConversionWorker &) = delete;
	ConversionWorker &operator=(const ConversionWorker &) = delete;

	void reset(const ConversionOptions &options);
	void submit(const obs_audio_data &audio, uint32_t sample_rate, uint16_t channels);
	bool receive(obs_audio_data &audio, uint16_t channels);
	void stop();
	static void stop_all();

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
} // namespace rvc::filter
