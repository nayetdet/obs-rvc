#include "rvc-audio-client.hpp"

#include <mutex>

namespace rvc {
namespace {
constexpr char kServiceName[] = "obs/rvc";
}

constexpr const char *RvcAudioRequest::IOX2_TYPE_NAME;
constexpr const char *RvcAudioResponse::IOX2_TYPE_NAME;

struct RvcAudioClient::Impl {
	std::unique_ptr<core::Service<RvcAudioRequest, RvcAudioResponse>> service;
	std::unique_ptr<core::Client<RvcAudioRequest, RvcAudioResponse>> client;
	std::mutex convert_mutex;
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

	std::lock_guard<std::mutex> lock(impl->convert_mutex);
	const core::BaseTransportStatus status = exchange(*impl->client, request, response, timeout_ms);
	if (status != core::BaseTransportStatus::Ok)
		return status;

	if (response.audio_size > RVC_AUDIO_MAX_OUTPUT_BYTES)
		return core::BaseTransportStatus::TransportError;

	return response.status == RVC_AUDIO_RESPONSE_OK ? core::BaseTransportStatus::Ok
							: core::BaseTransportStatus::RemoteError;
}

}
