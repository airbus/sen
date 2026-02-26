// === var_test.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/integer_compare.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/var.h"

// generated code
#include "stl/test_type_traits.stl.h"

// google test
#include <gtest/gtest.h>

// os
#ifdef __linux__
#  include <bits/basic_string.h>
#endif

// std
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

using sen::Duration;
using sen::getCopyAs;
using sen::TimeStamp;
using sen::Var;
using sen::VarList;
using sen::VarMap;

namespace
{
/// \tparam T: the type of the random number to generate
/// \tparam SmallestStorageType: the type into which this number should fit
template <typename T, typename SmallestStorageType>
T randomPositiveNumberAs()
{
  static std::mt19937 rng;

  if constexpr (sizeof(SmallestStorageType) == 1)
  {
    static std::uniform_int_distribution<uint32_t> dist(0, std::numeric_limits<uint8_t>::max());
    return static_cast<T>(dist(rng));
  }
  else
  {
    static std::uniform_int_distribution<SmallestStorageType> dist(0, std::numeric_limits<SmallestStorageType>::max());
    return static_cast<T>(dist(rng));
  }
}

template <typename T>
void checkEmptyVar()
{
  Var var;

  EXPECT_TRUE(var.isEmpty());
  ASSERT_NO_THROW(std::ignore = getCopyAs<std::monostate>(var));

  // T from an empty variant shall return T{}
  EXPECT_EQ(getCopyAs<T>(var), T {});
}

template <typename TargetType, typename T>
void checkNonOverflowingConversion(Var var, T value)  // NOLINT(readability-function-size)
{

  // Wrapper needed to prevent errors from gtest macro expansion
  auto copyAsWrapper = [](Var var) { return getCopyAs<TargetType, /*checkedConversion=*/true>(var); };

  if (sen::std_util::cmp_less(value, std::numeric_limits<TargetType>::lowest()))
  {
    EXPECT_DEBUG_DEATH(
      { EXPECT_EQ(copyAsWrapper(var), std::numeric_limits<TargetType>::lowest()); },
      "Needed to truncate `from` as it's value was to small for ToType.");
  }
  else if (sen::std_util::cmp_greater(value, std::numeric_limits<TargetType>::max()))
  {
    EXPECT_DEBUG_DEATH(
      { EXPECT_EQ(copyAsWrapper(var), std::numeric_limits<TargetType>::max()); },
      "Needed to truncate `from` as it's value was to big for ToType.");
  }
  else
  {
    EXPECT_EQ(copyAsWrapper(var), TargetType(value));
  }
}

template <typename TargetType, typename T>
void checkNonOverflowingConversionFloat(Var var, T value)  // NOLINT(readability-function-size)
{
  // Wrapper needed to prevent errors from gtest macro expansion
  auto copyAsWrapper = [](Var var) { return getCopyAs<TargetType, /*checkedConversion=*/true>(var); };

  if (value < std::numeric_limits<TargetType>::lowest())
  {
    EXPECT_DEBUG_DEATH(
      { EXPECT_EQ(copyAsWrapper(var), std::numeric_limits<TargetType>::lowest()); },
      "Needed to truncate `from` as it's value was to small for ToType.");
  }
  else if (value > std::numeric_limits<TargetType>::max())
  {
    EXPECT_DEBUG_DEATH(
      { EXPECT_EQ(copyAsWrapper(var), std::numeric_limits<TargetType>::max()); },
      "Needed to truncate `from` as it's value was to big for ToType.");
  }
  else
  {
    EXPECT_EQ(copyAsWrapper(var), TargetType(value));
  }
}

template <typename T>
void checkNumber(T value)  // NOLINT(readability-function-size)
{
  Var var(value);

  EXPECT_EQ(value, getCopyAs<T>(var));

  if constexpr (std::is_integral_v<T>)
  {
    checkNonOverflowingConversion<uint8_t>(var, value);
    checkNonOverflowingConversion<int16_t>(var, value);
    checkNonOverflowingConversion<uint16_t>(var, value);
    checkNonOverflowingConversion<int32_t>(var, value);
    checkNonOverflowingConversion<uint32_t>(var, value);
    checkNonOverflowingConversion<int64_t>(var, value);
    checkNonOverflowingConversion<uint64_t>(var, value);
  }
  else if constexpr (std::is_floating_point_v<T>)
  {
    checkNonOverflowingConversionFloat<float32_t>(var, value);
    checkNonOverflowingConversionFloat<float64_t>(var, value);
  }
  else
  {
    EXPECT_EQ(getCopyAs<uint8_t>(var), uint8_t(value));
    EXPECT_EQ(getCopyAs<int16_t>(var), int16_t(value));
    EXPECT_EQ(getCopyAs<uint16_t>(var), uint16_t(value));
    EXPECT_EQ(getCopyAs<int32_t>(var), int32_t(value));
    EXPECT_EQ(getCopyAs<uint32_t>(var), uint32_t(value));
    EXPECT_EQ(getCopyAs<int64_t>(var), int64_t(value));
    EXPECT_EQ(getCopyAs<uint64_t>(var), uint64_t(value));

    EXPECT_EQ(getCopyAs<float32_t>(var), float32_t(value));
    EXPECT_EQ(getCopyAs<float64_t>(var), float64_t(value));
  }

  EXPECT_EQ(getCopyAs<bool>(var), value != T {0});
  EXPECT_EQ(getCopyAs<std::string>(var), std::to_string(value));
}

template <typename T>
void checkNumberValues()
{
  checkNumber<T>(std::numeric_limits<T>::min());
  checkNumber<T>(std::numeric_limits<T>::max());
  checkNumber<T>(std::numeric_limits<T>::lowest());
  checkNumber<T>({});
  checkNumber<T>(randomPositiveNumberAs<T, uint8_t>());
}

template <typename T>
void checkString(T value)  // NOLINT(readability-function-size)
{
  const auto valueStr = std::to_string(value);
  Var var(valueStr);

  EXPECT_EQ(getCopyAs<uint8_t>(var), uint8_t(value));
  EXPECT_EQ(getCopyAs<int16_t>(var), int16_t(value));
  EXPECT_EQ(getCopyAs<uint16_t>(var), uint16_t(value));
  EXPECT_EQ(getCopyAs<int32_t>(var), int32_t(value));
  EXPECT_EQ(getCopyAs<uint32_t>(var), uint32_t(value));
  EXPECT_EQ(getCopyAs<int64_t>(var), int64_t(value));
  EXPECT_EQ(getCopyAs<uint64_t>(var), uint64_t(value));
  EXPECT_EQ(getCopyAs<float32_t>(var), float32_t(value));
  EXPECT_EQ(getCopyAs<float64_t>(var), float64_t(value));

  EXPECT_EQ(getCopyAs<bool>(var), value != T {0});
  EXPECT_EQ(getCopyAs<std::string>(var), valueStr);
}

template <typename T>
void checkStringValues()
{
  checkString<T>({});
  for (std::size_t i = 0U; i < 100; ++i)
  {
    checkString<T>(randomPositiveNumberAs<T, uint8_t>());
  }
}

}  // namespace

/// @test
/// Checks empty var creation
/// @requirements(SEN-1053)
TEST(Var, empty)
{
  checkEmptyVar<std::monostate>();
  checkEmptyVar<uint8_t>();
  checkEmptyVar<int16_t>();
  checkEmptyVar<uint16_t>();
  checkEmptyVar<int32_t>();
  checkEmptyVar<uint32_t>();
  checkEmptyVar<int64_t>();
  checkEmptyVar<uint64_t>();
  checkEmptyVar<float32_t>();
  checkEmptyVar<float64_t>();
  checkEmptyVar<bool>();
  checkEmptyVar<std::string>();
  checkEmptyVar<Duration>();
  checkEmptyVar<TimeStamp>();
  checkEmptyVar<VarList>();
  checkEmptyVar<VarMap>();
}

/// @test
/// Checks boolean var creation and possible copies into other types
/// @requirements(SEN-1053)
TEST(Var, boolean)
{
  // false
  {
    constexpr bool boolValue = false;
    Var var(boolValue);
    EXPECT_EQ(boolValue, getCopyAs<bool>(var));

    EXPECT_EQ(getCopyAs<uint8_t>(var), 0);
    EXPECT_EQ(getCopyAs<int16_t>(var), 0);
    EXPECT_EQ(getCopyAs<uint16_t>(var), 0);
    EXPECT_EQ(getCopyAs<int32_t>(var), 0);
    EXPECT_EQ(getCopyAs<uint32_t>(var), 0);
    EXPECT_EQ(getCopyAs<int64_t>(var), 0);
    EXPECT_EQ(getCopyAs<uint64_t>(var), 0);
    EXPECT_EQ(getCopyAs<std::string>(var), "false");

    // conversion from boolean to the following types is not supported
    EXPECT_ANY_THROW(std::ignore = getCopyAs<Duration>(var));
    EXPECT_ANY_THROW(std::ignore = getCopyAs<TimeStamp>(var));
    EXPECT_ANY_THROW(std::ignore = getCopyAs<VarList>(var));
    EXPECT_ANY_THROW(std::ignore = getCopyAs<VarMap>(var));
  }

  // true
  {
    constexpr bool boolValue = true;
    Var var(boolValue);
    EXPECT_EQ(boolValue, getCopyAs<bool>(var));

    EXPECT_NE(getCopyAs<uint8_t>(var), 0);
    EXPECT_NE(getCopyAs<int16_t>(var), 0);
    EXPECT_NE(getCopyAs<uint16_t>(var), 0);
    EXPECT_NE(getCopyAs<int32_t>(var), 0);
    EXPECT_NE(getCopyAs<uint32_t>(var), 0);
    EXPECT_NE(getCopyAs<int64_t>(var), 0);
    EXPECT_NE(getCopyAs<uint64_t>(var), 0);
    EXPECT_EQ(getCopyAs<std::string>(var), "true");

    // conversion from boolean to the following types is not supported
    EXPECT_ANY_THROW(std::ignore = getCopyAs<Duration>(var));
    EXPECT_ANY_THROW(std::ignore = getCopyAs<TimeStamp>(var));
    EXPECT_ANY_THROW(std::ignore = getCopyAs<VarList>(var));
    EXPECT_ANY_THROW(std::ignore = getCopyAs<VarMap>(var));
  }
}

/// @test
/// Checks var creation with different numeric limits of number types
/// @requirements(SEN-1053)
TEST(Var, numbers)
{
  checkNumberValues<uint8_t>();
  checkNumberValues<int16_t>();
  checkNumberValues<uint16_t>();
  checkNumberValues<int32_t>();
  checkNumberValues<uint32_t>();
  checkNumberValues<int64_t>();
  checkNumberValues<uint64_t>();
  checkNumberValues<float32_t>();
  checkNumberValues<float64_t>();
}

/// @test
/// Checks string var creation and conversion
/// @requirements(SEN-1053)
TEST(Var, string)
{
  {
    checkStringValues<uint8_t>();
    checkStringValues<int16_t>();
    checkStringValues<uint16_t>();
    checkStringValues<int32_t>();
    checkStringValues<uint32_t>();
    checkStringValues<int64_t>();
    checkStringValues<uint64_t>();
    checkStringValues<float32_t>();
    checkStringValues<float64_t>();
  }

  // Empty string conversion edge cases
  {
    const Var var {""};
    EXPECT_EQ(getCopyAs<bool>(var), false);
    EXPECT_EQ(getCopyAs<float32_t>(var), 0.0);
    EXPECT_EQ(getCopyAs<float64_t>(var), 0.0);
  }

  // String numeric conversion for JSON
  {
    Var stringVar {"1000"};
    EXPECT_EQ(sen::toJson(stringVar), "\"1000\"");
  }
}

/// @test
/// Checks duration var creation and possible conversions
/// @requirements(SEN-1053)
TEST(Var, duration)
{
  auto t = std::chrono::nanoseconds(100);
  const Duration dur {t};
  const Var var(dur);

  EXPECT_EQ(getCopyAs<uint8_t>(var), t.count());
  EXPECT_EQ(getCopyAs<uint16_t>(var), t.count());
  EXPECT_EQ(getCopyAs<int16_t>(var), t.count());
  EXPECT_EQ(getCopyAs<uint32_t>(var), t.count());
  EXPECT_EQ(getCopyAs<int32_t>(var), t.count());
  EXPECT_EQ(getCopyAs<uint64_t>(var), t.count());
  EXPECT_EQ(getCopyAs<int64_t>(var), t.count());
  EXPECT_EQ(getCopyAs<std::string>(var), std::to_string(t.count()));
  EXPECT_EQ(getCopyAs<Duration>(var), dur);
  EXPECT_EQ(getCopyAs<TimeStamp>(var), TimeStamp(dur));
  EXPECT_EQ(sen::toJson(var), "100");
  EXPECT_EQ(sen::stringToDuration("1000ns").getNanoseconds(), 1000);
  EXPECT_ANY_THROW(std::ignore = sen::stringToDuration("invalid"));
}

/// @test
/// Checks timestamp var creation and possible conversions
/// @requirements(SEN-1053)
TEST(Var, timestamp)
{
  auto t = std::chrono::nanoseconds(150);
  const TimeStamp time {Duration(t)};
  const Var var(time);

  EXPECT_EQ(getCopyAs<uint8_t>(var), t.count());
  EXPECT_EQ(getCopyAs<uint16_t>(var), t.count());
  EXPECT_EQ(getCopyAs<int16_t>(var), t.count());
  EXPECT_EQ(getCopyAs<uint32_t>(var), t.count());
  EXPECT_EQ(getCopyAs<int32_t>(var), t.count());
  EXPECT_EQ(getCopyAs<uint64_t>(var), t.count());
  EXPECT_EQ(getCopyAs<int64_t>(var), t.count());
  EXPECT_EQ(getCopyAs<Duration>(var), time.sinceEpoch());
  EXPECT_EQ(getCopyAs<TimeStamp>(var), time);
  EXPECT_EQ(sen::toJson(var), '\"' + getCopyAs<std::string>(var) + '\"');
}

/// @test
/// Checks varlist creation and possible conversions
/// @requirements(SEN-1053)
TEST(Var, list)
{
  using namespace std::string_literals;

  {
    Var var(VarList {});
    EXPECT_EQ(getCopyAs<std::string>(var), "[]");
  }

  {
    Var var(VarList {1, 2, 3});
    EXPECT_EQ(getCopyAs<std::string>(var), "[\n  1,\n  2,\n  3\n]");
  }

  {
    Var var(VarList({"lorem"s, "ipsum"s}));
    EXPECT_EQ(getCopyAs<std::string>(var), "[\n  \"lorem\",\n  \"ipsum\"\n]");
  }
}

/// @test
/// Checks varmap creation and possible conversions
/// @requirements(SEN-1053)
TEST(Var, map)
{
  using namespace std::string_literals;
  {
    VarMap theMap = {};
    Var var(theMap);
    EXPECT_EQ(getCopyAs<std::string>(var), "{}");
  }

  {
    VarMap theMap = {{"hello"s, "world"s}};
    Var var(theMap);
    EXPECT_EQ(getCopyAs<std::string>(var), "{\n  \"hello\": \"world\"\n}");
  }

  {
    VarMap theMap = {{"a", 1}, {"b", 2}, {"c", 3}};
    EXPECT_EQ(getCopyAs<std::string>(theMap), "{\n  \"a\": 1,\n  \"b\": 2,\n  \"c\": 3\n}");
    EXPECT_EQ(getCopyAs<uint8_t>(theMap), 0);
  }
}

/// @test
/// Checks KeyedVar conversions
/// @requirements(SEN-1053)
TEST(Var, keyedVar)
{
  const sen::KeyedVar keyedVar {1, std::make_shared<Var>(2)};
  const Var var {keyedVar};

  const auto result = getCopyAs<std::string>(var);
  EXPECT_TRUE(result.find("\"key\": 1") != std::string::npos);
}

/// @test
/// Checks comparison between different var types
/// @requirements(SEN-1053)
TEST(Var, comparison)
{
  // same types and values
  {
    EXPECT_EQ(Var(uint8_t {2}), Var(uint8_t {2}));
    EXPECT_EQ(Var(int8_t {20}), Var(int8_t {20}));
    EXPECT_EQ(Var(uint16_t {}), Var(uint16_t {}));
    EXPECT_EQ(Var(int16_t {10}), Var(int16_t {10}));
    EXPECT_EQ(Var(uint32_t {3}), Var(uint32_t {3}));
    EXPECT_EQ(Var(int32_t {30}), Var(int32_t {30}));
    EXPECT_EQ(Var(uint64_t {}), Var(uint64_t {}));
    EXPECT_EQ(Var(int64_t {70}), Var(int64_t {70}));
    EXPECT_EQ(Var(float32_t {22}), Var(float32_t {22}));
    EXPECT_EQ(Var(float64_t {33}), Var(float64_t {33}));
    EXPECT_EQ(Var(TimeStamp {15}), Var(TimeStamp {15}));
    EXPECT_EQ(Var(Duration {33}), Var(Duration {33}));
    EXPECT_EQ(Var(std::string {"hello world"}), Var(std::string {"hello world"}));
    EXPECT_EQ(Var(std::string {"hello world"}), Var(std::string {"hello world"}));
    EXPECT_EQ(Var(bool {true}), Var(bool {true}));
    EXPECT_EQ(Var(bool {}), Var(bool {}));
    EXPECT_EQ(Var(), Var());
    EXPECT_EQ(Var(), Var({}));
    EXPECT_EQ(Var({}), Var({}));
    EXPECT_EQ(Var(std::monostate {}), Var(std::monostate {}));
    EXPECT_EQ(Var(std::monostate {}), Var({}));
    EXPECT_EQ(Var(std::monostate {}), Var());
  }

  // same values but different types
  {
    EXPECT_NE(Var(uint8_t {2}), Var(int8_t {2}));
    EXPECT_NE(Var(uint16_t {1}), Var(int16_t {1}));
    EXPECT_NE(Var(uint32_t {3}), Var(int32_t {3}));
    EXPECT_NE(Var(int64_t {70}), Var(uint64_t {70}));
    EXPECT_NE(Var(float32_t {22}), Var(float64_t {22}));
    EXPECT_NE(Var(TimeStamp {15}), Var(Duration {15}));
    EXPECT_NE(Var(bool {true}), Var(uint8_t {1}));
    EXPECT_NE(Var(bool {false}), Var(uint8_t {0}));
  }
}

/// @test
/// Checks find element var methods
/// @requirements(SEN-1053)
TEST(Var, findElement)
{
  VarMap theMap = {{"a", 1}, {"b", 2}, {"c", 3}, {"e", "hello"}};

  // find element as Var
  {
    EXPECT_NO_THROW(std::ignore = sen::findElement(theMap, "a", "could not find a in map!"));
    EXPECT_THROW(std::ignore = sen::findElement(theMap, "d", "could not find d in map!"), std::exception);
  }

  // find element as type T
  {
    EXPECT_NO_THROW(std::ignore = sen::findElementAs<int32_t>(theMap, "c", "could not find c in map!"));
    EXPECT_THROW(std::ignore = sen::findElementAs<int32_t>(theMap, "C", "could not find C in map!"), std::exception);
  }
}

/// @test
/// Checks swap between vars
/// @requirements(SEN-1053)
TEST(Var, swap)
{
  // simple
  {
    Var var1(int8_t {1});
    Var var2(int8_t {2});

    var1.swap(var2);
    EXPECT_EQ(var1, Var(int8_t {2}));
    EXPECT_EQ(var2, Var(int8_t {1}));
  }

  // different type
  {
    Var var1(uint8_t {1});
    Var var2(std::string {"hello world"});

    EXPECT_TRUE(var1.holds<uint8_t>());
    EXPECT_FALSE(var1.holds<std::string>());

    var1.swap(var2);

    EXPECT_FALSE(var1.holds<uint8_t>());
    EXPECT_TRUE(var1.holds<std::string>());

    EXPECT_EQ(var1, Var(std::string {"hello world"}));
    EXPECT_EQ(var2, Var(uint8_t {1}));
  }

  // empty type
  {
    Var var1({});
    Var var2(TimeStamp {256});

    EXPECT_TRUE(var1.isEmpty());
    EXPECT_FALSE(var1.holds<TimeStamp>());

    var1.swap(var2);

    EXPECT_FALSE(var1.isEmpty());
    EXPECT_TRUE(var1.holds<TimeStamp>());

    EXPECT_EQ(var1, Var(TimeStamp {256}));
    EXPECT_EQ(var2, Var());
    EXPECT_TRUE(var2.isEmpty());
  }
}

/// @test
/// Checks if a var holds specific types (helper methods)
/// @requirements(SEN-1053)
TEST(Var, metaHoldsType)
{
  Var varInt {10};
  EXPECT_TRUE(varInt.holdsIntegralValue());
  EXPECT_FALSE(varInt.holdsFloatingPointValue());

  Var varFloat {10.5};
  EXPECT_FALSE(varFloat.holdsIntegralValue());
  EXPECT_TRUE(varFloat.holdsFloatingPointValue());
}

/// @test
/// Checks basic JSON struct conversion
/// @requirements(SEN-1053)
TEST(Var, jsonStructConversion)
{
  test_type_traits::MyStruct2 myStruct2Original {};

  const auto json = sen::toJson(sen::toVariant(myStruct2Original));

  auto myStruct2Parsed = sen::toValue<test_type_traits::MyStruct2>(sen::fromJson(json));

  EXPECT_EQ(myStruct2Original, myStruct2Parsed);
}

/// @test
/// Checks JSON struct conversion with variable indentation
/// @requirements(SEN-1053)
TEST(Var, jsonIndentation)
{
  test_type_traits::MyStruct2 myStruct2Original {};

  for (int indent = 0; indent <= 4; indent += 2)
  {
    const auto json = sen::toJson(sen::toVariant(myStruct2Original), indent);
    auto myStruct2Parsed = sen::toValue<test_type_traits::MyStruct2>(sen::fromJson(json));
    EXPECT_EQ(myStruct2Original, myStruct2Parsed);
  }
}

/// @test
/// Checks JSON conversion for specific primitives, arrays, and edges.
/// @requirements(SEN-1053)
TEST(Var, jsonPrimitivesAndEdges)
{
  // Bool
  {
    Var var = sen::fromJson("true");
    ASSERT_TRUE(var.holds<bool>());
    EXPECT_EQ(var.get<bool>(), true);
  }

  // Signed Int
  {
    Var var = sen::fromJson("-12345");
    ASSERT_TRUE(var.holds<int64_t>());
    EXPECT_EQ(var.get<int64_t>(), -12345);
  }

  // Array
  {
    Var var = sen::fromJson("[1, -2]");
    ASSERT_TRUE(var.holds<VarList>());
    const auto& list = var.get<VarList>();
    ASSERT_EQ(list.size(), 2U);
    EXPECT_EQ(list[0].get<uint64_t>(), 1);
    EXPECT_EQ(list[1].get<int64_t>(), -2);
  }

  // Null
  {
    Var var = sen::fromJson("null");
    EXPECT_TRUE(var.isEmpty());
    EXPECT_EQ(sen::toJson(var), "null");
  }

  // Unsupported Type
  {
    std::vector<uint8_t> bsonWithBinary = {
      0x12, 0x00, 0x00, 0x00, 0x05, 'x', 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00};
    EXPECT_ANY_THROW(std::ignore = sen::fromBson(bsonWithBinary));
  }
}

/// @test
/// Checks binary format conversions (BSON, CBOR, MsgPack, UBJson)
/// @requirements(SEN-1053)
TEST(Var, binaryFormats)
{
  VarMap map;
  map["float"] = Var {float64_t {123.456}};
  map["bool"] = Var {true};
  map["str"] = Var {std::string {"test"}};
  Var var {map};

  // BSON
  {
    auto bin = sen::toBson(var);
    EXPECT_FALSE(bin.empty());
    EXPECT_EQ(var, sen::fromBson(bin));
  }
  // CBOR
  {
    auto bin = sen::toCbor(var);
    EXPECT_FALSE(bin.empty());
    EXPECT_EQ(var, sen::fromCbor(bin));
  }
  // MsgPack
  {
    auto bin = sen::toMsgpack(var);
    EXPECT_FALSE(bin.empty());
    EXPECT_EQ(var, sen::fromMsgpack(bin));
  }
  // UBJson
  {
    auto bin = sen::toUbjson(var);
    EXPECT_FALSE(bin.empty());
    EXPECT_EQ(var, sen::fromUbjson(bin));
  }
}

/// @test
/// Checks Checked Conversions and Implementation Wrappers (Safety & Gaps)
/// @requirements(SEN-1053)
TEST(Var, checkedConversions)
{
  // Unchecked Conversion: Impossible cases
  {
    Var var(10);
    EXPECT_NO_THROW(std::ignore = getCopyAs<std::monostate>(var));
    EXPECT_ANY_THROW(std::ignore = getCopyAs<sen::KeyedVar>(var));
  }

  // Checked Conversions: Safe Cases
  {
    Var var {1};
    EXPECT_EQ((getCopyAs<bool, true>(var)), true);
    EXPECT_EQ((getCopyAs<std::string, true>(var)), "1");
    EXPECT_EQ((getCopyAs<Duration, true>(var).getNanoseconds()), 1);
    EXPECT_EQ((getCopyAs<TimeStamp, true>(var).sinceEpoch().getNanoseconds()), 1);
  }

  // Checked Conversions: Impossible Cases
  {
    const Var var {1};

    EXPECT_ANY_THROW(std::ignore = (getCopyAs<VarList, true>(var)));
    EXPECT_ANY_THROW(std::ignore = (getCopyAs<VarMap, true>(var)));
    EXPECT_ANY_THROW(std::ignore = (getCopyAs<sen::KeyedVar, true>(var)));
    EXPECT_NO_THROW(std::ignore = (getCopyAs<std::monostate, true>(var)));
  }

#ifndef NDEBUG
  // Checked Conversions: Overflow
  {
    const Var var {300};
    EXPECT_DEATH(std::ignore = (getCopyAs<uint8_t, true>(var)),
                 "Needed to truncate `from` as it's value was to big for ToType.");
  }
#endif
}
