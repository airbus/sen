// === util.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_UTIL_H
#define SEN_COMPONENTS_ETHER_SRC_UTIL_H

// generated code
#include "stl/configuration.stl.h"
#include "stl/discovery.stl.h"
#include "stl/runtime.stl.h"

// kernel
#include "sen/kernel/transport.h"

// spdlog
#include <spdlog/logger.h>

// asio
#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>

// std
#include <cstdint>
#include <vector>

namespace sen::components::ether
{

class EtherTransport;
class ProcessHandler;
class Acceptor;

constexpr uint32_t etherProtocolVersion = 2;

struct NetworkInterfaceInfo
{
  asio::ip::address address;
  std::string deviceName;
};

template <typename T, typename B>
[[nodiscard]] inline T readFromBuffer(const B& buffer)
{
  T val {};
  InputStream in({buffer.data(), buffer.size()});
  SerializationTraits<T>::read(in, val);
  return val;
}

[[nodiscard]] inline asio::ip::tcp::endpoint getAsioTcpEndpoint(const Endpoint& endpoint)
{
  return {asio::ip::make_address_v4(endpoint.ip), endpoint.port};
}

[[nodiscard]] std::vector<NetworkInterfaceInfo> getLocalInterfaces(const Configuration& config);

void configureMulticastSocket(asio::ip::udp::socket& socket,
                              asio::ip::udp::endpoint multicastEndpoint,
                              const MaybeDeviceName& deviceName,
                              const Configuration& config,
                              spdlog::logger* logger);

void configureTcpSocket(asio::ip::tcp::socket& socket, const Configuration& config);

[[nodiscard]] std::shared_ptr<spdlog::logger> getLogger();

struct BeamInfo
{
  ::sen::kernel::ProcessInfo process;
  EnpointList endpoints;
};

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_UTIL_H
