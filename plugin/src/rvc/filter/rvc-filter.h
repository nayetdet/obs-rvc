#pragma once

#include "../ipc/rvc-ipc.h"

#ifdef __cplusplus
extern "C" {
#endif

void rvc_filter_register(rvc_ipc_t *ipc);
void rvc_filter_shutdown(void);

#ifdef __cplusplus
}
#endif
