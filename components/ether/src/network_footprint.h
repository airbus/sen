// === network_footprint.h =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_NETWORK_FOOTPRINT_H
#define SEN_COMPONENTS_ETHER_SRC_NETWORK_FOOTPRINT_H

// component
#include "network_exclusion.h"
#include "port_binding.h"

// sen
#include "sen/core/base/span.h"

// generated code
#include "stl/configuration.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/network_footprint.stl.h"

// asio
#include <asio/ip/address_v4.hpp>

// std
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

namespace sen::components::ether
{

/// Builds the network footprint
///
/// @param configuredBusAddresses: bus addresses configured
/// @param suppliedBusAddresses: additional bus addresses supplied
/// @param config: ether configuration used to calculate the footprint
/// @param exclusions: multicast and port exclusions
/// @return the network footprint
[[nodiscard]] kernel::NetworkFootprint makeNetworkFootprint(Span<const kernel::BusAddress> configuredBusAddresses,
                                                            Span<const kernel::BusAddress> suppliedBusAddresses,
                                                            const Configuration& config,
                                                            const NetworkExclusions& exclusions);

using RuntimeFootprintTransportId = uint64_t;
using RuntimeFootprintPortId = uint64_t;

constexpr RuntimeFootprintTransportId invalidRuntimeFootprintTransportId = 0U;
constexpr RuntimeFootprintPortId invalidRuntimeFootprintPortId = 0U;

/// Stores the ports and multicast buses currently allocated by Ether transports.
class RuntimeNetworkFootprintState final
{
public:
  RuntimeNetworkFootprintState(Configuration config, NetworkExclusions exclusions);

  [[nodiscard]] RuntimeFootprintTransportId addTransport(std::string sessionName, uint32_t sessionId);
  void removeTransport(RuntimeFootprintTransportId transportId);
  void addBus(RuntimeFootprintTransportId transportId,
              uint32_t busId,
              std::string busName,
              asio::ip::address_v4 groupAddress);
  void removeBus(RuntimeFootprintTransportId transportId, uint32_t busId);
  [[nodiscard]] RuntimeFootprintPortId addPort(RuntimeFootprintTransportId transportId, PortKind kind, uint16_t port);
  void removePort(RuntimeFootprintTransportId transportId, RuntimeFootprintPortId portId);

  [[nodiscard]] kernel::NetworkFootprint snapshot() const;

private:
  struct RuntimeBus
  {
    uint32_t busId = 0;
    std::string busName;
    asio::ip::address_v4 groupAddress;
  };

  struct RuntimePort
  {
    PortKind kind = PortKind::tcpAcceptor;
    uint16_t port = 0;
  };

  struct RuntimeTransport
  {
    std::string sessionName;
    uint32_t sessionId = 0;
    std::unordered_map<uint32_t, RuntimeBus> buses;
    std::unordered_map<RuntimeFootprintPortId, RuntimePort> ports;
  };

private:
  Configuration config_;
  NetworkExclusions exclusions_;
  mutable std::mutex mutex_;
  std::unordered_map<RuntimeFootprintTransportId, RuntimeTransport> transports_;
  RuntimeFootprintTransportId nextTransportId_ = 1U;
  RuntimeFootprintPortId nextPortId_ = 1U;
};

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_NETWORK_FOOTPRINT_H
