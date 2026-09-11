#pragma once

#include "rvc-audio-client-protocol.h"
#include "../../ipc/rvc-ipc.h"
#include "../base/rvc-base-client.hpp"

#include <memory>

namespace rvc
{

struct AudioRequest final : rvc_audio_request_t
{
	static constexpr const char *IOX2_TYPE_NAME = "AudioRequestSchema";
};

struct AudioResponse final : rvc_audio_response_t
{
	static constexpr const char *IOX2_TYPE_NAME = "AudioResponseSchema";
};

class AudioClient final : public IpcRequestClient
{
public:
	explicit AudioClient(Node &node);
	~AudioClient();

	bool valid() const override;
	enum rvc_ipc_status convert(const rvc_audio_request_t &request, rvc_audio_response_t &response,
				   uint32_t timeout_ms);

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

}
