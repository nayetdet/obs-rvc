#include "rvc-audio-client.hpp"

#include <map>
#include <mutex>
#include <thread>

namespace rvc {
namespace {
constexpr char kServiceName[] = "obs/rvc";
}

constexpr const char *RvcAudioRequest::IOX2_TYPE_NAME;
constexpr const char *RvcAudioResponse::IOX2_TYPE_NAME;

struct RvcAudioClient::Impl {
	struct Connection {
		std::unique_ptr<core::Service<RvcAudioRequest, RvcAudioResponse>> service;
		std::unique_ptr<core::Client<RvcAudioRequest, RvcAudioResponse>> client;
	};

	std::mutex connections_mutex;
	std::map<std::thread::id, std::unique_ptr<Connection>> connections;
};

RvcAudioClient::RvcAudioClient(core::Node &node) : core::BaseClient(node), impl(std::make_unique<Impl>())
{
	auto connection = std::make_unique<Impl::Connection>();
	if (!open(kServiceName, connection->service, connection->client))
		return;
	impl->connections.emplace(std::this_thread::get_id(), std::move(connection));
}

RvcAudioClient::~RvcAudioClient() = default;

bool RvcAudioClient::valid() const
{
	std::lock_guard<std::mutex> lock(impl->connections_mutex);
	return !impl->connections.empty();
}

core::BaseTransportStatus RvcAudioClient::convert(const rvc_audio_request_t &request, rvc_audio_response_t &response,
						  uint32_t timeout_ms)
{
	if (!valid())
		return core::BaseTransportStatus::TransportError;

	core::Client<RvcAudioRequest, RvcAudioResponse> *client = nullptr;
	{
		std::lock_guard<std::mutex> lock(impl->connections_mutex);
		const std::thread::id thread_id = std::this_thread::get_id();
		auto found = impl->connections.find(thread_id);
		if (found == impl->connections.end()) {
			auto connection = std::make_unique<Impl::Connection>();
			if (!open(kServiceName, connection->service, connection->client))
				return core::BaseTransportStatus::TransportError;
			found = impl->connections.emplace(thread_id, std::move(connection)).first;
		}
		client = found->second->client.get();
	}

	const core::BaseTransportStatus status = exchange(*client, request, response, timeout_ms);
	if (status != core::BaseTransportStatus::Ok)
		return status;

	if (response.audio_size > RVC_AUDIO_MAX_OUTPUT_BYTES || response.audio_size % sizeof(int16_t) != 0U)
		return core::BaseTransportStatus::TransportError;

	return response.status == RVC_AUDIO_RESPONSE_OK ? core::BaseTransportStatus::Ok
							: core::BaseTransportStatus::RemoteError;
}

}
