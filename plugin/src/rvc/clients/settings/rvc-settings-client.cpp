#include "rvc-settings-client.hpp"

#include <cstring>

namespace rvc
{
namespace
{
constexpr char kServiceName[] = "obs/rvc/settings";
}

constexpr const char *SettingsRequest::IOX2_TYPE_NAME;
constexpr const char *SettingsResponse::IOX2_TYPE_NAME;

struct SettingsClient::Impl
{
	std::unique_ptr<Service<SettingsRequest, SettingsResponse>> service;
	std::unique_ptr<Client<SettingsRequest, SettingsResponse>> client;
};

SettingsClient::SettingsClient(Node &node) : IpcRequestClient(node), impl(std::make_unique<Impl>())
{
	(void)open(kServiceName, impl->service, impl->client);
}

SettingsClient::~SettingsClient() = default;

bool SettingsClient::valid() const
{
	return impl->service != nullptr && impl->client != nullptr;
}

enum rvc_ipc_status SettingsClient::configure(const rvc_settings_request_t &request,
						       rvc_settings_response_t &response, uint32_t timeout_ms)
{
	if (!valid())
		return RVC_IPC_STATUS_TRANSPORT_ERROR;

	SettingsRequest request_payload {};
	SettingsResponse response_payload {};
	std::memcpy(&request_payload, &request, sizeof(request_payload));
	const enum rvc_ipc_status status = exchange(*impl->client, request_payload, response_payload, timeout_ms);
	std::memcpy(&response, &response_payload, sizeof(response));
	if (status != RVC_IPC_STATUS_OK)
		return status;
	return response.status == RVC_SETTINGS_RESPONSE_OK ? RVC_IPC_STATUS_OK : RVC_IPC_STATUS_WORKER_ERROR;
}

}
