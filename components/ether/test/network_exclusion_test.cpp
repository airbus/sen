// === network_exclusion_test.cpp ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "network_exclusion.h"

// gtest
#include <gtest/gtest.h>

// asio
#include <asio/ip/address_v4.hpp>

// generated code
#include "stl/configuration.stl.h"

// std
#include <cstdint>
#include <string>

namespace sen::components::ether
{

namespace
{

[[nodiscard]] MulticastRange makeRange(uint8_t byte1Min,
                                       uint8_t byte1Max,
                                       uint8_t byte2Min,
                                       uint8_t byte2Max,
                                       uint8_t byte3Min,
                                       uint8_t byte3Max)
{
  return {
    ByteRange {239, 239},
    ByteRange {byte1Min, byte1Max},
    ByteRange {byte2Min, byte2Max},
    ByteRange {byte3Min, byte3Max},
  };
}

}  // namespace

/// @test
/// Merges overlapping and adjacent ranges.
/// @requirements(SEN-909)
TEST(NetworkExclusion, MergesRanges)
{
  ConfiguredPortExclusions exclusions;
  exclusions.add(20, 30);
  exclusions.add(10, 15);
  exclusions.add(16, 19);
  exclusions.add(25, 40);

  ASSERT_EQ(exclusions.ranges().size(), 1U);
  EXPECT_EQ(exclusions.ranges().front().min, 10);
  EXPECT_EQ(exclusions.ranges().front().max, 40);

  ConfiguredPortExclusions upperLimitExclusions;
  upperLimitExclusions.add(65534, 65535);
  upperLimitExclusions.add(65533, 65533);

  ASSERT_EQ(upperLimitExclusions.ranges().size(), 1U);
  EXPECT_EQ(upperLimitExclusions.ranges().front().min, 65533);
  EXPECT_EQ(upperLimitExclusions.ranges().front().max, 65535);
}

/// @test
/// Keeps separate ranges sorted.
/// @requirements(SEN-909)
TEST(NetworkExclusion, KeepsRangesSeparate)
{
  ConfiguredPortExclusions exclusions;
  exclusions.add(30, 40);
  exclusions.add(10, 20);
  exclusions.add(50, 60);

  ASSERT_EQ(exclusions.ranges().size(), 3U);
  EXPECT_EQ(exclusions.ranges().at(0).min, 10);
  EXPECT_EQ(exclusions.ranges().at(1).min, 30);
  EXPECT_EQ(exclusions.ranges().at(2).min, 50);
  EXPECT_TRUE(exclusions.isExcluded(35));
  EXPECT_FALSE(exclusions.isExcluded(45));
}

/// @test
/// Finds the next value that is not excluded.
/// @requirements(SEN-909)
TEST(NetworkExclusion, FindsNextValue)
{
  ConfiguredPortExclusions exclusions;
  exclusions.add(10, 12);

  const auto result = exclusions.nextUsable(
    10, 4, [](uint16_t value) { return value == 12 ? static_cast<uint16_t>(9) : static_cast<uint16_t>(value + 1); });

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, 9);
}

/// @test
/// Returns no value when every candidate is excluded.
/// @requirements(SEN-909)
TEST(NetworkExclusion, ReturnsNoValueWhenExhausted)
{
  ConfiguredPortExclusions exclusions;
  exclusions.add(10, 12);

  const auto result = exclusions.nextUsable(
    10, 3, [](uint16_t value) { return value == 12 ? static_cast<uint16_t>(10) : static_cast<uint16_t>(value + 1); });

  EXPECT_FALSE(result.has_value());
}

/// @test
/// Loads built-in and configured exclusions.
/// @requirements(SEN-909)
TEST(NetworkExclusion, LoadsExclusions)
{
  Configuration config {};
  config.busConfig.multicastExclusions.push_back({"239.200.0.1", "239.200.0.10"});
  config.portExclusions.push_back({4000, 4010});

  const auto result = makeNetworkExclusions(config);

  ASSERT_TRUE(result.isOk());
  const auto& exclusions = result.getValue();
  EXPECT_TRUE(exclusions.multicast.isExcluded(asio::ip::make_address_v4("239.195.255.1").to_uint()));
  EXPECT_TRUE(exclusions.multicast.isExcluded(asio::ip::make_address_v4("239.255.255.250").to_uint()));
  EXPECT_TRUE(exclusions.multicast.isExcluded(asio::ip::make_address_v4("239.200.0.5").to_uint()));
  EXPECT_TRUE(exclusions.ports.builtIn.isExcluded(443));
  EXPECT_TRUE(exclusions.ports.configured.isExcluded(4005));
}

/// @test
/// Loads port ranges reported by the operating system.
/// @requirements(SEN-909)
TEST(NetworkExclusion, LoadsOsPortExclusions)
{
  Configuration config {};

  const auto result = makeNetworkExclusions(config);

  ASSERT_TRUE(result.isOk());
  const auto& osRanges = result.getValue().ports.os.ranges();
  ASSERT_FALSE(osRanges.empty());

  for (const auto& range: osRanges)
  {
    EXPECT_LE(range.min, range.max);
    EXPECT_GT(range.min, 0);
    EXPECT_TRUE(result.getValue().ports.os.isExcluded(range.min));
    EXPECT_TRUE(result.getValue().ports.os.isExcluded(range.max));
  }
}

/// @test
/// Rejects invalid multicast exclusion ranges.
/// @requirements(SEN-909)
TEST(NetworkExclusion, RejectsInvalidMulticastRanges)
{
  {
    Configuration config {};
    config.busConfig.multicastExclusions.push_back({"238.0.0.0", "238.0.0.255"});

    const auto result = makeNetworkExclusions(config);

    ASSERT_TRUE(result.isError());
    EXPECT_NE(result.getError().find("239.0.0.0/8"), std::string::npos);
  }

  {
    Configuration config {};
    config.busConfig.multicastExclusions.push_back({"invalid", "239.200.0.10"});

    const auto result = makeNetworkExclusions(config);

    ASSERT_TRUE(result.isError());
    EXPECT_NE(result.getError().find("invalid multicast exclusion address"), std::string::npos);
  }

  {
    Configuration config {};
    config.busConfig.multicastExclusions.push_back({"239.200.0.10", "239.200.0.1"});

    const auto result = makeNetworkExclusions(config);

    ASSERT_TRUE(result.isError());
    EXPECT_NE(result.getError().find("min > max"), std::string::npos);
  }
}

/// @test
/// Rejects a port range with the limits reversed.
/// @requirements(SEN-909)
TEST(NetworkExclusion, RejectsInvalidPortRange)
{
  Configuration config {};
  config.portExclusions.push_back({4010, 4000});

  const auto result = makeNetworkExclusions(config);

  ASSERT_TRUE(result.isError());
  EXPECT_NE(result.getError().find("min > max"), std::string::npos);
}

/// @test
/// Finds a usable multicast address across the configured range.
/// @requirements(SEN-909)
TEST(NetworkExclusion, FindsUsableMulticastAddress)
{
  // Skip an excluded address.
  {
    const auto range = makeRange(192, 192, 0, 0, 0, 1);
    const auto initial = asio::ip::make_address_v4("239.192.0.0");

    MulticastExclusions exclusions;
    exclusions.add(initial.to_uint(), initial.to_uint());
    const auto result = getUsableMulticastAddress(initial, range, exclusions);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, asio::ip::make_address_v4("239.192.0.1"));
  }

  // Continue with the next byte.
  {
    const auto range = makeRange(192, 192, 0, 1, 254, 255);
    const auto initial = asio::ip::make_address_v4("239.192.0.255");

    MulticastExclusions exclusions;
    exclusions.add(initial.to_uint(), initial.to_uint());
    const auto result = getUsableMulticastAddress(initial, range, exclusions);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, asio::ip::make_address_v4("239.192.1.254"));
  }

  // Continue from the end of the range to its beginning.
  {
    const auto range = makeRange(192, 192, 0, 0, 0, 1);
    const auto initial = asio::ip::make_address_v4("239.192.0.1");

    MulticastExclusions exclusions;
    exclusions.add(initial.to_uint(), initial.to_uint());
    const auto result = getUsableMulticastAddress(initial, range, exclusions);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, asio::ip::make_address_v4("239.192.0.0"));
  }
}

/// @test
/// Counts usable multicast addresses by intersecting ranges.
/// @requirements(SEN-909)
TEST(NetworkExclusion, CountsUsableMulticastAddresses)
{
  const auto range = makeRange(192, 193, 0, 0, 0, 3);
  MulticastExclusions exclusions;
  exclusions.add(asio::ip::make_address_v4("239.192.0.1").to_uint(),
                 asio::ip::make_address_v4("239.192.0.2").to_uint());
  exclusions.add(asio::ip::make_address_v4("239.193.0.0").to_uint(),
                 asio::ip::make_address_v4("239.193.0.0").to_uint());
  exclusions.add(asio::ip::make_address_v4("239.194.0.0").to_uint(),
                 asio::ip::make_address_v4("239.194.0.255").to_uint());

  EXPECT_EQ(usableMulticastAddressCount(range, exclusions), 5U);
}

/// @test
/// Selects usable multicast addresses by their index.
/// @requirements(SEN-909)
TEST(NetworkExclusion, SelectsAddressByIndex)
{
  const auto range = makeRange(192, 192, 0, 0, 10, 13);
  MulticastExclusions exclusions;
  exclusions.add(asio::ip::make_address_v4("239.192.0.11").to_uint(),
                 asio::ip::make_address_v4("239.192.0.11").to_uint());

  EXPECT_EQ(getUsableMulticastAddressAtIndex(0, range, exclusions), asio::ip::make_address_v4("239.192.0.10"));
  EXPECT_EQ(getUsableMulticastAddressAtIndex(1, range, exclusions), asio::ip::make_address_v4("239.192.0.12"));
  EXPECT_EQ(getUsableMulticastAddressAtIndex(2, range, exclusions), asio::ip::make_address_v4("239.192.0.13"));
  EXPECT_FALSE(getUsableMulticastAddressAtIndex(3, range, exclusions).has_value());
}

/// @test
/// Selects indexed addresses correctly across gaps in the multicast range
/// @requirements(SEN-909)
TEST(NetworkExclusion, SelectsAcrossRangeBlocks)
{
  const auto range = makeRange(192, 192, 0, 1, 254, 255);
  MulticastExclusions exclusions;
  exclusions.add(asio::ip::make_address_v4("239.192.0.255").to_uint(),
                 asio::ip::make_address_v4("239.192.0.255").to_uint());

  EXPECT_EQ(getUsableMulticastAddressAtIndex(0, range, exclusions), asio::ip::make_address_v4("239.192.0.254"));
  EXPECT_EQ(getUsableMulticastAddressAtIndex(1, range, exclusions), asio::ip::make_address_v4("239.192.1.254"));
  EXPECT_EQ(getUsableMulticastAddressAtIndex(2, range, exclusions), asio::ip::make_address_v4("239.192.1.255"));
}

/// @test
/// Returns no multicast address when the complete range is excluded.
/// @requirements(SEN-909)
TEST(NetworkExclusion, ReturnsNoMulticastAddress)
{
  const auto range = makeRange(192, 192, 0, 0, 0, 0);
  MulticastExclusions exclusions;
  const auto onlyAddress = asio::ip::make_address_v4("239.192.0.0").to_uint();
  exclusions.add(onlyAddress, onlyAddress);

  EXPECT_FALSE(getUsableMulticastAddress(asio::ip::make_address_v4(onlyAddress), range, exclusions).has_value());
  EXPECT_FALSE(hasUsableMulticastAddress(range, exclusions));
  EXPECT_EQ(usableMulticastAddressCount(range, exclusions), 0U);
  EXPECT_FALSE(getUsableMulticastAddressAtIndex(0, range, exclusions).has_value());
}

}  // namespace sen::components::ether
