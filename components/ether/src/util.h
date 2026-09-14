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
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace sen::components::ether
{

class EtherTransport;
class ProcessHandler;
class Acceptor;

constexpr uint32_t etherProtocolVersion = 2;

// A stream socket has no message boundaries: one read can return part of a beam, or several. Each
// beam is written with its length in front so a reader can take exactly one, which is how the data
// channel already frames its own messages.
constexpr std::size_t beamHeaderSize = 4U;

inline std::array<uint8_t, beamHeaderSize> encodeBeamHeader(std::size_t payloadSize)
{
  const auto size = static_cast<uint32_t>(payloadSize);
  return {static_cast<uint8_t>(size & 0xFFU),
          static_cast<uint8_t>((size >> 8U) & 0xFFU),
          static_cast<uint8_t>((size >> 16U) & 0xFFU),
          static_cast<uint8_t>((size >> 24U) & 0xFFU)};
}

inline uint32_t decodeBeamHeader(const std::array<uint8_t, beamHeaderSize>& header)
{
  return static_cast<uint32_t>(header[0]) | (static_cast<uint32_t>(header[1]) << 8U) |
         (static_cast<uint32_t>(header[2]) << 16U) | (static_cast<uint32_t>(header[3]) << 24U);
}
constexpr uint16_t defaultDiscoveryPort = 60543;

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

/// Gets the discovery port used to compute bus multicast address allocation.
///
/// @param config: ether configuration containing the discovery settings
/// @return configured multicast discovery port
[[nodiscard]] uint16_t getBusDiscoveryPort(const Configuration& config) noexcept;

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
