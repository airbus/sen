// === serialization_traits_test.cpp ===================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/detail/serialization_traits.h"
#include "sen/core/meta/enum_traits.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_traits.h"
#include "sen/core/meta/sequence_traits.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type_traits.h"

// generated code
#include "stl/test_struct_traits.stl.h"
#include "stl/test_variant_traits.stl.h"

// google test
#include <gtest/gtest.h>

// json
#include <nlohmann/json.hpp>

// std
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <istream>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using sen::impl::allowsContiguousIO;
using sen::impl::isBasic;
using sen::impl::isNumeric;
using sen::impl::isPureIntegral;
using sen::impl::IsStreamable;

namespace
{

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

}  // namespace

namespace sen
{

template <>
struct StringConversionTraits<TestEnum8>
{
  static std::string_view toString(TestEnum8 val)
  {
    if (val == TestEnum8::value)
    {
      return "value";
    }
    return "unknown";
  }
  static TestEnum8 fromString(std::string_view val)
  {
    if (val == "value")
    {
      return TestEnum8::value;
    }
    throw std::runtime_error("Invalid enum");
  }
};

}  // namespace sen

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

/// @test
/// Checks JSON string conversion for native numeric types
/// @requirements(SEN-1052)
TEST(SerializationTraitsJsonTest, NativeTypeConversion)
{
  constexpr uint32_t original = 8080U;
  const std::string jsonStr = sen::SerializationTraits<uint32_t>::toJsonString(original);

  const auto jsonObj = nlohmann::json::parse(jsonStr);
  EXPECT_TRUE(jsonObj.is_number_unsigned());
  EXPECT_EQ(jsonObj.get<uint32_t>(), 8080U);

  uint32_t recovered = 0U;
  sen::SerializationTraits<uint32_t>::fromJsonString(jsonStr, recovered);
  EXPECT_EQ(original, recovered);
}

/// @test
/// Checks JSON string conversion for string types
/// @requirements(SEN-1052)
TEST(SerializationTraitsJsonTest, StringTypeConversion)
{
  const std::string original = "String Test";
  const std::string jsonStr = sen::SerializationTraits<std::string>::toJsonString(original);

  const auto jsonObj = nlohmann::json::parse(jsonStr);
  EXPECT_TRUE(jsonObj.is_string());
  EXPECT_EQ(jsonObj.get<std::string>(), "String Test");

  std::string recovered;
  sen::SerializationTraits<std::string>::fromJsonString(jsonStr, recovered);
  EXPECT_EQ(original, recovered);
}

/// @test
/// Checks JSON string conversion for sequence types
/// @requirements(SEN-1052)
TEST(SerializationTraitsJsonTest, SequenceConversion)
{
  const std::vector original = {256, 512, 1024};
  const std::string jsonStr = sen::SequenceTraitsBase<std::vector<int32_t>>::toJsonString(original);

  auto jsonObj = nlohmann::json::parse(jsonStr);
  EXPECT_TRUE(jsonObj.is_array());
  ASSERT_EQ(jsonObj.size(), 3U);
  EXPECT_EQ(jsonObj[0].get<int32_t>(), 256);
  EXPECT_EQ(jsonObj[1].get<int32_t>(), 512);
  EXPECT_EQ(jsonObj[2].get<int32_t>(), 1024);

  std::vector<int32_t> recovered;
  sen::SequenceTraitsBase<std::vector<int32_t>>::fromJsonString(jsonStr, recovered);
  EXPECT_EQ(original, recovered);
}

/// @test
/// Checks JSON string conversion for Duration type
/// @requirements(SEN-1052)
TEST(SerializationTraitsJsonTest, DurationConversion)
{
  const sen::Duration original(5000);
  const std::string jsonStr = sen::SerializationTraits<sen::Duration>::toJsonString(original);

  const auto jsonObj = nlohmann::json::parse(jsonStr);
  EXPECT_TRUE(jsonObj.is_number_integer());
  EXPECT_EQ(jsonObj.get<int64_t>(), 5000);

  sen::Duration recovered;
  sen::SerializationTraits<sen::Duration>::fromJsonString(jsonStr, recovered);
  EXPECT_EQ(original, recovered);
}

/// @test
/// Checks JSON string conversion for TimeStamp type
/// @requirements(SEN-1052)
TEST(SerializationTraitsJsonTest, TimeStampConversion)
{
  const sen::TimeStamp original(sen::Duration(0));
  const std::string jsonStr = sen::SerializationTraits<sen::TimeStamp>::toJsonString(original);

  const auto jsonObj = nlohmann::json::parse(jsonStr);
  EXPECT_TRUE(jsonObj.is_string());
  EXPECT_EQ(jsonObj.get<std::string>(), "1970-01-01 00:00:00 000000");

  sen::TimeStamp recovered;
  sen::SerializationTraits<sen::TimeStamp>::fromJsonString(jsonStr, recovered);

  const auto timeDifference =
    std::abs(original.sinceEpoch().getNanoseconds() - recovered.sinceEpoch().getNanoseconds());
  constexpr auto maxTimezoneOffset =
    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(24)).count();
  EXPECT_LE(timeDifference, maxTimezoneOffset);
}

/// @test
/// Checks JSON string conversion for optional types
/// @requirements(SEN-1052)
TEST(SerializationTraitsJsonTest, OptionalConversion)
{
  constexpr std::optional originalWithValue = 128;
  const std::string jsonStrWithValue = sen::OptionalTraitsBase<std::optional<int32_t>>::toJsonString(originalWithValue);

  auto jsonObjWithValue = nlohmann::json::parse(jsonStrWithValue);
  EXPECT_TRUE(jsonObjWithValue.is_number_integer());
  EXPECT_EQ(jsonObjWithValue.get<int32_t>(), 128);

  std::optional<int32_t> recoveredWithValue;
  sen::OptionalTraitsBase<std::optional<int32_t>>::fromJsonString(jsonStrWithValue, recoveredWithValue);
  EXPECT_TRUE(recoveredWithValue.has_value());
  EXPECT_EQ(originalWithValue.value(), recoveredWithValue.value());

  constexpr std::optional<int32_t> originalEmpty = std::nullopt;
  const std::string jsonStrEmpty = sen::OptionalTraitsBase<std::optional<int32_t>>::toJsonString(originalEmpty);

  auto jsonObjEmpty = nlohmann::json::parse(jsonStrEmpty);
  EXPECT_TRUE(jsonObjEmpty.is_null());

  std::optional<int32_t> recoveredEmpty;
  sen::OptionalTraitsBase<std::optional<int32_t>>::fromJsonString(jsonStrEmpty, recoveredEmpty);
  EXPECT_FALSE(recoveredEmpty.has_value());
}

/// @test
/// Checks JSON string conversion for enum types
/// @requirements(SEN-1052)
TEST(SerializationTraitsJsonTest, EnumConversion)
{
  constexpr auto original = TestEnum8::value;
  const std::string jsonStr = sen::EnumTraitsBase<TestEnum8>::toJsonString(original);

  const auto jsonObj = nlohmann::json::parse(jsonStr);
  EXPECT_TRUE(jsonObj.is_number_unsigned());
  EXPECT_EQ(jsonObj.get<uint8_t>(), static_cast<uint8_t>(TestEnum8::value));

  TestEnum8 recovered;
  sen::EnumTraitsBase<TestEnum8>::fromJsonString(jsonStr, recovered);
  EXPECT_EQ(original, recovered);
}

/// @test
/// Checks JSON string conversion for array and sequence types
/// @requirements(SEN-1052)
TEST(SerializationTraitsJsonTest, ArrayConversion)
{
  constexpr std::array<int32_t, 3> original = {192, 168, 1};
  const std::string jsonStr = sen::ArrayTraitsBase<std::array<int32_t, 3>>::toJsonString(original);

  auto jsonObj = nlohmann::json::parse(jsonStr);
  EXPECT_TRUE(jsonObj.is_array());
  ASSERT_EQ(jsonObj.size(), 3U);
  EXPECT_EQ(jsonObj[1].get<int32_t>(), 168);

  std::array<int32_t, 3> recovered {};
  sen::ArrayTraitsBase<std::array<int32_t, 3>>::fromJsonString(jsonStr, recovered);
  EXPECT_EQ(original, recovered);
}

/// @test
/// Checks JSON string conversion for structs
/// @requirements(SEN-1052)
TEST(SerializationTraitsJsonTest, StructConversion)
{
  test_struct_traits::MyStructWithNativeFieldsOnly original;
  original.field1 = 4096U;
  original.field2 = "StructTest";
  original.field3 = 3.14f;

  const std::string jsonStr =
    sen::SerializationTraits<test_struct_traits::MyStructWithNativeFieldsOnly>::toJsonString(original);

  auto jsonObj = nlohmann::json::parse(jsonStr);
  EXPECT_TRUE(jsonObj.is_object());
  EXPECT_EQ(jsonObj["field1"].get<uint32_t>(), 4096U);
  EXPECT_EQ(jsonObj["field2"].get<std::string>(), "StructTest");
  EXPECT_FLOAT_EQ(jsonObj["field3"].get<float>(), 3.14f);

  test_struct_traits::MyStructWithNativeFieldsOnly recovered;
  sen::SerializationTraits<test_struct_traits::MyStructWithNativeFieldsOnly>::fromJsonString(jsonStr, recovered);

  EXPECT_EQ(original.field1, recovered.field1);
  EXPECT_EQ(original.field2, recovered.field2);
  EXPECT_FLOAT_EQ(original.field3, recovered.field3);
}

/// @test
/// Checks JSON string conversion for variants
/// @requirements(SEN-1052)
TEST(SerializationTraitsJsonTest, VariantConversion)
{
  test_variant_traits::MockVariant original;
  original.emplace<2>(9999U);

  const std::string jsonStr = sen::SerializationTraits<test_variant_traits::MockVariant>::toJsonString(original);

  auto jsonObj = nlohmann::json::parse(jsonStr);
  EXPECT_TRUE(jsonObj.is_object());
  EXPECT_EQ(jsonObj["type"].get<uint32_t>(), 2U);
  EXPECT_EQ(jsonObj["value"].get<uint32_t>(), 9999U);

  test_variant_traits::MockVariant recovered;
  sen::SerializationTraits<test_variant_traits::MockVariant>::fromJsonString(jsonStr, recovered);

  ASSERT_EQ(recovered.index(), 2U);
  EXPECT_EQ(std::get<2>(recovered), 9999U);
}
