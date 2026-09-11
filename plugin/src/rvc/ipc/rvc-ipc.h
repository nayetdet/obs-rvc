#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "audio/rvc-audio-transport.h"
#include "settings/rvc-settings-transport.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rvc_ipc rvc_ipc_t;

enum rvc_ipc_status {
	RVC_IPC_STATUS_OK = 0,
	RVC_IPC_STATUS_WORKER_ERROR = 1,
	RVC_IPC_STATUS_INVALID_ARGUMENT = 2,
	RVC_IPC_STATUS_TRANSPORT_ERROR = 3,
	RVC_IPC_STATUS_TIMEOUT = 4,
};

bool rvc_ipc_create(rvc_ipc_t **context);
void rvc_ipc_destroy(rvc_ipc_t *context);

enum rvc_ipc_status rvc_ipc_configure(rvc_ipc_t *context, const rvc_settings_request_t *request,
				      rvc_settings_response_t *response, uint32_t timeout_ms);
enum rvc_ipc_status rvc_ipc_convert(rvc_ipc_t *context, const rvc_audio_request_t *request,
				    rvc_audio_response_t *response, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
