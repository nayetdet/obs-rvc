#include "rvc-ipc.h"
#include "audio/rvc-audio-client.hpp"
#include "settings/rvc-settings-client.hpp"

#include <memory>

struct rvc_ipc
{
	std::unique_ptr<core::Node> node;
	std::unique_ptr<rvc::RvcSettingsClient> settings;
	std::unique_ptr<rvc::RvcAudioClient> audio;
};

static enum rvc_ipc_status to_ipc_status(core::BaseTransportStatus status)
{
	switch (status)
	{
	case core::BaseTransportStatus::Ok:
		return RVC_IPC_STATUS_OK;
	case core::BaseTransportStatus::RemoteError:
		return RVC_IPC_STATUS_WORKER_ERROR;
	case core::BaseTransportStatus::InvalidArgument:
		return RVC_IPC_STATUS_INVALID_ARGUMENT;
	case core::BaseTransportStatus::TransportError:
		return RVC_IPC_STATUS_TRANSPORT_ERROR;
	case core::BaseTransportStatus::Timeout:
		return RVC_IPC_STATUS_TIMEOUT;
	}

	return RVC_IPC_STATUS_TRANSPORT_ERROR;
}

extern "C" bool rvc_ipc_create(rvc_ipc_t **context)
{
	if (context == nullptr)
		return false;
	*context = nullptr;

	auto created_node = iox2::NodeBuilder().create<iox2::ServiceType::Ipc>();
	if (!created_node)
		return false;

	auto new_context = std::make_unique<rvc_ipc>();
	new_context->node = std::make_unique<core::Node>(std::move(created_node.value()));
	new_context->settings = std::make_unique<rvc::RvcSettingsClient>(*new_context->node);
	new_context->audio = std::make_unique<rvc::RvcAudioClient>(*new_context->node);

	if (!new_context->settings->valid() || !new_context->audio->valid())
		return false;

	*context = new_context.release();
	return true;
}

extern "C" void rvc_ipc_destroy(rvc_ipc_t *context)
{
	delete context;
}

extern "C" enum rvc_ipc_status rvc_ipc_configure(rvc_ipc_t *context, const rvc_settings_request_t *request, rvc_settings_response_t *response, uint32_t timeout_ms)
{
	if (context == nullptr || request == nullptr || response == nullptr || timeout_ms == 0U)
		return RVC_IPC_STATUS_INVALID_ARGUMENT;

	return to_ipc_status(context->settings->configure(*request, *response, timeout_ms));
}

extern "C" enum rvc_ipc_status rvc_ipc_convert(rvc_ipc_t *context, const rvc_audio_request_t *request, rvc_audio_response_t *response, uint32_t timeout_ms)
{
	if (context == nullptr || request == nullptr || response == nullptr || timeout_ms == 0U ||
	    request->audio_size > RVC_AUDIO_MAX_INPUT_BYTES)
		return RVC_IPC_STATUS_INVALID_ARGUMENT;

	return to_ipc_status(context->audio->convert(*request, *response, timeout_ms));
}
