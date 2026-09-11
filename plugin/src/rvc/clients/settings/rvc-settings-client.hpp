#pragma once

#include "rvc-settings-client-protocol.h"
#include "../../ipc/rvc-ipc.h"
#include "../base/rvc-base-client.hpp"

#include <memory>

namespace rvc
{

struct SettingsRequest final : rvc_settings_request_t
{
	static constexpr const char *IOX2_TYPE_NAME = "SettingsRequestSchema";
};

struct SettingsResponse final : rvc_settings_response_t
{
	static constexpr const char *IOX2_TYPE_NAME = "SettingsResponseSchema";
};

class SettingsClient final : public IpcRequestClient
{
public:
	explicit SettingsClient(Node &node);
	~SettingsClient();

	bool valid() const override;
	enum rvc_ipc_status configure(const rvc_settings_request_t &request, rvc_settings_response_t &response,
				      uint32_t timeout_ms);

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

}
