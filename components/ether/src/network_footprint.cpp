// === network_footprint.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "network_footprint.h"

// component
#include "bus_handler.h"
#include "network_exclusion.h"
#include "port_binding.h"
#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/span.h"

// generated code
#include "stl/configuration.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/network_footprint.stl.h"

// asio
#include <asio/ip/address_v4.hpp>

// std
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace sen::components::ether
{

namespace
{

/// Combines configured and supplied buses while preserving order and removing duplicates
[[nodiscard]] std::vector<kernel::BusAddress> mergeBusAddresses(Span<const kernel::BusAddress> configuredBusAddresses,
                                                                Span<const kernel::BusAddress> suppliedBusAddresses)
{
  std::vector<kernel::BusAddress> busAddresses;
  busAddresses.reserve(configuredBusAddresses.size() + suppliedBusAddresses.size());

  // Appends addresses that are not already present.
  const auto appendUnique = [&busAddresses](Span<const kernel::BusAddress> addresses)
  {
    for (const auto& address: addresses)
    {
      if (std::find(busAddresses.begin(), busAddresses.end(), address) == busAddresses.end())
      {
        busAddresses.push_back(address);
      }
    }
  };

  appendUnique(configuredBusAddresses);
  appendUnique(suppliedBusAddresses);
  return busAddresses;
}

/// Classifies a bus as configured or supplied
[[nodiscard]] kernel::NetworkFootprintBusSource getBusSource(const kernel::BusAddress& busAddress,
                                                             Span<const kernel::BusAddress> configuredBusAddresses)
{
  const auto configured =
    std::find(configuredBusAddresses.begin(), configuredBusAddresses.end(), busAddress) != configuredBusAddresses.end();
  return configured ? kernel::NetworkFootprintBusSource::configured : kernel::NetworkFootprintBusSource::supplied;
}

/// Converts a multicast allocation into a footprint bus identity
[[nodiscard]] kernel::NetworkFootprintBusIdentity toFootprintBusIdentity(
  const ConfiguredBusMulticastAllocation& allocation)
{
  return {
    allocation.busAddress.sessionName,
    allocation.busAddress.busName,
    allocation.sessionId,
    allocation.busId,
  };
}

/// Converts a multicast allocation into a footprint bus entry
[[nodiscard]] kernel::NetworkFootprintBus toFootprintBus(const ConfiguredBusMulticastAllocation& allocation,
                                                         kernel::NetworkFootprintBusSource source)
{
  return {
    allocation.busAddress.sessionName,
    allocation.busAddress.busName,
    allocation.sessionId,
    allocation.busId,
    allocation.groupAddress.to_string(),
    source,
  };
}

/// Converts a detected multicast collision into its footprint representation
[[nodiscard]] kernel::NetworkFootprintSelfCollision toFootprintSelfCollision(const MulticastSelfCollision& collision)
{
  return {
    toFootprintBusIdentity(collision.firstAllocation),
    toFootprintBusIdentity(collision.secondAllocation),
    collision.firstAllocation.groupAddress.to_string(),
  };
}

/// Converts multicast exclusion ranges into textual IPv4 ranges.
[[nodiscard]] kernel::NetworkFootprintAddressRangeList toFootprintAddressRanges(const MulticastExclusions& exclusions)
{
  kernel::NetworkFootprintAddressRangeList footprintRanges;
  footprintRanges.reserve(exclusions.ranges().size());
  for (const auto& range: exclusions.ranges())
  {
    footprintRanges.push_back(
      {asio::ip::make_address_v4(range.min).to_string(), asio::ip::make_address_v4(range.max).to_string()});
  }
  return footprintRanges;
}

/// Converts multicast capacity and collision data into the footprint model
[[nodiscard]] kernel::NetworkFootprintCollision toFootprintCollision(const MulticastCollisionAnalysis& analysis)
{
  kernel::NetworkFootprintSelfCollisionList selfCollisions;
  selfCollisions.reserve(analysis.selfCollisions.size());
  for (const auto& collision: analysis.selfCollisions)
  {
    selfCollisions.push_back(toFootprintSelfCollision(collision));
  }

  return {
    analysis.usableAddressCount,
    static_cast<uint64_t>(analysis.allocations.size()),
    analysis.collisionProbability,
    std::move(selfCollisions),
  };
}

/// Converts an Ether port kind into its footprint equivalent
[[nodiscard]] kernel::NetworkFootprintPortKind toFootprintPortKind(PortKind kind)
{
  switch (kind)
  {
    case PortKind::tcpAcceptor:
      return kernel::NetworkFootprintPortKind::tcpAcceptor;
    case PortKind::udpUnicast:
      return kernel::NetworkFootprintPortKind::udpUnicast;
    case PortKind::tcpSource:
      return kernel::NetworkFootprintPortKind::tcpSource;
  }
  sen::throwRuntimeError("unknown port kind");
}

[[nodiscard]] kernel::NetworkFootprintPortMode toFootprintPortMode(const PortBinding& binding)
{
  return std::visit(
    ::sen::Overloaded {
      [](const Ephemeral&) { return kernel::NetworkFootprintPortMode::ephemeral; },
      [](const PinnedPort&) { return kernel::NetworkFootprintPortMode::pinned; },
      [](const ProbePortRange&) { return kernel::NetworkFootprintPortMode::probe; },
    },
    binding);
}

/// Converts a configured port binding into a footprint port entry.
[[nodiscard]] kernel::NetworkFootprintPort toFootprintPort(PortKind kind, const PortBinding& binding)
{
  auto value = std::visit(
    ::sen::Overloaded {
      [](const Ephemeral&) -> kernel::MaybeNetworkFootprintPortValue { return {}; },
      [](const PinnedPort& pinnedPort) -> kernel::MaybeNetworkFootprintPortValue
      { return kernel::NetworkFootprintPortValue {pinnedPort.port}; },
      [](const ProbePortRange& probeRange) -> kernel::MaybeNetworkFootprintPortValue
      {
        return kernel::NetworkFootprintPortValue {kernel::NetworkFootprintPortRange {probeRange.min, probeRange.max}};
      },
    },
    binding);
  return {toFootprintPortKind(kind), toFootprintPortMode(binding), std::move(value)};
}

[[nodiscard]] kernel::NetworkFootprintPort toRuntimeFootprintPort(PortKind kind,
                                                                  const PortBinding& binding,
                                                                  uint16_t port)
{
  return {
    toFootprintPortKind(kind),
    toFootprintPortMode(binding),
    kernel::NetworkFootprintPortValue {port},
  };
}

/// Converts one source of port exclusions into footprint ranges.
template <typename Tag>
[[nodiscard]] kernel::NetworkFootprintPortRangeList toFootprintPortRanges(const ExclusionSet<uint16_t, Tag>& exclusions)
{
  kernel::NetworkFootprintPortRangeList footprintRanges;
  footprintRanges.reserve(exclusions.ranges().size());
  for (const auto& range: exclusions.ranges())
  {
    footprintRanges.push_back({range.min, range.max});
  }
  return footprintRanges;
}

/// Converts all port exclusion sources into the footprint model.
[[nodiscard]] kernel::NetworkFootprintPortExclusions toFootprintPortExclusions(const PortExclusionSources& exclusions)
{
  return {
    toFootprintPortRanges(exclusions.builtIn),
    toFootprintPortRanges(exclusions.configured),
    toFootprintPortRanges(exclusions.os),
  };
}

/// Analyzes the reported buses and builds the multicast footprint section.
[[nodiscard]] kernel::NetworkFootprintMulticast buildMulticastFootprint(
  const std::vector<kernel::BusAddress>& busAddresses,
  Span<const kernel::BusAddress> configuredBusAddresses,
  uint16_t discoveryPort,
  const BusConfig& config,
  const MulticastExclusions& exclusions)
{
  const auto analysisResult =
    analyzeConfiguredMulticastBuses(busAddresses, discoveryPort, config.multicastRange, exclusions);
  if (analysisResult.isError())
  {
    sen::throwRuntimeError(analysisResult.getError());
  }
  const auto& analysis = analysisResult.getValue();

  kernel::NetworkFootprintBusList buses;
  buses.reserve(analysis.allocations.size());
  for (const auto& allocation: analysis.allocations)
  {
    buses.push_back(toFootprintBus(allocation, getBusSource(allocation.busAddress, configuredBusAddresses)));
  }

  return {
    config.multicastPort,
    std::move(buses),
    toFootprintAddressRanges(exclusions),
    toFootprintCollision(analysis),
  };
}

/// Builds the port entries from the effective ether configuration.
[[nodiscard]] kernel::NetworkFootprintPortList buildPortFootprints(const Configuration& config)
{
  kernel::NetworkFootprintPortList ports;
  ports.reserve(3U);
  ports.push_back(toFootprintPort(PortKind::tcpAcceptor, getPortBinding(config, PortKind::tcpAcceptor)));
  ports.push_back(toFootprintPort(PortKind::udpUnicast, getPortBinding(config, PortKind::udpUnicast)));
  ports.push_back(toFootprintPort(PortKind::tcpSource, getPortBinding(config, PortKind::tcpSource)));
  return ports;
}

}  // namespace

kernel::NetworkFootprint makeNetworkFootprint(Span<const kernel::BusAddress> configuredBusAddresses,
                                              Span<const kernel::BusAddress> suppliedBusAddresses,
                                              const Configuration& config,
                                              const NetworkExclusions& exclusions)
{
  const auto discoveryPort = getBusDiscoveryPort(config);
  kernel::MaybeNetworkFootprintMulticast multicast;
  if (!config.busConfig.multicastDisabled)
  {
    const auto busAddresses = mergeBusAddresses(configuredBusAddresses, suppliedBusAddresses);
    multicast = buildMulticastFootprint(
      busAddresses, configuredBusAddresses, discoveryPort, config.busConfig, exclusions.multicast);
  }

  return {
    discoveryPort,
    std::move(multicast),
    buildPortFootprints(config),
    toFootprintPortExclusions(exclusions.ports),
  };
}

RuntimeNetworkFootprintState::RuntimeNetworkFootprintState(Configuration config, NetworkExclusions exclusions)
  : config_(std::move(config)), exclusions_(std::move(exclusions))
{
}

RuntimeFootprintTransportId RuntimeNetworkFootprintState::addTransport(std::string sessionName, uint32_t sessionId)
{
  std::scoped_lock lock(mutex_);
  const auto transportId = nextTransportId_++;
  RuntimeTransport transport;
  transport.sessionName = std::move(sessionName);
  transport.sessionId = sessionId;
  transports_.emplace(transportId, std::move(transport));
  return transportId;
}

void RuntimeNetworkFootprintState::removeTransport(RuntimeFootprintTransportId transportId)
{
  if (transportId == invalidRuntimeFootprintTransportId)
  {
    return;
  }
  std::scoped_lock lock(mutex_);
  transports_.erase(transportId);
}

void RuntimeNetworkFootprintState::addBus(RuntimeFootprintTransportId transportId,
                                          uint32_t busId,
                                          std::string busName,
                                          asio::ip::address_v4 groupAddress)
{
  std::scoped_lock lock(mutex_);
  if (auto transportItr = transports_.find(transportId); transportItr != transports_.end())
  {
    transportItr->second.buses.insert_or_assign(busId, RuntimeBus {busId, std::move(busName), groupAddress});
  }
}

void RuntimeNetworkFootprintState::removeBus(RuntimeFootprintTransportId transportId, uint32_t busId)
{
  std::scoped_lock lock(mutex_);
  if (auto transportItr = transports_.find(transportId); transportItr != transports_.end())
  {
    transportItr->second.buses.erase(busId);
  }
}

RuntimeFootprintPortId RuntimeNetworkFootprintState::addPort(RuntimeFootprintTransportId transportId,
                                                             PortKind kind,
                                                             uint16_t port)
{
  std::scoped_lock lock(mutex_);
  auto transportItr = transports_.find(transportId);
  if (transportItr == transports_.end())
  {
    return invalidRuntimeFootprintPortId;
  }

  const auto portId = nextPortId_++;
  transportItr->second.ports.emplace(portId, RuntimePort {kind, port});
  return portId;
}

void RuntimeNetworkFootprintState::removePort(RuntimeFootprintTransportId transportId, RuntimeFootprintPortId portId)
{
  if (portId == invalidRuntimeFootprintPortId)
  {
    return;
  }
  std::scoped_lock lock(mutex_);
  if (auto transportItr = transports_.find(transportId); transportItr != transports_.end())
  {
    transportItr->second.ports.erase(portId);
  }
}

kernel::NetworkFootprint RuntimeNetworkFootprintState::snapshot() const
{
  std::vector<ConfiguredBusMulticastAllocation> allocations;
  std::vector<RuntimePort> runtimePorts;
  {
    std::scoped_lock lock(mutex_);
    for (const auto& [transportId, transport]: transports_)
    {
      std::ignore = transportId;
      for (const auto& [busId, bus]: transport.buses)
      {
        std::ignore = busId;
        allocations.push_back({
          {transport.sessionName, bus.busName},
          transport.sessionId,
          bus.busId,
          bus.groupAddress,
        });
      }
      for (const auto& [portId, port]: transport.ports)
      {
        std::ignore = portId;
        runtimePorts.push_back(port);
      }
    }
  }

  std::sort(allocations.begin(),
            allocations.end(),
            [](const auto& lhs, const auto& rhs)
            {
              return std::tie(lhs.busAddress.sessionName, lhs.busAddress.busName, lhs.sessionId, lhs.busId) <
                     std::tie(rhs.busAddress.sessionName, rhs.busAddress.busName, rhs.sessionId, rhs.busId);
            });
  std::sort(runtimePorts.begin(),
            runtimePorts.end(),
            [](const auto& lhs, const auto& rhs)
            { return std::tie(lhs.kind, lhs.port) < std::tie(rhs.kind, rhs.port); });

  kernel::MaybeNetworkFootprintMulticast multicast;
  if (!config_.busConfig.multicastDisabled)
  {
    const auto usableAddressCount =
      usableMulticastAddressCount(config_.busConfig.multicastRange, exclusions_.multicast);
    const auto analysis = analyzeMulticastAllocations(std::move(allocations), usableAddressCount);
    kernel::NetworkFootprintBusList buses;
    buses.reserve(analysis.allocations.size());
    for (const auto& allocation: analysis.allocations)
    {
      buses.push_back(toFootprintBus(allocation, kernel::NetworkFootprintBusSource::runtime));
    }
    multicast = kernel::NetworkFootprintMulticast {
      config_.busConfig.multicastPort,
      std::move(buses),
      toFootprintAddressRanges(exclusions_.multicast),
      toFootprintCollision(analysis),
    };
  }

  kernel::NetworkFootprintPortList ports;
  ports.reserve(runtimePorts.size());
  for (const auto& runtimePort: runtimePorts)
  {
    ports.push_back(
      toRuntimeFootprintPort(runtimePort.kind, getPortBinding(config_, runtimePort.kind), runtimePort.port));
  }

  return {
    getBusDiscoveryPort(config_),
    std::move(multicast),
    std::move(ports),
    toFootprintPortExclusions(exclusions_.ports),
  };
}

}  // namespace sen::components::ether
