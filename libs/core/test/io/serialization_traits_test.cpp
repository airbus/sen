// === serialization_traits_test.cpp ===================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/io/detail/serialization_traits.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <istream>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

using sen::impl::allowsContiguousIO;
using sen::impl::isBasic;
using sen::impl::isNumeric;
using sen::impl::isPureIntegral;
using sen::impl::IsStreamable;

struct TrivialType
{
  int32_t x;
  int32_t y;
};

/// @test
/// Check pure integral types
/// @requirements(SEN-1052)
TEST(SerializationTraitss, pureInts)
{
  EXPECT_TRUE(isPureIntegral<uint8_t>());
  EXPECT_TRUE(isPureIntegral<unsigned char>());
  EXPECT_TRUE(isPureIntegral<int8_t>());
  EXPECT_TRUE(isPureIntegral<uint16_t>());
  EXPECT_TRUE(isPureIntegral<int16_t>());
  EXPECT_TRUE(isPureIntegral<uint32_t>());
  EXPECT_TRUE(isPureIntegral<int32_t>());
  EXPECT_TRUE(isPureIntegral<uint64_t>());
  EXPECT_TRUE(isPureIntegral<int64_t>());
  EXPECT_FALSE(isPureIntegral<float32_t>());
  EXPECT_FALSE(isPureIntegral<float64_t>());
  EXPECT_FALSE(isPureIntegral<char>());
}

/// @test
/// Check numeric types
/// @requirements(SEN-1052)
TEST(SerializationTraitss, isNumeric)
{
  EXPECT_TRUE(isNumeric<uint8_t>());
  EXPECT_TRUE(isNumeric<unsigned char>());
  EXPECT_TRUE(isNumeric<int8_t>());
  EXPECT_TRUE(isNumeric<uint16_t>());
  EXPECT_TRUE(isNumeric<int16_t>());
  EXPECT_TRUE(isNumeric<uint32_t>());
  EXPECT_TRUE(isNumeric<int32_t>());
  EXPECT_TRUE(isNumeric<uint64_t>());
  EXPECT_TRUE(isNumeric<int64_t>());
  EXPECT_TRUE(isNumeric<float32_t>());
  EXPECT_TRUE(isNumeric<float64_t>());
  EXPECT_FALSE(isNumeric<char>());
}

/// @test
/// Check basic types
/// @requirements(SEN-1052)
TEST(SerializationTraitss, isBasic)
{
  EXPECT_TRUE(isBasic<uint8_t>());
  EXPECT_TRUE(isBasic<unsigned char>());
  EXPECT_TRUE(isBasic<int8_t>());
  EXPECT_TRUE(isBasic<uint16_t>());
  EXPECT_TRUE(isBasic<int16_t>());
  EXPECT_TRUE(isBasic<uint32_t>());
  EXPECT_TRUE(isBasic<int32_t>());
  EXPECT_TRUE(isBasic<uint64_t>());
  EXPECT_TRUE(isBasic<int64_t>());
  EXPECT_TRUE(isBasic<float32_t>());
  EXPECT_TRUE(isBasic<float64_t>());
  EXPECT_TRUE(isBasic<char>());
}

/// @test
/// Check which types can be memcopied in a sequence without gaps
/// @requirements(SEN-1052)
TEST(SerializationTraitss, allowsContiguousIO)
{
  EXPECT_TRUE(allowsContiguousIO<uint8_t>());
  EXPECT_TRUE(allowsContiguousIO<int8_t>());
  EXPECT_TRUE(allowsContiguousIO<uint16_t>());
  EXPECT_TRUE(allowsContiguousIO<int16_t>());
  EXPECT_TRUE(allowsContiguousIO<uint32_t>());
  EXPECT_TRUE(allowsContiguousIO<int32_t>());
  EXPECT_TRUE(allowsContiguousIO<int64_t>());
  EXPECT_TRUE(allowsContiguousIO<uint64_t>());
  EXPECT_TRUE(allowsContiguousIO<float32_t>());
  EXPECT_TRUE(allowsContiguousIO<float64_t>());
  EXPECT_TRUE(allowsContiguousIO<char>());
  EXPECT_TRUE(allowsContiguousIO<unsigned char>());
  EXPECT_TRUE(allowsContiguousIO<char*>());
  EXPECT_TRUE(allowsContiguousIO<void*>());
  EXPECT_TRUE(allowsContiguousIO<int16_t*>());

  EXPECT_FALSE(allowsContiguousIO<std::string>());
  EXPECT_FALSE(allowsContiguousIO<std::vector<char>>());
  EXPECT_FALSE(allowsContiguousIO<std::vector<float64_t>>());
  EXPECT_FALSE(allowsContiguousIO<std::vector<std::string>>());
}

/// @test
/// Check if std streams can stream different data types
/// @requirements(SEN-1052)
TEST(SerializationTraitss, isStreamableStdStreams)
{
  {
    auto canStream = IsStreamable<std::iostream, void>();
    EXPECT_FALSE(canStream.input);
    EXPECT_FALSE(canStream.output);
  }

  {
    auto canStream = IsStreamable<std::iostream, void*>();
    EXPECT_TRUE(canStream.input);
    EXPECT_TRUE(canStream.output);
  }

  {
    auto canStream = IsStreamable<std::ostream, std::string>();
    EXPECT_FALSE(canStream.input);
    EXPECT_TRUE(canStream.output);
  }

  {
    auto canStream = IsStreamable<std::istream, char>();
    EXPECT_TRUE(canStream.input);
    EXPECT_FALSE(canStream.output);
  }

  {
    auto canStream = IsStreamable<std::iostream, char*>();
    EXPECT_TRUE(canStream.input);
    EXPECT_TRUE(canStream.output);
  }

  {
    auto canStream = IsStreamable<std::stringstream, char*>();
    EXPECT_TRUE(canStream.input);
    EXPECT_TRUE(canStream.output);
  }

  {
    auto canStream = IsStreamable<std::stringstream, float64_t>();
    EXPECT_TRUE(canStream.input);
    EXPECT_TRUE(canStream.output);
  }

  {
    auto canStream = IsStreamable<std::stringstream, uint64_t>();
    EXPECT_TRUE(canStream.input);
    EXPECT_TRUE(canStream.output);
  }
}
