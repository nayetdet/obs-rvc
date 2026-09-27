#pragma once

#include "rvc/ipc/rvc-ipc.h"

#ifdef __cplusplus
#include <memory>
#include <mutex>

namespace rvc::filter {
class ConversionWorker;
}

struct RvcFilterData {
	std::mutex mutex;
	std::mutex update_mutex;
	bool configured = false;
	bool gpu_accelerated = false;
	std::unique_ptr<rvc::filter::ConversionWorker> conversion_worker;
};

extern "C" {
#endif

void rvc_filter_register(rvc_ipc_t *ipc);
void rvc_filter_shutdown(void);

#ifdef __cplusplus
}
#endif
