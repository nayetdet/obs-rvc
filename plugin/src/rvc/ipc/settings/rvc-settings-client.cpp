#include "rvc-settings-client.hpp"

#include <cstring>

namespace rvc {
namespace {
constexpr char kServiceName[] = "obs/rvc/settings";
}

constexpr const char *RvcSettingsRequest::IOX2_TYPE_NAME;
constexpr const char *RvcSettingsResponse::IOX2_TYPE_NAME;

struct RvcSettingsClient::Impl {
	std::unique_ptr<core::Service<RvcSettingsRequest, RvcSettingsResponse>> service;
	std::unique_ptr<core::Client<RvcSettingsRequest, RvcSettingsResponse>> client;
};

RvcSettingsClient::RvcSettingsClient(core::Node &node) : core::BaseClient(node), impl(std::make_unique<Impl>())
{
	(void)open(kServiceName, impl->service, impl->client);
}

RvcSettingsClient::~RvcSettingsClient() = default;

bool RvcSettingsClient::valid() const
{
	return impl->service != nullptr && impl->client != nullptr;
}

core::BaseTransportStatus RvcSettingsClient::configure(const rvc_settings_request_t &request,
						       rvc_settings_response_t &response, uint32_t timeout_ms)
{
	if (!valid())
		return core::BaseTransportStatus::TransportError;

	RvcSettingsRequest request_payload{};
	RvcSettingsResponse response_payload{};
	std::memcpy(&request_payload, &request, sizeof(request_payload));
	const core::BaseTransportStatus status = exchange(*impl->client, request_payload, response_payload, timeout_ms);
	std::memcpy(&response, &response_payload, sizeof(response));

	if (status != core::BaseTransportStatus::Ok)
		return status;

	return response.status == RVC_SETTINGS_RESPONSE_OK ? core::BaseTransportStatus::Ok
							   : core::BaseTransportStatus::RemoteError;
}

} // namespace rvc
