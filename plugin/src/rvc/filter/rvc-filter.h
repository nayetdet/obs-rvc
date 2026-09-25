#pragma once

#include "rvc/ipc/rvc-ipc.h"

#ifdef __cplusplus
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace rvc::filter {
class ConversionWorker;
}

struct RvcFilterData {
	std::mutex mutex;
	std::string model;
	std::string f0_method;
	int32_t speaker = 0;
	int32_t f0_up_key = 0;
	int32_t filter_radius = 3;
	int32_t resample_sr = 0;
	float rms_mix_rate = 0.25F;
	float protect = 0.33F;
	int32_t chunk_duration_ms = 8000;
	bool configured = false;
	std::unique_ptr<rvc::filter::ConversionWorker> conversion_worker;
};

extern "C" {
#endif

void rvc_filter_register(rvc_ipc_t *ipc);
void rvc_filter_shutdown(void);

#ifdef __cplusplus
}
#endif
