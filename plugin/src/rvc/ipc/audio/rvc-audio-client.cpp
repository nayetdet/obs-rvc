#include "rvc-audio-client.hpp"

#include <cstring>

namespace rvc {
namespace {
constexpr char kServiceName[] = "obs/rvc";
}

constexpr const char *RvcAudioRequest::IOX2_TYPE_NAME;
constexpr const char *RvcAudioResponse::IOX2_TYPE_NAME;

struct RvcAudioClient::Impl {
	std::unique_ptr<core::Service<RvcAudioRequest, RvcAudioResponse>> service;
	std::unique_ptr<core::Client<RvcAudioRequest, RvcAudioResponse>> client;
};

RvcAudioClient::RvcAudioClient(core::Node &node) : core::BaseClient(node), impl(std::make_unique<Impl>())
{
	(void)open(kServiceName, impl->service, impl->client);
}

RvcAudioClient::~RvcAudioClient() = default;

bool RvcAudioClient::valid() const
{
	return impl->service != nullptr && impl->client != nullptr;
}

core::BaseTransportStatus RvcAudioClient::convert(const rvc_audio_request_t &request, rvc_audio_response_t &response,
						  uint32_t timeout_ms)
{
	if (!valid())
		return core::BaseTransportStatus::TransportError;

	auto request_payload = std::make_unique<RvcAudioRequest>();
	auto response_payload = std::make_unique<RvcAudioResponse>();
	std::memcpy(request_payload.get(), &request, sizeof(*request_payload));
	const core::BaseTransportStatus status =
		exchange(*impl->client, *request_payload, *response_payload, timeout_ms);
	std::memcpy(&response, response_payload.get(), sizeof(response));

	if (status != core::BaseTransportStatus::Ok)
		return status;

	if (response_payload->audio_size > RVC_AUDIO_MAX_OUTPUT_BYTES)
		return core::BaseTransportStatus::TransportError;

	return response_payload->status == RVC_AUDIO_RESPONSE_OK ? core::BaseTransportStatus::Ok
								 : core::BaseTransportStatus::RemoteError;
}

} // namespace rvc
