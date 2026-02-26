// === time_types_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/time_types.h"

// google test
#include <gtest/gtest.h>

namespace
{

template <typename Meta>
void checkIntegerEquality()
{
  EXPECT_NE(Meta::get(), sen::Int16Type::get());
  EXPECT_NE(Meta::get(), sen::Int32Type::get());
  EXPECT_NE(Meta::get(), sen::Int64Type::get());
}

template <typename Meta>
void checkUnsignedIntegerEquality()
{
  EXPECT_NE(Meta::get(), sen::UInt8Type::get());
  EXPECT_NE(Meta::get(), sen::UInt16Type::get());
  EXPECT_NE(Meta::get(), sen::UInt32Type::get());
  EXPECT_NE(Meta::get(), sen::UInt64Type::get());
}

template <typename Meta>
void checkEquality()
{
  checkIntegerEquality<Meta>();
  checkUnsignedIntegerEquality<Meta>();

  EXPECT_NE(Meta::get(), sen::Float32Type::get());
  EXPECT_NE(Meta::get(), sen::Float64Type::get());

  EXPECT_NE(Meta::get(), sen::BoolType::get());
  EXPECT_NE(Meta::get(), sen::StringType::get());
}

}  // namespace

/// @test
/// Checks duration instance type
/// @requirements(SEN-358)
TEST(TimeTypes, durationBoolConversion)
{
  {

    auto type = sen::DurationType::get();
    EXPECT_EQ(type->getName(), "Duration");
    EXPECT_FALSE(type->getDescription().empty());

    // things that durations are
    EXPECT_TRUE(type->isCustomType());
    EXPECT_TRUE(type->isQuantityType());
    EXPECT_TRUE(type->isDurationType());

    // things that durations aren't
    EXPECT_FALSE(type->isNativeType());
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
    EXPECT_FALSE(type->isStringType());
    EXPECT_FALSE(type->isTimestampType());
    EXPECT_FALSE(type->isSequenceType());
    EXPECT_FALSE(type->isStructType());
    EXPECT_FALSE(type->isEnumType());
    EXPECT_FALSE(type->isClassType());
    EXPECT_FALSE(type->isVariantType());
    EXPECT_FALSE(type->isAliasType());
  }
}

/// @test
/// Checks duration instance type
/// @requirements(SEN-358)
TEST(TimeTypes, durationConversion)
{
  {

    auto type = sen::DurationType::get();
    EXPECT_EQ(type->getName(), "Duration");
    EXPECT_FALSE(type->getDescription().empty());

    // things that durations are
    EXPECT_NE(type->asCustomType(), nullptr);
    EXPECT_NE(type->asQuantityType(), nullptr);
    EXPECT_NE(type->asDurationType(), nullptr);

    // things that durations aren't
    EXPECT_EQ(type->asNativeType(), nullptr);
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
    EXPECT_EQ(type->asBoolType(), nullptr);
    EXPECT_EQ(type->asStringType(), nullptr);
    EXPECT_EQ(type->asTimestampType(), nullptr);
    EXPECT_EQ(type->asSequenceType(), nullptr);
    EXPECT_EQ(type->asStructType(), nullptr);
    EXPECT_EQ(type->asEnumType(), nullptr);
    EXPECT_EQ(type->asClassType(), nullptr);
    EXPECT_EQ(type->asVariantType(), nullptr);
    EXPECT_EQ(type->asAliasType(), nullptr);
  }

  {
    checkEquality<sen::DurationType>();
    EXPECT_EQ(sen::DurationType::get(), sen::DurationType::get());
    EXPECT_NE(sen::DurationType::get(), sen::TimestampType::get());
  }
}

/// @test
/// Checks timestamp instance type
/// @requirements(SEN-1050)
TEST(TimeTypes, timestampBoolConversion)
{
  {
    auto type = sen::TimestampType::get();
    EXPECT_EQ(type->getName(), "TimeStamp");
    EXPECT_FALSE(type->getDescription().empty());

    // things that timestamps are
    EXPECT_TRUE(type->isCustomType());
    EXPECT_TRUE(type->isQuantityType());
    EXPECT_TRUE(type->isTimestampType());

    // things that durations aren't
    EXPECT_FALSE(type->isNativeType());
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
    EXPECT_FALSE(type->isStringType());
    EXPECT_FALSE(type->isDurationType());
    EXPECT_FALSE(type->isSequenceType());
    EXPECT_FALSE(type->isStructType());
    EXPECT_FALSE(type->isEnumType());
    EXPECT_FALSE(type->isClassType());
    EXPECT_FALSE(type->isVariantType());
    EXPECT_FALSE(type->isAliasType());
  }
}

/// @test
/// Checks timestamp instance type
/// @requirements(SEN-1050)
TEST(TimeTypes, timestampConversion)
{
  {
    auto type = sen::TimestampType::get();
    EXPECT_EQ(type->getName(), "TimeStamp");
    EXPECT_FALSE(type->getDescription().empty());

    // things that timestamps are
    EXPECT_NE(type->asCustomType(), nullptr);
    EXPECT_NE(type->asQuantityType(), nullptr);
    EXPECT_NE(type->asTimestampType(), nullptr);

    // things that durations aren't
    EXPECT_EQ(type->asNativeType(), nullptr);
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
    EXPECT_EQ(type->asBoolType(), nullptr);
    EXPECT_EQ(type->asStringType(), nullptr);
    EXPECT_EQ(type->asDurationType(), nullptr);
    EXPECT_EQ(type->asSequenceType(), nullptr);
    EXPECT_EQ(type->asStructType(), nullptr);
    EXPECT_EQ(type->asEnumType(), nullptr);
    EXPECT_EQ(type->asClassType(), nullptr);
    EXPECT_EQ(type->asVariantType(), nullptr);
    EXPECT_EQ(type->asAliasType(), nullptr);
  }

  {
    checkEquality<sen::TimestampType>();
    EXPECT_NE(sen::TimestampType::get(), sen::DurationType::get());
    EXPECT_EQ(sen::TimestampType::get(), sen::TimestampType::get());
  }
}
