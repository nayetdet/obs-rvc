#pragma once

namespace core {

enum class BaseTransportStatus {
	Ok,
	RemoteError,
	InvalidArgument,
	TransportError,
	Timeout,
};

}
