#pragma once

#include "rvc-base-client-transport.hpp"

#include <cstring>

namespace rvc
{

class IpcRequestClient
{
public:
	virtual ~IpcRequestClient() = default;
	virtual bool valid() const = 0;

protected:
	explicit IpcRequestClient(Node &node) : node(node) {}

	template<typename Request, typename Response>
	bool open(const char *service_name, std::unique_ptr<Service<Request, Response>> &service,
		  std::unique_ptr<Client<Request, Response>> &client)
	{
		auto name = iox2::ServiceName::create(service_name);
		if (!name)
			return false;

		auto opened_service = node.service_builder(name.value())
					  .template request_response<Request, Response>()
					  .open_or_create();
		if (!opened_service)
			return false;

		service = std::make_unique<Service<Request, Response>>(std::move(opened_service.value()));
		auto created_client = service->client_builder().create();
		if (!created_client)
			return false;

		client = std::make_unique<Client<Request, Response>>(std::move(created_client.value()));
		return true;
	}

	template<typename Request, typename Response>
	enum rvc_ipc_status exchange(Client<Request, Response> &client, const Request &request, Response &response, uint32_t timeout_ms)
	{
		auto loaned_request = client.loan_uninit();
		if (!loaned_request)
			return RVC_IPC_STATUS_TRANSPORT_ERROR;

		std::memcpy(&loaned_request.value().payload_mut(), &request, sizeof(request));
		auto pending_response = iox2::send(iox2::assume_init(std::move(loaned_request.value())));
		if (!pending_response)
			return RVC_IPC_STATUS_TRANSPORT_ERROR;

		for (uint32_t elapsed_ms = 0U; elapsed_ms < timeout_ms; ++elapsed_ms)
		{
			auto received_response = pending_response.value().receive();
			if (!received_response)
				return RVC_IPC_STATUS_TRANSPORT_ERROR;

			if (received_response.value().has_value())
			{
				std::memcpy(&response, &received_response.value()->payload(), sizeof(response));
				return RVC_IPC_STATUS_OK;
			}

			(void)node.wait(iox2::bb::Duration::from_millis(1U));
		}

		return RVC_IPC_STATUS_TIMEOUT;
	}

	Node &node;
};

}
