#pragma once

#include "rvc-settings-transport.h"
#include "../base/base-client.hpp"

#include <memory>

namespace rvc
{

struct RvcSettingsRequest final : rvc_settings_request_t
{
	static constexpr const char *IOX2_TYPE_NAME = "SettingsRequestSchema";
};

struct RvcSettingsResponse final : rvc_settings_response_t
{
	static constexpr const char *IOX2_TYPE_NAME = "SettingsResponseSchema";
};

class RvcSettingsClient final : public core::BaseClient
{
public:
	explicit RvcSettingsClient(core::Node &node);
	~RvcSettingsClient();

	bool valid() const override;
	core::BaseTransportStatus configure(const rvc_settings_request_t &request, rvc_settings_response_t &response,
						   uint32_t timeout_ms);

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

}
