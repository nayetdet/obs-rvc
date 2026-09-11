#pragma once

#include <stddef.h>
#include <stdint.h>

#define RVC_AUDIO_MAX_MODEL_BYTES 128U
#define RVC_AUDIO_MAX_PATH_BYTES 512U
#define RVC_AUDIO_MAX_ERROR_BYTES 1024U
#define RVC_AUDIO_MAX_INPUT_BYTES (4U * 1024U * 1024U)
#define RVC_AUDIO_MAX_OUTPUT_BYTES (8U * 1024U * 1024U)

enum rvc_audio_response_status {
	RVC_AUDIO_RESPONSE_OK = 0,
	RVC_AUDIO_RESPONSE_ERROR = 1,
};

typedef struct rvc_audio_request {
	uint32_t audio_size;
	char model[RVC_AUDIO_MAX_MODEL_BYTES];
	char input_format[8];
	int32_t speaker;
	int32_t f0_up_key;
	char f0_method[8];
	char index_file[RVC_AUDIO_MAX_PATH_BYTES];
	float index_rate;
	int32_t filter_radius;
	int32_t resample_sr;
	float rms_mix_rate;
	float protect;
	uint8_t audio[RVC_AUDIO_MAX_INPUT_BYTES];
} rvc_audio_request_t;

typedef struct rvc_audio_response {
	uint8_t status;
	uint32_t audio_size;
	uint32_t sample_rate;
	uint32_t error_size;
	char error[RVC_AUDIO_MAX_ERROR_BYTES];
	uint8_t audio[RVC_AUDIO_MAX_OUTPUT_BYTES];
} rvc_audio_response_t;

#if defined(__cplusplus)
static_assert(sizeof(rvc_audio_request_t) == 4194992U, "Audio request layout must match Python.");
static_assert(offsetof(rvc_audio_request_t, audio) == 688U, "Audio request layout must match Python.");
static_assert(sizeof(rvc_audio_response_t) == 8389648U, "Audio response layout must match Python.");
static_assert(offsetof(rvc_audio_response_t, audio) == 1040U, "Audio response layout must match Python.");
#else
_Static_assert(sizeof(rvc_audio_request_t) == 4194992U, "Audio request layout must match Python.");
_Static_assert(offsetof(rvc_audio_request_t, audio) == 688U, "Audio request layout must match Python.");
_Static_assert(sizeof(rvc_audio_response_t) == 8389648U, "Audio response layout must match Python.");
_Static_assert(offsetof(rvc_audio_response_t, audio) == 1040U, "Audio response layout must match Python.");
#endif
