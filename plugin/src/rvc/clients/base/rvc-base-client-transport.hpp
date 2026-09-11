#pragma once

#include "../../ipc/rvc-ipc.h"

#include <iox2/iceoryx2.hpp>

namespace rvc
{

using Node = iox2::Node<iox2::ServiceType::Ipc>;

template<typename Request, typename Response>
using Service = iox2::PortFactoryRequestResponse<iox2::ServiceType::Ipc, Request, void, Response, void>;

template<typename Request, typename Response>
using Client = iox2::Client<iox2::ServiceType::Ipc, Request, void, Response, void>;
}
