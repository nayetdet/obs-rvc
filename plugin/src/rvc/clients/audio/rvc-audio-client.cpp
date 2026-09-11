#include "rvc-audio-client.hpp"

#include <cstring>

namespace rvc
{
namespace
{
constexpr char kServiceName[] = "obs/rvc";
}

constexpr const char *AudioRequest::IOX2_TYPE_NAME;
constexpr const char *AudioResponse::IOX2_TYPE_NAME;

struct AudioClient::Impl
{
	std::unique_ptr<Service<AudioRequest, AudioResponse>> service;
	std::unique_ptr<Client<AudioRequest, AudioResponse>> client;
};

AudioClient::AudioClient(Node &node) : IpcRequestClient(node), impl(std::make_unique<Impl>())
{
	(void)open(kServiceName, impl->service, impl->client);
}

AudioClient::~AudioClient() = default;

bool AudioClient::valid() const
{
	return impl->service != nullptr && impl->client != nullptr;
}

enum rvc_ipc_status AudioClient::convert(const rvc_audio_request_t &request, rvc_audio_response_t &response,
						 uint32_t timeout_ms)
{
	if (!valid())
		return RVC_IPC_STATUS_TRANSPORT_ERROR;

	auto request_payload = std::make_unique<AudioRequest>();
	auto response_payload = std::make_unique<AudioResponse>();
	std::memcpy(request_payload.get(), &request, sizeof(*request_payload));
	const enum rvc_ipc_status status = exchange(*impl->client, *request_payload, *response_payload, timeout_ms);
	std::memcpy(&response, response_payload.get(), sizeof(response));
	if (status != RVC_IPC_STATUS_OK)
		return status;
	if (response_payload->audio_size > RVC_AUDIO_MAX_OUTPUT_BYTES)
		return RVC_IPC_STATUS_TRANSPORT_ERROR;
	return response_payload->status == RVC_AUDIO_RESPONSE_OK ? RVC_IPC_STATUS_OK : RVC_IPC_STATUS_WORKER_ERROR;
}

}
