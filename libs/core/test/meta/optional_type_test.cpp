// === optional_type_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/optional_type.h"

// google test
#include <gtest/gtest.h>

// std
#include <exception>
#include <tuple>

using sen::OptionalSpec;
using sen::OptionalType;

namespace
{
const OptionalSpec& validNativeSpec()
{
  static const OptionalSpec validNativeSpec = {
    "TestMaybeNative", "TestMaybeNative", "maybe native type", sen::UInt64Type::get()};
  return validNativeSpec;
}

const OptionalType& type()
{
  static const auto type = OptionalType::make(validNativeSpec());
  return *type;
}

void checkSpecData(const OptionalType& type, const OptionalSpec& spec)
{
  EXPECT_EQ(type.getName(), spec.name);
  EXPECT_EQ(type.getQualifiedName(), spec.qualifiedName);
  EXPECT_EQ(type.getDescription(), spec.description);
  EXPECT_EQ(type.getType(), spec.type);
}

void checkInvalidSpec(const OptionalSpec& spec)
{
  EXPECT_THROW(std::ignore = OptionalType::make(spec), std::exception);
}

}  // namespace

/// @test
/// Checks optional spec comparison
/// @requirements(SEN-355)
TEST(OptionalSpec, specComparison)
{
  const auto& lhs = validNativeSpec();

  // same
  {
    const OptionalSpec& rhs = validNativeSpec();
    EXPECT_EQ(lhs, rhs);
  }

  // different name
  {
    OptionalSpec rhs = validNativeSpec();
    rhs.name = validNativeSpec().name + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different qualified name
  {
    OptionalSpec rhs = validNativeSpec();
    rhs.qualifiedName = validNativeSpec().qualifiedName + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different description
  {
    OptionalSpec rhs = validNativeSpec();
    rhs.description = validNativeSpec().description + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different type
  {
    OptionalSpec rhs = validNativeSpec();
    rhs.type = sen::Int16Type::get();
    EXPECT_NE(lhs, rhs);
  }

  // no name
  {
    OptionalSpec rhs = validNativeSpec();
    rhs.name = "";
    EXPECT_NE(lhs, rhs);
  }

  // no name 2
  {
    OptionalSpec rhs = validNativeSpec();
    rhs.name = {};
    EXPECT_NE(lhs, rhs);
  }

  // no qualify name
  {
    OptionalSpec rhs = validNativeSpec();
    rhs.qualifiedName = "";
    EXPECT_NE(lhs, rhs);
  }

  // no qualify name 2
  {
    OptionalSpec rhs = validNativeSpec();
    rhs.qualifiedName = {};
    EXPECT_NE(lhs, rhs);
  }

  // no description
  {
    OptionalSpec rhs = validNativeSpec();
    rhs.description = "";
    EXPECT_NE(lhs, rhs);
  }

  // no description 2
  {
    OptionalSpec rhs = validNativeSpec();
    rhs.description = {};
    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks optional instance type
/// @requirements(SEN-355)
TEST(OptionalType, basicsBoolConversion)
{
  auto type = OptionalType::make(validNativeSpec());

  // things that optionals are
  EXPECT_TRUE(type->isCustomType());
  EXPECT_TRUE(type->isOptionalType());
  EXPECT_EQ(type->isBounded(), type->getType()->isBounded());

  // things that optionals aren't
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
  EXPECT_FALSE(type->isDurationType());
  EXPECT_FALSE(type->isSequenceType());
  EXPECT_FALSE(type->isEnumType());
  EXPECT_FALSE(type->isStructType());
  EXPECT_FALSE(type->isClassType());
  EXPECT_FALSE(type->isVariantType());
  EXPECT_FALSE(type->isQuantityType());
  EXPECT_FALSE(type->isAliasType());
}

/// @test
/// Checks optional instance type
/// @requirements(SEN-355)
TEST(OptionalType, basicsConversion)
{
  auto type = OptionalType::make(validNativeSpec());

  // things that optionals are
  EXPECT_NE(type->asCustomType(), nullptr);
  EXPECT_NE(type->asOptionalType(), nullptr);
  EXPECT_EQ(type->isBounded(), type->getType()->isBounded());

  // things that optionals aren't
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
  EXPECT_EQ(type->asDurationType(), nullptr);
  EXPECT_EQ(type->asSequenceType(), nullptr);
  EXPECT_EQ(type->asEnumType(), nullptr);
  EXPECT_EQ(type->asStructType(), nullptr);
  EXPECT_EQ(type->asClassType(), nullptr);
  EXPECT_EQ(type->asVariantType(), nullptr);
  EXPECT_EQ(type->asQuantityType(), nullptr);
  EXPECT_EQ(type->asAliasType(), nullptr);
}

/// @test
/// Checks correct optional instance creation
/// @requirements(SEN-355)
TEST(OptionalType, makeBasic)
{
  // basic
  {
    const auto& spec = validNativeSpec();
    auto type = OptionalType::make(spec);
    checkSpecData(*type, spec);
  }

  // optional of optional
  {
    const auto& spec =
      OptionalSpec {"DoubleOptionalTest", "DoubleOptionalTest", "", OptionalType::make(validNativeSpec())};
    auto type = OptionalType::make(spec);
    checkSpecData(*type, spec);
  }
}

/// @test
/// Checks invalid optional creation
/// @requirements(SEN-355)
TEST(OptionalType, makeInvalid)
{
  // spec with no name
  {
    OptionalSpec invalid = validNativeSpec();
    invalid.name = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid name
  {
    OptionalSpec invalid = validNativeSpec();
    invalid.name = "someStruct";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 2
  {
    OptionalSpec invalid = validNativeSpec();
    invalid.name = "+-$/&%";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 3
  {
    OptionalSpec invalid = validNativeSpec();
    invalid.name = "1nvalidName";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 4
  {
    OptionalSpec invalid = validNativeSpec();
    invalid.name = "invalid name";
    checkInvalidSpec(invalid);
  }

  // spec with no qualified name
  {
    OptionalSpec invalid = validNativeSpec();
    invalid.qualifiedName = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name
  {
    OptionalSpec invalid = validNativeSpec();
    invalid.qualifiedName = "+-$/&%";
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name 2
  {
    OptionalSpec invalid = validNativeSpec();
    invalid.qualifiedName = "0ptionalName";
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name 3
  {
    OptionalSpec invalid = validNativeSpec();
    invalid.qualifiedName = "optional name";
    checkInvalidSpec(invalid);
  }
}

/// @test
/// Checks optional instance comparison
/// @requirements(SEN-355)
TEST(OptionalType, comparison)
{
  // different types
  {
    const auto& spec = validNativeSpec();
    auto type = OptionalType::make(spec);

    EXPECT_NE(type, sen::UInt8Type::get());
    EXPECT_NE(type, sen::Int16Type::get());
    EXPECT_NE(type, sen::UInt16Type::get());
    EXPECT_NE(type, sen::Int32Type::get());
    EXPECT_NE(type, sen::UInt32Type::get());
    EXPECT_NE(type, sen::Int64Type::get());
    EXPECT_NE(type, sen::UInt64Type::get());
    EXPECT_NE(type, sen::Float32Type::get());
    EXPECT_NE(type, sen::Float64Type::get());
    EXPECT_NE(type, sen::BoolType::get());
    EXPECT_NE(type, sen::StringType::get());
  }

  // same content
  {
    const auto& spec = validNativeSpec();
    auto type1 = OptionalType::make(spec);
    auto type2 = OptionalType::make(spec);
    EXPECT_EQ(*type1, *type2);
  }

  // same instance
  {
    const auto& spec = validNativeSpec();
    auto type1 = OptionalType::make(spec);
    EXPECT_EQ(*type1, *type1);
  }

  // different name
  {
    const auto& lhs = validNativeSpec();
    OptionalSpec rhs = validNativeSpec();
    rhs.name = validNativeSpec().name + "X";

    EXPECT_NE(*OptionalType::make(lhs), *OptionalType::make(rhs));
  }

  // different qualified name
  {
    const auto& lhs = validNativeSpec();
    OptionalSpec rhs = validNativeSpec();
    rhs.qualifiedName = validNativeSpec().qualifiedName + "X";
    EXPECT_NE(*OptionalType::make(lhs), *OptionalType::make(rhs));
  }

  // different description
  {
    const auto& lhs = validNativeSpec();
    OptionalSpec rhs = validNativeSpec();
    rhs.description = validNativeSpec().description + "X";
    EXPECT_NE(*OptionalType::make(lhs), *OptionalType::make(rhs));
  }

  // different description 2
  {
    const auto& lhs = validNativeSpec();
    OptionalSpec rhs = validNativeSpec();
    rhs.description = "";
    EXPECT_NE(*OptionalType::make(lhs), *OptionalType::make(rhs));
  }

  // different type
  {
    const auto& lhs = validNativeSpec();
    OptionalSpec rhs = validNativeSpec();
    rhs.type = sen::StringType::get();
    EXPECT_NE(*OptionalType::make(lhs), *OptionalType::make(rhs));
  }

  // same native type but double optional wrap
  {
    const auto& lhs = validNativeSpec();
    const auto& rhs =
      OptionalSpec {"DoubleOptionalTest", "DoubleOptionalTest", "", OptionalType::make(validNativeSpec())};

    EXPECT_NE(*OptionalType::make(lhs), *OptionalType::make(rhs));
  }

  // different Type ptr
  {
    const auto& lhs = OptionalType::make(validNativeSpec());
    const auto& rhs = sen::StringType::get();

    EXPECT_NE(lhs, rhs);
  }
}
