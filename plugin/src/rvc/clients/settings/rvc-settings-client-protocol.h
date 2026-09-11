#pragma once

#include <stddef.h>
#include <stdint.h>

#define RVC_SETTINGS_MAX_MODEL_BYTES 128U
#define RVC_SETTINGS_MAX_PATH_BYTES 512U
#define RVC_SETTINGS_MAX_ERROR_BYTES 1024U

enum rvc_settings_response_status
{
	RVC_SETTINGS_RESPONSE_OK = 0,
	RVC_SETTINGS_RESPONSE_ERROR = 1,
};

typedef struct rvc_settings_request
{
	char model[RVC_SETTINGS_MAX_MODEL_BYTES];
	char model_dir[RVC_SETTINGS_MAX_PATH_BYTES];
	char hubert_path[RVC_SETTINGS_MAX_PATH_BYTES];
	char rmvpe_root[RVC_SETTINGS_MAX_PATH_BYTES];
} rvc_settings_request_t;

typedef struct rvc_settings_response
{
	uint8_t status;
	uint32_t error_size;
	char error[RVC_SETTINGS_MAX_ERROR_BYTES];
} rvc_settings_response_t;

#if defined(__cplusplus)
static_assert(sizeof(rvc_settings_request_t) == 1664U, "Settings request layout must match Python.");
static_assert(sizeof(rvc_settings_response_t) == 1032U, "Settings response layout must match Python.");
static_assert(offsetof(rvc_settings_response_t, error_size) == 4U, "Settings response layout must match Python.");
#else
_Static_assert(sizeof(rvc_settings_request_t) == 1664U, "Settings request layout must match Python.");
_Static_assert(sizeof(rvc_settings_response_t) == 1032U, "Settings response layout must match Python.");
_Static_assert(offsetof(rvc_settings_response_t, error_size) == 4U, "Settings response layout must match Python.");
#endif
