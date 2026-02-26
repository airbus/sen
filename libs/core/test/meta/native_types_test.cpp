// === native_types_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/type.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

namespace
{

void checkEqual(const sen::Type& lhs, const sen::Type& rhs, bool expectation)
{
  EXPECT_EQ(lhs == rhs, expectation);
  EXPECT_EQ(lhs != rhs, !expectation);
}

template <typename Meta>
void checkEquality()
{
  using Native = typename Meta::NativeType;

  checkEqual(*Meta::get(), *sen::UInt8Type::get(), std::is_same_v<Native, uint8_t>);
  checkEqual(*Meta::get(), *sen::Int16Type::get(), std::is_same_v<Native, int16_t>);
  checkEqual(*Meta::get(), *sen::UInt16Type::get(), std::is_same_v<Native, uint16_t>);
  checkEqual(*Meta::get(), *sen::Int32Type::get(), std::is_same_v<Native, int32_t>);
  checkEqual(*Meta::get(), *sen::UInt32Type::get(), std::is_same_v<Native, uint32_t>);
  checkEqual(*Meta::get(), *sen::Int64Type::get(), std::is_same_v<Native, int64_t>);
  checkEqual(*Meta::get(), *sen::UInt64Type::get(), std::is_same_v<Native, uint64_t>);
  checkEqual(*Meta::get(), *sen::Float32Type::get(), std::is_same_v<Native, float32_t>);
  checkEqual(*Meta::get(), *sen::Float64Type::get(), std::is_same_v<Native, float64_t>);
  checkEqual(*Meta::get(), *sen::BoolType::get(), std::is_same_v<Native, bool>);
  checkEqual(*Meta::get(), *sen::VoidType::get(), std::is_same_v<Native, void>);
  checkEqual(*Meta::get(), *sen::StringType::get(), std::is_same_v<Native, std::string>);
}

template <typename Native, typename Meta>
void checkIntegralType()  // NOLINT(readability-function-size)
{
  const auto& type = Meta::get();
  EXPECT_FALSE(type->getDescription().empty());
  EXPECT_TRUE(type->isBounded());

  // numeric
  EXPECT_EQ(type->isSigned(), std::numeric_limits<Native>::is_signed);
  EXPECT_EQ(type->isIEC559(), std::numeric_limits<Native>::is_iec559);
  EXPECT_EQ(type->hasInfinity(), std::numeric_limits<Native>::has_infinity);

  // things that integrals are
  EXPECT_NE(type->asNativeType(), nullptr);
  EXPECT_NE(type->asNumericType(), nullptr);
  EXPECT_NE(type->asIntegralType(), nullptr);

  // things that integrals aren't
  EXPECT_EQ(type->asVoidType(), nullptr);
  EXPECT_EQ(type->asBoolType(), nullptr);
  EXPECT_EQ(type->asRealType(), nullptr);
  EXPECT_EQ(type->asFloat32Type(), nullptr);
  EXPECT_EQ(type->asFloat64Type(), nullptr);
  EXPECT_EQ(type->asStringType(), nullptr);
  EXPECT_EQ(type->asDurationType(), nullptr);
  EXPECT_EQ(type->asTimestampType(), nullptr);
  EXPECT_EQ(type->asCustomType(), nullptr);
  EXPECT_EQ(type->asSequenceType(), nullptr);
  EXPECT_EQ(type->asStructType(), nullptr);
  EXPECT_EQ(type->asEnumType(), nullptr);
  EXPECT_EQ(type->asClassType(), nullptr);
  EXPECT_EQ(type->asVariantType(), nullptr);
  EXPECT_EQ(type->asQuantityType(), nullptr);

  if (std::is_same_v<typename Meta::NativeType, uint8_t>)
  {
    EXPECT_NE(type->asUInt8Type(), nullptr);
    EXPECT_EQ(type->asInt16Type(), nullptr);
    EXPECT_EQ(type->asUInt16Type(), nullptr);
    EXPECT_EQ(type->asInt32Type(), nullptr);
    EXPECT_EQ(type->asUInt32Type(), nullptr);
    EXPECT_EQ(type->asInt64Type(), nullptr);
    EXPECT_EQ(type->asUInt64Type(), nullptr);
  }
  if (std::is_same_v<typename Meta::NativeType, int16_t>)
  {
    EXPECT_EQ(type->asUInt8Type(), nullptr);
    EXPECT_NE(type->asInt16Type(), nullptr);
    EXPECT_EQ(type->asUInt16Type(), nullptr);
    EXPECT_EQ(type->asInt32Type(), nullptr);
    EXPECT_EQ(type->asUInt32Type(), nullptr);
    EXPECT_EQ(type->asInt64Type(), nullptr);
    EXPECT_EQ(type->asUInt64Type(), nullptr);
  }
  if (std::is_same_v<typename Meta::NativeType, uint16_t>)
  {
    EXPECT_EQ(type->asUInt8Type(), nullptr);
    EXPECT_EQ(type->asInt16Type(), nullptr);
    EXPECT_NE(type->asUInt16Type(), nullptr);
    EXPECT_EQ(type->asInt32Type(), nullptr);
    EXPECT_EQ(type->asUInt32Type(), nullptr);
    EXPECT_EQ(type->asInt64Type(), nullptr);
    EXPECT_EQ(type->asUInt64Type(), nullptr);
  }
  if (std::is_same_v<typename Meta::NativeType, int32_t>)
  {
    EXPECT_EQ(type->asUInt8Type(), nullptr);
    EXPECT_EQ(type->asInt16Type(), nullptr);
    EXPECT_EQ(type->asUInt16Type(), nullptr);
    EXPECT_NE(type->asInt32Type(), nullptr);
    EXPECT_EQ(type->asUInt32Type(), nullptr);
    EXPECT_EQ(type->asInt64Type(), nullptr);
    EXPECT_EQ(type->asUInt64Type(), nullptr);
  }
  if (std::is_same_v<typename Meta::NativeType, uint32_t>)
  {
    EXPECT_EQ(type->asUInt8Type(), nullptr);
    EXPECT_EQ(type->asInt16Type(), nullptr);
    EXPECT_EQ(type->asUInt16Type(), nullptr);
    EXPECT_EQ(type->asInt32Type(), nullptr);
    EXPECT_NE(type->asUInt32Type(), nullptr);
    EXPECT_EQ(type->asInt64Type(), nullptr);
    EXPECT_EQ(type->asUInt64Type(), nullptr);
  }
  if (std::is_same_v<typename Meta::NativeType, int64_t>)
  {
    EXPECT_EQ(type->asUInt8Type(), nullptr);
    EXPECT_EQ(type->asInt16Type(), nullptr);
    EXPECT_EQ(type->asUInt16Type(), nullptr);
    EXPECT_EQ(type->asInt32Type(), nullptr);
    EXPECT_EQ(type->asUInt32Type(), nullptr);
    EXPECT_NE(type->asInt64Type(), nullptr);
    EXPECT_EQ(type->asUInt64Type(), nullptr);
  }
  if (std::is_same_v<typename Meta::NativeType, uint64_t>)
  {
    EXPECT_EQ(type->asUInt8Type(), nullptr);
    EXPECT_EQ(type->asInt16Type(), nullptr);
    EXPECT_EQ(type->asUInt16Type(), nullptr);
    EXPECT_EQ(type->asInt32Type(), nullptr);
    EXPECT_EQ(type->asUInt32Type(), nullptr);
    EXPECT_EQ(type->asInt64Type(), nullptr);
    EXPECT_NE(type->asUInt64Type(), nullptr);
  }
}

template <typename Native, typename Meta>
void checkRealType()  // NOLINT(readability-function-size)
{
  const auto& type = Meta::get();
  EXPECT_FALSE(type->getDescription().empty());
  EXPECT_TRUE(type->isBounded());

  // numeric
  EXPECT_EQ(type->isSigned(), std::numeric_limits<Native>::is_signed);
  EXPECT_EQ(type->isIEC559(), std::numeric_limits<Native>::is_iec559);
  EXPECT_EQ(type->hasInfinity(), std::numeric_limits<Native>::has_infinity);

  // real
  EXPECT_EQ(type->getMaxValue(), std::numeric_limits<Native>::max());
  EXPECT_EQ(type->getMinValue(), std::numeric_limits<Native>::lowest());
  EXPECT_EQ(type->getEpsilon(), std::numeric_limits<Native>::epsilon());

  // things that reals are
  EXPECT_NE(type->asNativeType(), nullptr);
  EXPECT_NE(type->asNumericType(), nullptr);
  EXPECT_NE(type->asRealType(), nullptr);

  // things that reals aren't
  EXPECT_EQ(type->asVoidType(), nullptr);
  EXPECT_EQ(type->asBoolType(), nullptr);
  EXPECT_EQ(type->asIntegralType(), nullptr);
  EXPECT_EQ(type->asUInt8Type(), nullptr);
  EXPECT_EQ(type->asInt16Type(), nullptr);
  EXPECT_EQ(type->asUInt16Type(), nullptr);
  EXPECT_EQ(type->asInt32Type(), nullptr);
  EXPECT_EQ(type->asUInt32Type(), nullptr);
  EXPECT_EQ(type->asInt64Type(), nullptr);
  EXPECT_EQ(type->asUInt64Type(), nullptr);
  EXPECT_EQ(type->asStringType(), nullptr);
  EXPECT_EQ(type->asDurationType(), nullptr);
  EXPECT_EQ(type->asTimestampType(), nullptr);
  EXPECT_EQ(type->asCustomType(), nullptr);
  EXPECT_EQ(type->asSequenceType(), nullptr);
  EXPECT_EQ(type->asStructType(), nullptr);
  EXPECT_EQ(type->asEnumType(), nullptr);
  EXPECT_EQ(type->asClassType(), nullptr);
  EXPECT_EQ(type->asVariantType(), nullptr);
  EXPECT_EQ(type->asQuantityType(), nullptr);
  EXPECT_EQ(type->asAliasType(), nullptr);

  if (std::is_same_v<typename Meta::NativeType, float32_t>)
  {
    EXPECT_NE(type->asFloat32Type(), nullptr);
    EXPECT_EQ(type->asFloat64Type(), nullptr);
  }
  if (std::is_same_v<typename Meta::NativeType, float64_t>)
  {
    EXPECT_EQ(type->asFloat32Type(), nullptr);
    EXPECT_NE(type->asFloat64Type(), nullptr);
  }
}

}  // namespace

/// @test
/// Checks equality of native types
/// @requirements(SEN-575)
TEST(NativeTypes, comparison)
{
  checkEquality<sen::VoidType>();
  checkEquality<sen::UInt8Type>();
  checkEquality<sen::Int16Type>();
  checkEquality<sen::UInt16Type>();
  checkEquality<sen::Int32Type>();
  checkEquality<sen::UInt32Type>();
  checkEquality<sen::Int64Type>();
  checkEquality<sen::UInt64Type>();
  checkEquality<sen::Float32Type>();
  checkEquality<sen::Float64Type>();
  checkEquality<sen::BoolType>();
  checkEquality<sen::StringType>();
}

/// @test
/// Checks integral types
/// @requirements(SEN-575)
TEST(NativeTypes, integrals)
{
  checkIntegralType<uint8_t, sen::UInt8Type>();
  checkIntegralType<int16_t, sen::Int16Type>();
  checkIntegralType<uint16_t, sen::UInt16Type>();
  checkIntegralType<int32_t, sen::Int32Type>();
  checkIntegralType<uint32_t, sen::UInt32Type>();
  checkIntegralType<int64_t, sen::Int64Type>();
  checkIntegralType<uint64_t, sen::UInt64Type>();
}

/// @test
/// Checks real types
/// @requirements(SEN-575)
TEST(NativeTypes, reals)
{
  checkRealType<float32_t, sen::Float32Type>();
  checkRealType<float64_t, sen::Float64Type>();
}

/// @test
/// Checks void types
/// @requirements(SEN-575)
TEST(NativeTypes, voidCheck)
{
  auto type = sen::VoidType::get();
  EXPECT_EQ(type->getName(), "void");
  EXPECT_FALSE(type->getDescription().empty());
  EXPECT_TRUE(type->isBounded());

  // things that void is not
  EXPECT_FALSE(type->isNativeType());
  EXPECT_FALSE(type->isBoolType());
  EXPECT_FALSE(type->isNumericType());
  EXPECT_FALSE(type->isRealType());
  EXPECT_FALSE(type->isIntegralType());
  EXPECT_FALSE(type->isUInt8Type());
  EXPECT_FALSE(type->isInt16Type());
  EXPECT_FALSE(type->isUInt16Type());
  EXPECT_FALSE(type->isInt32Type());
  EXPECT_FALSE(type->isUInt32Type());
  EXPECT_FALSE(type->isInt64Type());
  EXPECT_FALSE(type->isUInt64Type());
  EXPECT_FALSE(type->isStringType());
  EXPECT_FALSE(type->isDurationType());
  EXPECT_FALSE(type->isTimestampType());
  EXPECT_FALSE(type->isCustomType());
  EXPECT_FALSE(type->isSequenceType());
  EXPECT_FALSE(type->isStructType());
  EXPECT_FALSE(type->isEnumType());
  EXPECT_FALSE(type->isClassType());
  EXPECT_FALSE(type->isVariantType());
  EXPECT_FALSE(type->isQuantityType());
  EXPECT_FALSE(type->isAliasType());
}

TEST(NativeTypes, voidConversion)
{
  const auto& type = sen::VoidType::get();

  // things that void is not
  EXPECT_EQ(type->asNativeType(), nullptr);
  EXPECT_EQ(type->asBoolType(), nullptr);
  EXPECT_EQ(type->asNumericType(), nullptr);
  EXPECT_EQ(type->asRealType(), nullptr);
  EXPECT_EQ(type->asIntegralType(), nullptr);
  EXPECT_EQ(type->asUInt8Type(), nullptr);
  EXPECT_EQ(type->asInt16Type(), nullptr);
  EXPECT_EQ(type->asUInt16Type(), nullptr);
  EXPECT_EQ(type->asInt32Type(), nullptr);
  EXPECT_EQ(type->asUInt32Type(), nullptr);
  EXPECT_EQ(type->asInt64Type(), nullptr);
  EXPECT_EQ(type->asUInt64Type(), nullptr);
  EXPECT_EQ(type->asStringType(), nullptr);
  EXPECT_EQ(type->asDurationType(), nullptr);
  EXPECT_EQ(type->asTimestampType(), nullptr);
  EXPECT_EQ(type->asCustomType(), nullptr);
  EXPECT_EQ(type->asSequenceType(), nullptr);
  EXPECT_EQ(type->asStructType(), nullptr);
  EXPECT_EQ(type->asEnumType(), nullptr);
  EXPECT_EQ(type->asClassType(), nullptr);
  EXPECT_EQ(type->asVariantType(), nullptr);
  EXPECT_EQ(type->asQuantityType(), nullptr);
  EXPECT_EQ(type->asAliasType(), nullptr);
}

/// @test
/// Checks boolean types
/// @requirements(SEN-575)
TEST(NativeTypes, booleanCheck)
{
  auto type = sen::BoolType::get();
  EXPECT_EQ(type->getName(), "bool");
  EXPECT_FALSE(type->getDescription().empty());
  EXPECT_TRUE(type->isBounded());

  // things that booleans are
  EXPECT_TRUE(type->isNativeType());
  EXPECT_TRUE(type->isBoolType());

  // things that booleans aren't
  EXPECT_FALSE(type->isNumericType());
  EXPECT_FALSE(type->isRealType());
  EXPECT_FALSE(type->isIntegralType());
  EXPECT_FALSE(type->isUInt8Type());
  EXPECT_FALSE(type->isInt16Type());
  EXPECT_FALSE(type->isUInt16Type());
  EXPECT_FALSE(type->isInt32Type());
  EXPECT_FALSE(type->isUInt32Type());
  EXPECT_FALSE(type->isInt64Type());
  EXPECT_FALSE(type->isUInt64Type());
  EXPECT_FALSE(type->isStringType());
  EXPECT_FALSE(type->isDurationType());
  EXPECT_FALSE(type->isTimestampType());
  EXPECT_FALSE(type->isCustomType());
  EXPECT_FALSE(type->isSequenceType());
  EXPECT_FALSE(type->isStructType());
  EXPECT_FALSE(type->isEnumType());
  EXPECT_FALSE(type->isClassType());
  EXPECT_FALSE(type->isVariantType());
  EXPECT_FALSE(type->isQuantityType());
  EXPECT_FALSE(type->isAliasType());
}

TEST(NativeTypes, booleanConversion)
{
  const auto& type = sen::BoolType::get();
  // things that booleans are
  EXPECT_NE(type->asNativeType(), nullptr);
  EXPECT_NE(type->asBoolType(), nullptr);

  // things that booleans aren't
  EXPECT_EQ(type->asNumericType(), nullptr);
  EXPECT_EQ(type->asRealType(), nullptr);
  EXPECT_EQ(type->asIntegralType(), nullptr);
  EXPECT_EQ(type->asUInt8Type(), nullptr);
  EXPECT_EQ(type->asInt16Type(), nullptr);
  EXPECT_EQ(type->asUInt16Type(), nullptr);
  EXPECT_EQ(type->asInt32Type(), nullptr);
  EXPECT_EQ(type->asUInt32Type(), nullptr);
  EXPECT_EQ(type->asInt64Type(), nullptr);
  EXPECT_EQ(type->asUInt64Type(), nullptr);
  EXPECT_EQ(type->asStringType(), nullptr);
  EXPECT_EQ(type->asDurationType(), nullptr);
  EXPECT_EQ(type->asTimestampType(), nullptr);
  EXPECT_EQ(type->asCustomType(), nullptr);
  EXPECT_EQ(type->asSequenceType(), nullptr);
  EXPECT_EQ(type->asStructType(), nullptr);
  EXPECT_EQ(type->asEnumType(), nullptr);
  EXPECT_EQ(type->asClassType(), nullptr);
  EXPECT_EQ(type->asVariantType(), nullptr);
  EXPECT_EQ(type->asQuantityType(), nullptr);
  EXPECT_EQ(type->asAliasType(), nullptr);
}

/// @test
/// Checks string type
/// @requirements(SEN-575)
TEST(NativeTypes, string)
{
  const auto& type = sen::StringType::get();
  EXPECT_EQ(type->getName(), "string");
  EXPECT_FALSE(type->getDescription().empty());
  EXPECT_FALSE(type->isBounded());

  // things that strings are
  EXPECT_TRUE(type->isNativeType());
  EXPECT_TRUE(type->isStringType());

  // things that strings aren't
  EXPECT_FALSE(type->isNumericType());
  EXPECT_FALSE(type->isRealType());
  EXPECT_FALSE(type->isIntegralType());
  EXPECT_FALSE(type->isUInt8Type());
  EXPECT_FALSE(type->isInt16Type());
  EXPECT_FALSE(type->isUInt16Type());
  EXPECT_FALSE(type->isInt32Type());
  EXPECT_FALSE(type->isUInt32Type());
  EXPECT_FALSE(type->isInt64Type());
  EXPECT_FALSE(type->isUInt64Type());
  EXPECT_FALSE(type->isBoolType());
  EXPECT_FALSE(type->isDurationType());
  EXPECT_FALSE(type->isTimestampType());
  EXPECT_FALSE(type->isCustomType());
  EXPECT_FALSE(type->isSequenceType());
  EXPECT_FALSE(type->isStructType());
  EXPECT_FALSE(type->isEnumType());
  EXPECT_FALSE(type->isClassType());
  EXPECT_FALSE(type->isVariantType());
  EXPECT_FALSE(type->isQuantityType());
  EXPECT_FALSE(type->isAliasType());
}
