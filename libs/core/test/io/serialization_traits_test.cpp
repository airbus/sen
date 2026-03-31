// === serialization_traits_test.cpp ===================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
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

enum class TestEnum8 : uint8_t
{
  value = 1
};
enum class TestEnum32 : uint32_t
{
  value = 100
};

/// @test
/// Check pure integral types
/// @requirements(SEN-1052)
TEST(SerializationTraits, pureInts)
{
  EXPECT_TRUE(isPureIntegral<uint8_t>);
  EXPECT_TRUE(isPureIntegral<unsigned char>);
  EXPECT_TRUE(isPureIntegral<int8_t>);
  EXPECT_TRUE(isPureIntegral<uint16_t>);
  EXPECT_TRUE(isPureIntegral<int16_t>);
  EXPECT_TRUE(isPureIntegral<uint32_t>);
  EXPECT_TRUE(isPureIntegral<int32_t>);
  EXPECT_TRUE(isPureIntegral<uint64_t>);
  EXPECT_TRUE(isPureIntegral<int64_t>);

  EXPECT_FALSE(isPureIntegral<float32_t>);
  EXPECT_FALSE(isPureIntegral<float64_t>);
  EXPECT_FALSE(isPureIntegral<char>);
  EXPECT_FALSE(isPureIntegral<bool>);
}

/// @test
/// Check numeric types
/// @requirements(SEN-1052)
TEST(SerializationTraits, isNumeric)
{
  EXPECT_TRUE(isNumeric<uint8_t>);
  EXPECT_TRUE(isNumeric<unsigned char>);
  EXPECT_TRUE(isNumeric<int8_t>);
  EXPECT_TRUE(isNumeric<uint16_t>);
  EXPECT_TRUE(isNumeric<int16_t>);
  EXPECT_TRUE(isNumeric<uint32_t>);
  EXPECT_TRUE(isNumeric<int32_t>);
  EXPECT_TRUE(isNumeric<uint64_t>);
  EXPECT_TRUE(isNumeric<int64_t>);
  EXPECT_TRUE(isNumeric<float32_t>);
  EXPECT_TRUE(isNumeric<float64_t>);

  EXPECT_FALSE(isNumeric<char>);
  EXPECT_FALSE(isNumeric<bool>);
}

/// @test
/// Check basic types
/// @requirements(SEN-1052)
TEST(SerializationTraits, isBasic)
{
  EXPECT_TRUE(isBasic<uint8_t>);
  EXPECT_TRUE(isBasic<unsigned char>);
  EXPECT_TRUE(isBasic<int8_t>);
  EXPECT_TRUE(isBasic<uint16_t>);
  EXPECT_TRUE(isBasic<int16_t>);
  EXPECT_TRUE(isBasic<uint32_t>);
  EXPECT_TRUE(isBasic<int32_t>);
  EXPECT_TRUE(isBasic<uint64_t>);
  EXPECT_TRUE(isBasic<int64_t>);
  EXPECT_TRUE(isBasic<float32_t>);
  EXPECT_TRUE(isBasic<float64_t>);
  EXPECT_TRUE(isBasic<char>);

  EXPECT_FALSE(isBasic<std::string>);
  EXPECT_FALSE(isBasic<bool>);
}

/// @test
/// Check which types can be memcopied in a sequence without gaps
/// @requirements(SEN-1052)
TEST(SerializationTraits, allowsContiguousIO)
{
  EXPECT_TRUE(allowsContiguousIO<uint8_t>);
  EXPECT_TRUE(allowsContiguousIO<int8_t>);
  EXPECT_TRUE(allowsContiguousIO<uint16_t>);
  EXPECT_TRUE(allowsContiguousIO<int16_t>);
  EXPECT_TRUE(allowsContiguousIO<uint32_t>);
  EXPECT_TRUE(allowsContiguousIO<int32_t>);
  EXPECT_TRUE(allowsContiguousIO<int64_t>);
  EXPECT_TRUE(allowsContiguousIO<uint64_t>);
  EXPECT_TRUE(allowsContiguousIO<float32_t>);
  EXPECT_TRUE(allowsContiguousIO<float64_t>);
  EXPECT_TRUE(allowsContiguousIO<char>);
  EXPECT_TRUE(allowsContiguousIO<unsigned char>);
  EXPECT_TRUE(allowsContiguousIO<char*>);
  EXPECT_TRUE(allowsContiguousIO<void*>);
  EXPECT_TRUE(allowsContiguousIO<int16_t*>);
  EXPECT_TRUE(allowsContiguousIO<TestEnum8>);

  EXPECT_FALSE(allowsContiguousIO<TrivialType>);
  EXPECT_FALSE(allowsContiguousIO<bool>);
  EXPECT_FALSE(allowsContiguousIO<std::string>);
  EXPECT_FALSE(allowsContiguousIO<std::vector<char>>);
  EXPECT_FALSE(allowsContiguousIO<std::vector<float64_t>>);
  EXPECT_FALSE(allowsContiguousIO<std::vector<std::string>>);
}

/// @test
/// Check if std streams can stream different data types
/// @requirements(SEN-1052)
TEST(SerializationTraits, isStreamableStdStreams)
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

/// @test
/// Check getSerializedSize for boolean values
/// @requirements(SEN-1052)
TEST(SerializationTraits, getSerializedSizeBool)
{
  EXPECT_EQ(sen::impl::getSerializedSize(true), sizeof(sen::impl::BoolTransportType));
  EXPECT_EQ(sen::impl::getSerializedSize(false), sizeof(sen::impl::BoolTransportType));
}

/// @test
/// Check getSerializedSize for basic numeric types
/// @requirements(SEN-1052)
TEST(SerializationTraits, getSerializedSizeBasic)
{
  EXPECT_EQ(sen::impl::getSerializedSize(uint8_t {5}), sizeof(uint8_t));
  EXPECT_EQ(sen::impl::getSerializedSize(int32_t {-10}), sizeof(int32_t));
  EXPECT_EQ(sen::impl::getSerializedSize(float32_t {3.14f}), sizeof(float32_t));
  EXPECT_EQ(sen::impl::getSerializedSize('a'), sizeof(char));
}

/// @test
/// Check getSerializedSize for enumerations
/// @requirements(SEN-1052)
TEST(SerializationTraits, getSerializedSizeEnum)
{
  EXPECT_EQ(sen::impl::getSerializedSize(TestEnum8::value), sizeof(uint8_t));
  EXPECT_EQ(sen::impl::getSerializedSize(TestEnum32::value), sizeof(uint32_t));
}

/// @test
/// Check getSerializedSize for strings
/// @requirements(SEN-1052)
TEST(SerializationTraits, getSerializedSizeString)
{
  const std::string emptyStr;
  constexpr uint32_t sizeSize = sen::impl::getSerializedSize(0U);
  EXPECT_EQ(sen::impl::getSerializedSize(emptyStr), sizeSize);

  const std::string text = "coverage_test";
  EXPECT_EQ(sen::impl::getSerializedSize(text), sizeSize + text.size());
}

/// @test
/// Check getSerializedSize for TimeStamp
/// @requirements(SEN-1052)
TEST(SerializationTraits, getSerializedSizeTimeStamp)
{
  const sen::TimeStamp ts;
  EXPECT_EQ(sen::impl::getSerializedSize(ts), sen::impl::getSerializedSize(int64_t {0}));
}
