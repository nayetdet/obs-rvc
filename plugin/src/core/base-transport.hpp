#pragma once

#include <iox2/iceoryx2.hpp>

namespace core
{

using BaseNode = iox2::Node<iox2::ServiceType::Ipc>;

template<typename Request, typename Response>
using BaseService = iox2::PortFactoryRequestResponse<iox2::ServiceType::Ipc, Request, void, Response, void>;

template<typename Request, typename Response>
using BaseClientPort = iox2::Client<iox2::ServiceType::Ipc, Request, void, Response, void>;
}
