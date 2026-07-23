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

/// Converts a configured port binding into a footprint port entry.
[[nodiscard]] kernel::NetworkFootprintPort toFootprintPort(PortKind kind, const PortBinding& binding)
{
  const auto footprintKind = toFootprintPortKind(kind);
  return std::visit(
    ::sen::Overloaded {
      [footprintKind](const Ephemeral&) -> kernel::NetworkFootprintPort
      {
        return {
          footprintKind,
          kernel::NetworkFootprintPortMode::ephemeral,
          kernel::MaybeNetworkFootprintPortValue {},
        };
      },
      [footprintKind](const PinnedPort& pinnedPort) -> kernel::NetworkFootprintPort
      {
        return {
          footprintKind,
          kernel::NetworkFootprintPortMode::pinned,
          kernel::NetworkFootprintPortValue {pinnedPort.port},
        };
      },
      [footprintKind](const ProbePortRange& probeRange) -> kernel::NetworkFootprintPort
      {
        return {
          footprintKind,
          kernel::NetworkFootprintPortMode::probe,
          kernel::NetworkFootprintPortRange {probeRange.min, probeRange.max},
        };
      },
    },
    binding);
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

}  // namespace sen::components::ether
