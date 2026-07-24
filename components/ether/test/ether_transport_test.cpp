// === ether_transport_test.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "discovery.h"
#include "ether_transport.h"
#include "network_exclusion.h"

// generated code
#include "stl/configuration.stl.h"

// google test
#include <gtest/gtest.h>

// asio
#include <asio/io_context.hpp>

// std
#include <chrono>

namespace sen::components::ether
{

/// @test
/// Verifies that stopping the transport cleans up pending timers
/// @requirements(SEN-365, SEN-908)
TEST(EtherTransport, ClearPendingTimers)
{
  Configuration config {};
  config.discovery = TcpDiscovery {};
  NetworkExclusions exclusions;

  asio::io_context io;
  auto discovery = DiscoverySystem::make(config, io);

  EtherTransport transport(config, "etherTransportTest", "etherTransportTest", discovery, nullptr, exclusions);
  transport.start(nullptr);

  auto timerId = transport.startTimer(std::chrono::hours(1), []() {});

  EXPECT_NO_THROW(transport.stop());
  EXPECT_FALSE(transport.cancelTimer(timerId));
}

}  // namespace sen::components::ether
