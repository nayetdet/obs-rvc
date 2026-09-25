#pragma once

#include "rvc/ipc/rvc-ipc.h"

#ifdef __cplusplus
#include <cstdint>
#include <mutex>
#include <string>

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
	bool configured = false;
	bool bypass_conversion = false;
	bool conversion_error_logged = false;
};

extern "C" {
#endif

void rvc_filter_register(rvc_ipc_t *ipc);
void rvc_filter_shutdown(void);

#ifdef __cplusplus
}
#endif
