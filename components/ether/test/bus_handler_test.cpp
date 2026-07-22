// === bus_handler_test.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "bus_handler.h"

// generated code
#include "stl/configuration.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// gtest
#include <gtest/gtest.h>

// asio
#include <asio/ip/address_v4.hpp>

// std
#include <cmath>
#include <string>
#include <vector>

namespace sen::components::ether
{

namespace
{

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

}  // namespace

/// @test
/// Detects a collision and computes its probability over the usable address space.
/// @requirements(SEN-909)
TEST(BusHandler, LowCollisionRisk)
{
  constexpr double warningThreshold = 0.05;
  const MulticastRange range {ByteRange {239, 239}, ByteRange {192, 192}, ByteRange {0, 0}, ByteRange {0, 19}};
  const std::vector<kernel::BusAddress> configuredBusAddresses {{"session", "first"}, {"session", "second"}};

  const auto result = analyzeConfiguredMulticastBuses(configuredBusAddresses, 15000, range, {});

  ASSERT_TRUE(result.isOk());
  const auto& analysis = result.getValue();
  ASSERT_EQ(analysis.allocations.size(), 2U);
  EXPECT_EQ(analysis.usableAddressCount, 20U);
  EXPECT_TRUE(analysis.selfCollisions.empty());
  EXPECT_NE(analysis.allocations.at(0).groupAddress, analysis.allocations.at(1).groupAddress);
  EXPECT_NEAR(analysis.collisionProbability, 1.0 - std::exp(-1.0 / 20.0), 1e-12);
  EXPECT_LT(analysis.collisionProbability, warningThreshold);
}

/// @test
/// Detects a collision and computes its probability over the usable address space.
/// @requirements(SEN-909)
TEST(BusHandler, DetectsMulticastCollision)
{
  const auto range = makeSingleAddressRange("239.192.0.10");
  const std::vector<kernel::BusAddress> configuredBusAddresses {
    {"session", "first"},
    {"session", "second"},
    {"session", "first"},
  };

  const auto result = analyzeConfiguredMulticastBuses(configuredBusAddresses, 15000, range, {});

  ASSERT_TRUE(result.isOk());
  const auto& analysis = result.getValue();
  ASSERT_EQ(analysis.allocations.size(), 2U);
  ASSERT_EQ(analysis.selfCollisions.size(), 1U);
  EXPECT_EQ(analysis.usableAddressCount, 1U);
  EXPECT_NEAR(analysis.collisionProbability, 1.0 - std::exp(-1.0), 1e-12);
  EXPECT_EQ(analysis.allocations.at(0).groupAddress, asio::ip::make_address_v4("239.192.0.10"));
  EXPECT_EQ(analysis.allocations.at(1).groupAddress, asio::ip::make_address_v4("239.192.0.10"));

  const auto message = formatMulticastSelfCollision(analysis.selfCollisions.front());
  EXPECT_NE(message.find("session.first"), std::string::npos);
  EXPECT_NE(message.find("session.second"), std::string::npos);
  EXPECT_NE(message.find("239.192.0.10"), std::string::npos);
  EXPECT_NE(message.find("sessionId="), std::string::npos);
  EXPECT_NE(message.find("busId="), std::string::npos);
}

/// @test
/// Reports an error when exclusions remove the complete multicast range.
/// @requirements(SEN-909)
TEST(BusHandler, RejectsFullyExcludedRange)
{
  const auto range = makeSingleAddressRange("239.192.0.10");
  const auto onlyAddress = asio::ip::make_address_v4("239.192.0.10").to_uint();
  MulticastExclusions exclusions;
  exclusions.add(onlyAddress, onlyAddress);

  const auto result =
    analyzeConfiguredMulticastBuses({kernel::BusAddress {"session", "bus"}}, 15000, range, exclusions);

  ASSERT_TRUE(result.isError());
  EXPECT_NE(result.getError().find("no usable addresses"), std::string::npos);
}

}  // namespace sen::components::ether
