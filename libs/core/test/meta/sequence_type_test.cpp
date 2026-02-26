// === sequence_type_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/time_types.h"

// google test
#include <gtest/gtest.h>

// std
#include <exception>
#include <optional>
#include <tuple>

using sen::SequenceSpec;
using sen::SequenceType;

namespace
{

const SequenceSpec& validBoundedSpec()
{
  static const SequenceSpec validBoundedSpec(
    "TestSequence1", "ns.TestSequence1", "bounded sequence of strings", sen::StringType::get(), 10U);

  return validBoundedSpec;
}

const SequenceSpec& validArraySpec()
{
  static const SequenceSpec validArraySpec(
    "TestSequence2", "ns.TestSequence2", "array sequence of int32", sen::Int32Type::get(), 3U, true);
  return validArraySpec;
}

const SequenceSpec& validUnboundedSpec()
{
  static const SequenceSpec validUnboundedSpec(
    "TestSequence3", "ns.TestSequence3", "bounded sequence of timestamps", sen::TimestampType::get());
  return validUnboundedSpec;
}

void checkSpecData(const SequenceType& type, const SequenceSpec& spec)
{
  EXPECT_EQ(type.getName(), spec.name);
  EXPECT_EQ(type.getQualifiedName(), spec.qualifiedName);
  EXPECT_EQ(type.getDescription(), spec.description);
  EXPECT_EQ(*type.getElementType(), *spec.elementType);
  EXPECT_EQ(type.getMaxSize().has_value(), spec.maxSize.has_value());
  EXPECT_EQ(type.getElementType()->getHash(), spec.elementType->getHash());

  if (type.getMaxSize().has_value())
  {
    EXPECT_EQ(type.getMaxSize().value(), spec.maxSize.value());
  }
}

void checkInvalidSpec(const SequenceSpec& spec)
{
  EXPECT_THROW(std::ignore = SequenceType::make(spec), std::exception);
}

}  // namespace

/// @test
/// Checks bounded sequence spec comparison
/// @requirements(SEN-579)
TEST(SequenceType, BoundedSpecComparison)
{
  const auto& lhs = validBoundedSpec();

  // same
  {
    const SequenceSpec& rhs = validBoundedSpec();
    EXPECT_EQ(lhs, rhs);
  }

  // different name
  {
    SequenceSpec rhs = validBoundedSpec();
    rhs.name = validBoundedSpec().name + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different qualified name
  {
    SequenceSpec rhs = validBoundedSpec();
    rhs.qualifiedName = validBoundedSpec().qualifiedName + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different description
  {
    SequenceSpec rhs = validBoundedSpec();
    rhs.description = validBoundedSpec().description + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different bounds
  {
    SequenceSpec rhs = validBoundedSpec();
    rhs.maxSize = lhs.maxSize.value() + 1U;

    EXPECT_NE(lhs, rhs);
  }

  // absent bounds
  {
    SequenceSpec rhs = validBoundedSpec();
    rhs.maxSize = std::nullopt;
    EXPECT_NE(lhs, rhs);
  }

  // different element type
  {
    SequenceSpec rhs = validBoundedSpec();
    rhs.elementType = sen::UInt16Type::get();
    EXPECT_NE(lhs, rhs);
  }

  // different fixed
  {
    SequenceSpec rhs = validBoundedSpec();
    rhs.fixedSize = !lhs.fixedSize;
    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks unbounded sequence spec comparison
/// @requirements(SEN-579)
TEST(SequenceType, UnboundedSpecComparison)
{
  const auto& lhs = validUnboundedSpec();

  // same
  {
    const SequenceSpec& rhs = validUnboundedSpec();
    EXPECT_EQ(lhs, rhs);
  }

  // different name
  {
    SequenceSpec rhs = validUnboundedSpec();
    rhs.name = validBoundedSpec().name + "Z";
    EXPECT_NE(lhs, rhs);
  }

  // different qualified name
  {
    SequenceSpec rhs = validUnboundedSpec();
    rhs.qualifiedName = validBoundedSpec().qualifiedName + "Z";
    EXPECT_NE(lhs, rhs);
  }

  // different description
  {
    SequenceSpec rhs = validUnboundedSpec();
    rhs.description = validBoundedSpec().description + "Z";
    EXPECT_NE(lhs, rhs);
  }

  // compare with bounded one
  {
    SequenceSpec rhs = validUnboundedSpec();
    rhs.maxSize = 29U;

    EXPECT_NE(lhs, rhs);
  }

  // same absent bounds
  {
    SequenceSpec rhs = validUnboundedSpec();
    rhs.maxSize = std::nullopt;
    EXPECT_EQ(lhs, rhs);
  }

  // different element type
  {
    SequenceSpec rhs = validUnboundedSpec();
    rhs.elementType = sen::UInt32Type::get();
    EXPECT_NE(lhs, rhs);
  }

  // different fixed
  {
    SequenceSpec rhs = validUnboundedSpec();
    rhs.fixedSize = !lhs.fixedSize;
    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks array sequence spec comparison
/// @requirements(SEN-579)
TEST(SequenceType, ArraySpecComparison)
{
  const auto& lhs = validArraySpec();

  // same
  {
    const SequenceSpec& rhs = validArraySpec();
    EXPECT_EQ(lhs, rhs);
  }

  // different name
  {
    SequenceSpec rhs = validArraySpec();
    rhs.name = validBoundedSpec().name + "Y";
    EXPECT_NE(lhs, rhs);
  }

  // different qualified name
  {
    SequenceSpec rhs = validArraySpec();
    rhs.qualifiedName = validBoundedSpec().qualifiedName + "Y";
    EXPECT_NE(lhs, rhs);
  }

  // different description
  {
    SequenceSpec rhs = validArraySpec();
    rhs.description = validBoundedSpec().description + "Y";
    EXPECT_NE(lhs, rhs);
  }

  // compare with bounded one
  {
    SequenceSpec rhs = validArraySpec();
    rhs.maxSize = 44U;

    EXPECT_NE(lhs, rhs);
  }

  // absent bounds
  {
    SequenceSpec rhs = validArraySpec();
    rhs.maxSize = std::nullopt;
    EXPECT_NE(lhs, rhs);
  }

  // different element type
  {
    SequenceSpec rhs = validArraySpec();
    rhs.elementType = sen::UInt32Type::get();
    EXPECT_NE(lhs, rhs);
  }

  // different fixed
  {
    SequenceSpec rhs = validArraySpec();
    rhs.fixedSize = !lhs.fixedSize;
    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks bounded sequence instance type
/// @requirements(SEN-579)
TEST(SequenceType, basicsBoundedBoolConvertion)
{
  auto type = SequenceType::make(validBoundedSpec());

  // things that sequences are
  EXPECT_TRUE(type->isCustomType());
  EXPECT_TRUE(type->isSequenceType());
  EXPECT_TRUE(type->isBounded());

  // things that sequences aren't
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
  EXPECT_FALSE(type->isEnumType());
  EXPECT_FALSE(type->isStructType());
  EXPECT_FALSE(type->isClassType());
  EXPECT_FALSE(type->isVariantType());
  EXPECT_FALSE(type->isQuantityType());
  EXPECT_FALSE(type->isAliasType());
}

/// @test
/// Checks bounded sequence instance type
/// @requirements(SEN-579)
TEST(SequenceType, basicsBoundedConversion)
{
  auto type = SequenceType::make(validBoundedSpec());

  // things that sequences are
  EXPECT_NE(type->asCustomType(), nullptr);
  EXPECT_NE(type->asSequenceType(), nullptr);
  EXPECT_TRUE(type->isBounded());

  // things that sequences aren't
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
  EXPECT_EQ(type->asEnumType(), nullptr);
  EXPECT_EQ(type->asStructType(), nullptr);
  EXPECT_EQ(type->asClassType(), nullptr);
  EXPECT_EQ(type->asVariantType(), nullptr);
  EXPECT_EQ(type->asQuantityType(), nullptr);
  EXPECT_EQ(type->asAliasType(), nullptr);
}

/// @test
/// Checks unbounded sequence instance type
/// @requirements(SEN-579)
TEST(SequenceType, basicsUnboundedBoolConversion)
{
  auto type = SequenceType::make(validUnboundedSpec());

  // things that sequences are
  EXPECT_TRUE(type->isCustomType());
  EXPECT_TRUE(type->isSequenceType());
  EXPECT_FALSE(type->isBounded());

  // things that sequences aren't
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
  EXPECT_FALSE(type->isTimestampType());
  EXPECT_FALSE(type->isEnumType());
  EXPECT_FALSE(type->isStructType());
  EXPECT_FALSE(type->isClassType());
  EXPECT_FALSE(type->isVariantType());
  EXPECT_FALSE(type->isQuantityType());
  EXPECT_FALSE(type->isAliasType());
}

/// @test
/// Checks unbounded sequence instance type
/// @requirements(SEN-579)
TEST(SequenceType, basicsUnboundedConversion)
{
  auto type = SequenceType::make(validUnboundedSpec());

  // things that sequences are
  EXPECT_NE(type->asCustomType(), nullptr);
  EXPECT_NE(type->asSequenceType(), nullptr);
  EXPECT_FALSE(type->isBounded());

  // things that sequences aren't
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
  EXPECT_EQ(type->asTimestampType(), nullptr);
  EXPECT_EQ(type->asEnumType(), nullptr);
  EXPECT_EQ(type->asStructType(), nullptr);
  EXPECT_EQ(type->asClassType(), nullptr);
  EXPECT_EQ(type->asVariantType(), nullptr);
  EXPECT_EQ(type->asQuantityType(), nullptr);
  EXPECT_EQ(type->asAliasType(), nullptr);
}

/// @test
/// Checks array sequence instance type
/// @requirements(SEN-579)
TEST(SequenceType, basicsArrayBoolConversion)
{
  auto type = SequenceType::make(validArraySpec());

  // things that sequences are
  EXPECT_TRUE(type->isCustomType());
  EXPECT_TRUE(type->isSequenceType());
  EXPECT_TRUE(type->isBounded());

  // things that sequences aren't
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
  EXPECT_FALSE(type->isTimestampType());
  EXPECT_FALSE(type->isEnumType());
  EXPECT_FALSE(type->isStructType());
  EXPECT_FALSE(type->isClassType());
  EXPECT_FALSE(type->isVariantType());
  EXPECT_FALSE(type->isQuantityType());
  EXPECT_FALSE(type->isAliasType());
}

/// @test
/// Checks array sequence instance type
/// @requirements(SEN-579)
TEST(SequenceType, basicsArrayConversion)
{
  auto type = SequenceType::make(validArraySpec());

  // things that sequences are
  EXPECT_NE(type->asCustomType(), nullptr);
  EXPECT_NE(type->asSequenceType(), nullptr);
  EXPECT_TRUE(type->isBounded());

  // things that sequences aren't
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
  EXPECT_EQ(type->asTimestampType(), nullptr);
  EXPECT_EQ(type->asEnumType(), nullptr);
  EXPECT_EQ(type->asStructType(), nullptr);
  EXPECT_EQ(type->asClassType(), nullptr);
  EXPECT_EQ(type->asVariantType(), nullptr);
  EXPECT_EQ(type->asQuantityType(), nullptr);
  EXPECT_EQ(type->asAliasType(), nullptr);
}

/// @test
/// CHecks correct sequence instant creation from spec
/// @requirements(SEN-579)
TEST(SequenceType, makeBasic)
{
  {
    const auto& spec = validBoundedSpec();
    auto type = SequenceType::make(spec);
    checkSpecData(*type, spec);
  }

  {
    const SequenceSpec spec(
      "TestSequence", "ns.TestSequence", "unbounded sequence of int32", sen::Int32Type::get(), std::nullopt);

    auto type = SequenceType::make(spec);
    checkSpecData(*type, spec);
  }
}

/// @test
/// Checks invalid creation of sequence specs
/// @requirements(SEN-579)
TEST(SequenceType, makeInvalid)
{
  // spec with no name
  {
    SequenceSpec invalid = validBoundedSpec();
    invalid.name = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid name
  {
    SequenceSpec invalid = validArraySpec();
    invalid.name = "someSpec";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 2
  {
    SequenceSpec invalid = validArraySpec();
    invalid.name = "2omeName";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 3
  {
    SequenceSpec invalid = validArraySpec();
    invalid.name = "&%$/()";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 4
  {
    SequenceSpec invalid = validArraySpec();
    invalid.name = "Some name";
    checkInvalidSpec(invalid);
  }

  // spec with no qualified name
  {
    SequenceSpec invalid = validUnboundedSpec();
    invalid.qualifiedName = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name
  {
    SequenceSpec invalid = validArraySpec();
    invalid.name = "some spec";
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name 2
  {
    SequenceSpec invalid = validArraySpec();
    invalid.name = "&%$/()";
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name 3
  {
    SequenceSpec invalid = validArraySpec();
    invalid.name = "0name";
    checkInvalidSpec(invalid);
  }
}

/// @test
/// Checks sequence type comparison
/// @requirements(SEN-579)
TEST(SequenceType, comparison)
{  // different types
  {
    const auto& spec = validBoundedSpec();
    auto type = SequenceType::make(spec);

    EXPECT_NE(*type, *sen::UInt8Type::get());
    EXPECT_NE(*type, *sen::Int16Type::get());
    EXPECT_NE(*type, *sen::UInt16Type::get());
    EXPECT_NE(*type, *sen::Int32Type::get());
    EXPECT_NE(*type, *sen::UInt32Type::get());
    EXPECT_NE(*type, *sen::Int64Type::get());
    EXPECT_NE(*type, *sen::UInt64Type::get());
    EXPECT_NE(*type, *sen::Float32Type::get());
    EXPECT_NE(*type, *sen::Float64Type::get());
    EXPECT_NE(*type, *sen::BoolType::get());
    EXPECT_NE(*type, *sen::StringType::get());
  }

  // same content
  {
    const auto& spec = validArraySpec();
    auto type1 = SequenceType::make(spec);
    auto type2 = SequenceType::make(spec);
    EXPECT_EQ(*type1, *type2);
  }

  // same instance
  {
    const auto& spec = validUnboundedSpec();
    auto type1 = SequenceType::make(spec);
    EXPECT_EQ(*type1, *type1);
  }

  // different name
  {
    const auto& lhs = validUnboundedSpec();
    SequenceSpec rhs = validUnboundedSpec();
    rhs.name = validUnboundedSpec().name + "X";

    EXPECT_NE(*SequenceType::make(lhs), *SequenceType::make(rhs));
  }

  // different qualified name
  {
    const auto& lhs = validArraySpec();
    SequenceSpec rhs = validArraySpec();
    rhs.qualifiedName = validArraySpec().qualifiedName + "X";
    EXPECT_NE(*SequenceType::make(lhs), *SequenceType::make(rhs));
  }

  // different description
  {
    const auto& lhs = validBoundedSpec();
    SequenceSpec rhs = validBoundedSpec();
    rhs.description = validBoundedSpec().description + "X";
    EXPECT_NE(*SequenceType::make(lhs), *SequenceType::make(rhs));
  }

  // absent bounds
  {
    const auto& lhs = validBoundedSpec();
    SequenceSpec rhs = validBoundedSpec();
    rhs.maxSize = std::nullopt;
    EXPECT_NE(*SequenceType::make(lhs), *SequenceType::make(rhs));
  }

  // different bounds
  {
    const auto& lhs = validBoundedSpec();
    SequenceSpec rhs = validBoundedSpec();
    rhs.maxSize = lhs.maxSize.value() + 1U;
    EXPECT_NE(*SequenceType::make(lhs), *SequenceType::make(rhs));
  }

  // different element type
  {
    const auto& lhs = validBoundedSpec();
    SequenceSpec rhs = validBoundedSpec();
    rhs.elementType = sen::UInt16Type::get();
    EXPECT_NE(*SequenceType::make(lhs), *SequenceType::make(rhs));
  }

  // different fixed size type
  {
    const auto& lhs = validUnboundedSpec();
    SequenceSpec rhs = validUnboundedSpec();
    rhs.fixedSize = !lhs.fixedSize;
    EXPECT_NE(*SequenceType::make(lhs), *SequenceType::make(rhs));
  }
}
