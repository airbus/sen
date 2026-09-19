// === network_footprint_test.cpp ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "network_footprint.h"

// component
#include "network_exclusion.h"
#include "port_binding.h"

// sen
#include "sen/core/base/hash32.h"

// generated code
#include "stl/configuration.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/network_footprint.stl.h"

// gtest
#include <gtest/gtest.h>

// asio
#include <asio/ip/address_v4.hpp>

// std
#include <cmath>
#include <cstdint>
#include <exception>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace sen::components::ether
{

namespace
{

constexpr uint16_t discoveryPort = 15000;
constexpr uint16_t multicastPort = 16000;
constexpr uint16_t probePortMin = 21000;
constexpr uint16_t probePortMax = 21999;
constexpr uint16_t pinnedPort = 25000;
constexpr const char* multicastAddress = "239.192.0.10";

[[nodiscard]] MulticastRange makeSingleAddressRange(const std::string& address)
{
  const auto bytes = asio::ip::make_address_v4(address).to_bytes();
  return {
    ByteRange {bytes.at(0), bytes.at(0)},
    ByteRange {bytes.at(1), bytes.at(1)},
    ByteRange {bytes.at(2), bytes.at(2)},
    ByteRange {bytes.at(3), bytes.at(3)},
  };
}

[[nodiscard]] Configuration makeConfiguration(bool multicastDisabled = false)
{
  Configuration config {};
  MulticastDiscovery discovery {};
  discovery.port = discoveryPort;
  config.discovery = std::move(discovery);
  config.busConfig.multicastRange = makeSingleAddressRange(multicastAddress);
  config.busConfig.multicastPort = multicastPort;
  config.busConfig.multicastDisabled = multicastDisabled;
  config.portConfig.emplace(PortConfig {
    Ephemeral {},
    ProbePortRange {probePortMin, probePortMax},
    PinnedPort {pinnedPort},
  });
  return config;
}

[[nodiscard]] NetworkExclusions makeExclusions()
{
  NetworkExclusions exclusions;
  exclusions.multicast.add(asio::ip::make_address_v4("239.192.0.20").to_uint(),
                           asio::ip::make_address_v4("239.192.0.21").to_uint());
  exclusions.ports.builtIn.add(0, 1023);
  exclusions.ports.configured.add(21001, 21002);
  exclusions.ports.os.add(49152, 65535);
  return exclusions;
}

}  // namespace

/// @test
/// Checks that the footprint contains configured network and conflicts.
/// @requirements(SEN-909)
TEST(NetworkFootprint, BuildsNetworkFootprint)
{
  const auto config = makeConfiguration();
  const auto exclusions = makeExclusions();
  const std::vector<kernel::BusAddress> configuredBusAddresses {
    {"session", "configured"},
    {"session", "configured"},
  };
  const std::vector<kernel::BusAddress> suppliedBusAddresses {
    {"session", "configured"},
    {"session", "supplied"},
    {"session", "supplied"},
  };

  const auto footprint = makeNetworkFootprint(configuredBusAddresses, suppliedBusAddresses, config, exclusions);

  EXPECT_EQ(footprint.discoveryPort, discoveryPort);
  ASSERT_TRUE(footprint.multicast.has_value());
  const auto& multicast = footprint.multicast.value();
  EXPECT_EQ(multicast.port, multicastPort);
  ASSERT_EQ(multicast.buses.size(), 2U);

  const auto& configuredBus = multicast.buses.at(0);
  EXPECT_EQ(configuredBus.sessionName, "session");
  EXPECT_EQ(configuredBus.busName, "configured");
  EXPECT_EQ(configuredBus.sessionId, crc32("session"));
  EXPECT_EQ(configuredBus.busId, crc32("configured"));
  EXPECT_EQ(configuredBus.groupAddress, multicastAddress);
  EXPECT_EQ(configuredBus.source, kernel::NetworkFootprintBusSource::configured);

  const auto& suppliedBus = multicast.buses.at(1);
  EXPECT_EQ(suppliedBus.sessionName, "session");
  EXPECT_EQ(suppliedBus.busName, "supplied");
  EXPECT_EQ(suppliedBus.sessionId, crc32("session"));
  EXPECT_EQ(suppliedBus.busId, crc32("supplied"));
  EXPECT_EQ(suppliedBus.groupAddress, multicastAddress);
  EXPECT_EQ(suppliedBus.source, kernel::NetworkFootprintBusSource::supplied);

  ASSERT_EQ(multicast.exclusions.size(), 1U);
  EXPECT_EQ(multicast.exclusions.front().min, "239.192.0.20");
  EXPECT_EQ(multicast.exclusions.front().max, "239.192.0.21");

  EXPECT_EQ(multicast.collision.usableSpaceSize, 1U);
  EXPECT_EQ(multicast.collision.busCount, 2U);
  EXPECT_NEAR(multicast.collision.probability, 1.0 - std::exp(-1.0), 1e-12);
  ASSERT_EQ(multicast.collision.selfCollisions.size(), 1U);
  const auto& collision = multicast.collision.selfCollisions.front();
  EXPECT_EQ(collision.firstBus.sessionName, "session");
  EXPECT_EQ(collision.firstBus.busName, "configured");
  EXPECT_EQ(collision.firstBus.sessionId, crc32("session"));
  EXPECT_EQ(collision.firstBus.busId, crc32("configured"));
  EXPECT_EQ(collision.secondBus.sessionName, "session");
  EXPECT_EQ(collision.secondBus.busName, "supplied");
  EXPECT_EQ(collision.secondBus.sessionId, crc32("session"));
  EXPECT_EQ(collision.secondBus.busId, crc32("supplied"));
  EXPECT_EQ(collision.groupAddress, multicastAddress);

  ASSERT_EQ(footprint.ports.size(), 3U);
  EXPECT_EQ(footprint.ports.at(0).kind, kernel::NetworkFootprintPortKind::tcpAcceptor);
  EXPECT_EQ(footprint.ports.at(0).mode, kernel::NetworkFootprintPortMode::ephemeral);
  EXPECT_FALSE(footprint.ports.at(0).value.has_value());

  EXPECT_EQ(footprint.ports.at(1).kind, kernel::NetworkFootprintPortKind::udpUnicast);
  EXPECT_EQ(footprint.ports.at(1).mode, kernel::NetworkFootprintPortMode::probe);
  ASSERT_TRUE(footprint.ports.at(1).value.has_value());
  const auto& probeRange = std::get<kernel::NetworkFootprintPortRange>(footprint.ports.at(1).value.value());
  EXPECT_EQ(probeRange.min, probePortMin);
  EXPECT_EQ(probeRange.max, probePortMax);

  EXPECT_EQ(footprint.ports.at(2).kind, kernel::NetworkFootprintPortKind::tcpSource);
  EXPECT_EQ(footprint.ports.at(2).mode, kernel::NetworkFootprintPortMode::pinned);
  ASSERT_TRUE(footprint.ports.at(2).value.has_value());
  const auto exactPort = std::get<uint16_t>(footprint.ports.at(2).value.value());
  EXPECT_EQ(exactPort, pinnedPort);

  ASSERT_EQ(footprint.portExclusions.builtIn.size(), 1U);
  EXPECT_EQ(footprint.portExclusions.builtIn.front().min, 0U);
  EXPECT_EQ(footprint.portExclusions.builtIn.front().max, 1023U);
  ASSERT_EQ(footprint.portExclusions.configured.size(), 1U);
  EXPECT_EQ(footprint.portExclusions.configured.front().min, 21001U);
  EXPECT_EQ(footprint.portExclusions.configured.front().max, 21002U);
  ASSERT_EQ(footprint.portExclusions.operatingSystem.size(), 1U);
  EXPECT_EQ(footprint.portExclusions.operatingSystem.front().min, 49152U);
  EXPECT_EQ(footprint.portExclusions.operatingSystem.front().max, 65535U);
}

/// @test
/// Omits multicast and reports ephemeral ports when no port configuration is present
/// @requirements(SEN-909)
TEST(NetworkFootprint, OmitsDisabledMulticast)
{
  auto config = makeConfiguration(true);
  config.portConfig.reset();
  const auto exclusions = makeExclusions();
  const std::vector<kernel::BusAddress> suppliedBusAddresses {{"session", "bus"}};

  const auto footprint = makeNetworkFootprint({}, suppliedBusAddresses, config, exclusions);

  EXPECT_EQ(footprint.discoveryPort, discoveryPort);
  EXPECT_FALSE(footprint.multicast.has_value());
  ASSERT_EQ(footprint.ports.size(), 3U);
  for (const auto& port: footprint.ports)
  {
    EXPECT_EQ(port.mode, kernel::NetworkFootprintPortMode::ephemeral);
    EXPECT_FALSE(port.value.has_value());
  }
  EXPECT_EQ(footprint.portExclusions.configured.size(), 1U);
}

/// @test
/// Rejects a multicast configuration with no usable address.
/// @requirements(SEN-909)
TEST(NetworkFootprint, RejectsMulticastWithoutUsableAddress)
{
  const auto config = makeConfiguration();
  NetworkExclusions exclusions;
  const auto onlyAddress = asio::ip::make_address_v4(multicastAddress).to_uint();
  exclusions.multicast.add(onlyAddress, onlyAddress);
  const std::vector<kernel::BusAddress> suppliedBusAddresses {{"session", "bus"}};

  try
  {
    static_cast<void>(makeNetworkFootprint({}, suppliedBusAddresses, config, exclusions));
    FAIL() << "Expected makeNetworkFootprint to throw";
  }
  catch (const std::exception& error)
  {
    EXPECT_NE(std::string(error.what()).find("no usable addresses"), std::string::npos);
  }
}

/// @test
/// Checks that a state with no transports reports no buses and no ports.
/// @requirements(SEN-909)
TEST(RuntimeNetworkFootprint, StartsEmpty)
{
  const RuntimeNetworkFootprintState state(makeConfiguration(), makeExclusions());

  const auto footprint = state.snapshot();

  EXPECT_TRUE(footprint.ports.empty());
  ASSERT_TRUE(footprint.multicast.has_value());
  EXPECT_TRUE(footprint.multicast.value().buses.empty());
}

/// @test
/// Checks that a live transport's bus and ports reach the snapshot, marked as runtime.
/// @requirements(SEN-909)
TEST(RuntimeNetworkFootprint, ReportsTheBusesAndPortsOfALiveTransport)
{
  RuntimeNetworkFootprintState state(makeConfiguration(), makeExclusions());

  const auto transportId = state.addTransport("session", 7U);
  state.addBus(transportId, 3U, "bus", asio::ip::make_address_v4(multicastAddress));
  std::ignore = state.addPort(transportId, PortKind::tcpAcceptor, pinnedPort);

  const auto footprint = state.snapshot();

  ASSERT_EQ(footprint.ports.size(), 1U);
  ASSERT_TRUE(footprint.multicast.has_value());
  ASSERT_EQ(footprint.multicast.value().buses.size(), 1U);

  const auto& bus = footprint.multicast.value().buses.front();
  EXPECT_EQ(bus.sessionName, "session");
  EXPECT_EQ(bus.busName, "bus");
  EXPECT_EQ(bus.sessionId, 7U);
  EXPECT_EQ(bus.busId, 3U);
  EXPECT_EQ(bus.source, kernel::NetworkFootprintBusSource::runtime);
}

/// @test
/// Checks that removing a transport drops its entries, so a stopped transport leaves nothing behind.
/// @requirements(SEN-909)
TEST(RuntimeNetworkFootprint, ForgetsARemovedTransport)
{
  RuntimeNetworkFootprintState state(makeConfiguration(), makeExclusions());

  const auto transportId = state.addTransport("session", 7U);
  state.addBus(transportId, 3U, "bus", asio::ip::make_address_v4(multicastAddress));
  std::ignore = state.addPort(transportId, PortKind::tcpAcceptor, pinnedPort);
  ASSERT_FALSE(state.snapshot().ports.empty());

  state.removeTransport(transportId);

  const auto footprint = state.snapshot();
  EXPECT_TRUE(footprint.ports.empty());
  ASSERT_TRUE(footprint.multicast.has_value());
  EXPECT_TRUE(footprint.multicast.value().buses.empty());
}

/// @test
/// Checks that removing an already-removed transport is harmless. The transport is removed once on
/// stop and again in the destructor, so this pair really happens.
/// @requirements(SEN-909)
TEST(RuntimeNetworkFootprint, ToleratesRemovingATransportTwice)
{
  RuntimeNetworkFootprintState state(makeConfiguration(), makeExclusions());

  const auto keptId = state.addTransport("kept", 1U);
  state.addBus(keptId, 1U, "keptBus", asio::ip::make_address_v4(multicastAddress));

  const auto goneId = state.addTransport("gone", 2U);
  // The gone transport needs a bus of its own, or the assertion below holds whether or not the
  // removal happened.
  state.addBus(goneId, 2U, "goneBus", asio::ip::make_address_v4(multicastAddress));
  std::ignore = state.addPort(goneId, PortKind::tcpAcceptor, pinnedPort);
  state.removeTransport(goneId);
  state.removeTransport(goneId);
  state.removeTransport(invalidRuntimeFootprintTransportId);

  const auto footprint = state.snapshot();
  EXPECT_TRUE(footprint.ports.empty());
  ASSERT_TRUE(footprint.multicast.has_value());
  ASSERT_EQ(footprint.multicast.value().buses.size(), 1U);
  EXPECT_EQ(footprint.multicast.value().buses.front().sessionName, "kept");
}

/// @test
/// Checks that removing one port leaves the transport's other ports reported.
/// @requirements(SEN-909)
TEST(RuntimeNetworkFootprint, RemovesOnlyTheNamedPort)
{
  RuntimeNetworkFootprintState state(makeConfiguration(), makeExclusions());

  const auto transportId = state.addTransport("session", 7U);
  const auto firstPortId = state.addPort(transportId, PortKind::tcpAcceptor, pinnedPort);
  std::ignore = state.addPort(transportId, PortKind::udpUnicast, probePortMin);
  ASSERT_EQ(state.snapshot().ports.size(), 2U);

  state.removePort(transportId, firstPortId);

  EXPECT_EQ(state.snapshot().ports.size(), 1U);
}

}  // namespace sen::components::ether
