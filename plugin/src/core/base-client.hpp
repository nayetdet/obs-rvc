#pragma once

#include "base-transport.hpp"
#include "base-transport-status.hpp"

#include <cstring>
#include <memory>

namespace core
{

class BaseClient
{
public:
	virtual ~BaseClient() = default;
	virtual bool valid() const = 0;

protected:
	explicit BaseClient(BaseNode &node) : node(node) {}

	template<typename Request, typename Response>
	bool open(const char *service_name, std::unique_ptr<BaseService<Request, Response>> &service, std::unique_ptr<BaseClientPort<Request, Response>> &client)
	{
		auto name = iox2::ServiceName::create(service_name);
		if (!name)
			return false;

		auto opened_service = node.service_builder(name.value())
			.template request_response<Request, Response>()
			.open_or_create();

		if (!opened_service)
			return false;

		service = std::make_unique<BaseService<Request, Response>>(std::move(opened_service.value()));
		auto created_client = service->client_builder().create();
		if (!created_client)
			return false;

		client = std::make_unique<BaseClientPort<Request, Response>>(std::move(created_client.value()));
		return true;
	}

	template<typename Request, typename Response>
	BaseTransportStatus exchange(BaseClientPort<Request, Response> &client, const Request &request, Response &response, uint32_t timeout_ms)
	{
		auto loaned_request = client.loan_uninit();
		if (!loaned_request)
			return BaseTransportStatus::TransportError;

		std::memcpy(&loaned_request.value().payload_mut(), &request, sizeof(request));
		auto pending_response = iox2::send(iox2::assume_init(std::move(loaned_request.value())));
		if (!pending_response)
			return BaseTransportStatus::TransportError;

		for (uint32_t elapsed_ms = 0U; elapsed_ms < timeout_ms; ++elapsed_ms)
		{
			auto received_response = pending_response.value().receive();
			if (!received_response)
				return BaseTransportStatus::TransportError;

			if (received_response.value().has_value())
			{
				std::memcpy(&response, &received_response.value()->payload(), sizeof(response));
				return BaseTransportStatus::Ok;
			}

			(void)node.wait(iox2::bb::Duration::from_millis(1U));
		}

		return BaseTransportStatus::Timeout;
	}

	BaseNode &node;
};

}
