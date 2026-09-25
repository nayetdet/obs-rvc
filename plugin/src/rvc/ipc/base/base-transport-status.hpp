#pragma once

namespace core {

enum class BaseTransportStatus {
	Ok,
	RemoteError,
	TransportError,
	Timeout,
};

}
