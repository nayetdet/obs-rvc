#pragma once

#include "rvc-audio-transport.h"
#include "../base/base-client.hpp"

#include <memory>

namespace rvc
{

struct RvcAudioRequest final : rvc_audio_request_t
{
	static constexpr const char *IOX2_TYPE_NAME = "AudioRequestSchema";
};

struct RvcAudioResponse final : rvc_audio_response_t
{
	static constexpr const char *IOX2_TYPE_NAME = "AudioResponseSchema";
};

class RvcAudioClient final : public core::BaseClient
{
public:
	explicit RvcAudioClient(core::Node &node);
	~RvcAudioClient();

	bool valid() const override;
	core::BaseTransportStatus convert(const rvc_audio_request_t &request, rvc_audio_response_t &response,
						 uint32_t timeout_ms);

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

}
