// === quantity_type_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/unit_registry.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <tuple>

using sen::QuantitySpec;
using sen::QuantityType;
using sen::Unit;
using sen::UnitCategory;
using sen::UnitSpec;

namespace
{

const UnitSpec& unitSpec()
{
  static const UnitSpec unitSpec = {UnitCategory::length, "meter", "meters", "m", 1.0, 0.0, 0.0};
  return unitSpec;
}

const Unit* getValidUnit()
{
  static auto unit = Unit::make(unitSpec());
  return unit.get();
}

const QuantitySpec& validSpec()
{
  static const QuantitySpec validSpec = {
    "MyQuantity", "MyQuantity", "desc", sen::UInt8Type::get(), getValidUnit(), 0.0, 10.0};
  return validSpec;
}

void checkSpecData(const QuantityType& type, const QuantitySpec& spec)
{
  EXPECT_EQ(type.getDescription(), spec.description);
  EXPECT_EQ(*type.getElementType(), *spec.elementType);
  EXPECT_EQ(type.getMaxValue(), spec.maxValue);
  EXPECT_EQ(type.getMinValue(), spec.minValue);
  EXPECT_EQ(type.getName(), spec.name);
  EXPECT_EQ(type.getQualifiedName(), spec.qualifiedName);
  EXPECT_EQ(type.getElementType()->getHash(), spec.elementType->getHash());
}

void checkInvalidSpec(const QuantitySpec& spec)
{
  EXPECT_THROW(std::ignore = QuantityType::make(spec), std::exception);
}

}  // namespace

/// @test
/// Checks quantity spec comparison
/// @requirements(SEN-355)
TEST(QuantityType, specComparison)
{
  const auto& lhs = validSpec();

  // same
  {
    const QuantitySpec& rhs = validSpec();
    EXPECT_EQ(lhs, rhs);
  }

  // different description
  {
    QuantitySpec rhs = validSpec();
    rhs.description = validSpec().description + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different element type
  {
    QuantitySpec rhs = validSpec();
    rhs.elementType = sen::UInt64Type::get();
    EXPECT_NE(lhs, rhs);
  }

  // different max
  {
    QuantitySpec rhs = validSpec();
    rhs.maxValue = validSpec().maxValue.value() + 1;
    EXPECT_NE(lhs, rhs);
  }

  // different min
  {
    QuantitySpec rhs = validSpec();
    rhs.minValue = validSpec().minValue.value() - 1;
    EXPECT_NE(lhs, rhs);
  }

  // no max
  {
    QuantitySpec rhs = validSpec();
    rhs.maxValue = std::nullopt;
    EXPECT_NE(lhs, rhs);
  }

  // no min
  {
    QuantitySpec rhs = validSpec();
    rhs.minValue = std::nullopt;
    EXPECT_NE(lhs, rhs);
  }

  // no max, no min
  {
    QuantitySpec rhs = validSpec();
    rhs.maxValue = std::nullopt;
    rhs.minValue = std::nullopt;
    EXPECT_NE(lhs, rhs);
  }

  // different unit
  {
    auto unit = Unit::make(UnitSpec {UnitCategory::temperature, "centigrade", "centigrades", "c", 1.0, 0.0, 0.0});

    QuantitySpec rhs = validSpec();
    rhs.unit = unit.get();

    EXPECT_NE(lhs, rhs);
  }

  // different unit, just
  {
    QuantitySpec rhs = validSpec();

    UnitSpec unitSpec2 = unitSpec();
    unitSpec2.f = unitSpec().f - 0.01;
    auto unit2 = Unit::make(unitSpec2);
    rhs.unit = unit2.get();

    EXPECT_NE(lhs, rhs);
  }

  // no unit
  {
    QuantitySpec rhs = validSpec();
    rhs.unit = {};
    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Check quantity correct type
/// @requirements(SEN-355)
TEST(QuantityType, basicsBoolConversion)
{
  auto type = QuantityType::make(validSpec());
  EXPECT_TRUE(type->isBounded());

  // things that quantities are
  EXPECT_TRUE(type->isCustomType());
  EXPECT_TRUE(type->isQuantityType());

  // things that quantities aren't
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
  EXPECT_FALSE(type->isStructType());
  EXPECT_FALSE(type->isClassType());
  EXPECT_FALSE(type->isVariantType());
  EXPECT_FALSE(type->isEnumType());
  EXPECT_FALSE(type->isAliasType());
}

/// @test
/// Check quantity correct type
/// @requirements(SEN-355)
TEST(QuantityType, basicsConversion)
{
  auto type = QuantityType::make(validSpec());
  EXPECT_TRUE(type->isBounded());

  // things that quantities are
  EXPECT_NE(type->asCustomType(), nullptr);
  EXPECT_NE(type->asQuantityType(), nullptr);

  // things that quantities aren't
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
  EXPECT_EQ(type->asStructType(), nullptr);
  EXPECT_EQ(type->asClassType(), nullptr);
  EXPECT_EQ(type->asVariantType(), nullptr);
  EXPECT_EQ(type->asEnumType(), nullptr);
  EXPECT_EQ(type->asAliasType(), nullptr);
}

/// @test
/// Checks quantity correct creation
/// @requirements(SEN-355)
TEST(QuantityType, makeBasic)
{
  // basic
  {
    const auto& spec = validSpec();
    auto type = QuantityType::make(spec);
    checkSpecData(*type, spec);
  }

  // min value = max value
  {
    auto unit = std::make_shared<Unit>(*sen::UnitRegistry::get().searchUnitByName("kelvin").value());
    sen::Int64Type localInt;
    auto spec = QuantitySpec {
      "TestQuantity2", "ns.TestQuantity2", "", sen::makeNonOwningTypeHandle(&localInt), unit.get(), 1.0002, 1.0002};
    auto type = QuantityType::make(spec);
    checkSpecData(*type, spec);
  }

  // no min or max values
  {
    auto unit = std::make_shared<Unit>(*sen::UnitRegistry::get().searchUnitByName("degree").value());
    sen::Int64Type localInt64;
    auto spec = QuantitySpec {
      "TestQuantity3", "ns.TestQuantity3", "", sen::makeNonOwningTypeHandle(&localInt64), unit.get(), {}, {}};
    auto type = QuantityType::make(spec);
    checkSpecData(*type, spec);
  }

  // no unit
  {
    sen::Float64Type localFloat64;
    QuantitySpec spec(
      "TestQuantity4", "ns.TestQuantity4", "", sen::makeNonOwningTypeHandle(&localFloat64), {}, 2.54, 2.54000001);
    auto type = QuantityType::make(spec);
    checkSpecData(*type, spec);
  }
}

/// @test
/// Checks quantity invalid creation
/// @requirements(SEN-355)
TEST(QuantityType, makeInvalid)
{
  // spec with no name
  {
    QuantitySpec invalid = validSpec();
    invalid.name = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid name
  {
    QuantitySpec invalid = validSpec();
    invalid.name = "$%&/(";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 2
  {
    QuantitySpec invalid = validSpec();
    invalid.name = "1nvalidName";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 3
  {
    QuantitySpec invalid = validSpec();
    invalid.name = "invalid name";
    checkInvalidSpec(invalid);
  }

  // spec with empty qualified name
  {
    QuantitySpec invalid = validSpec();
    invalid.qualifiedName = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name
  {
    QuantitySpec invalid = validSpec();
    invalid.qualifiedName = "%$&/()";
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name 2
  {
    QuantitySpec invalid = validSpec();
    invalid.qualifiedName = "687";
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name 3
  {
    QuantitySpec invalid = validSpec();
    invalid.qualifiedName = "example Name";
    checkInvalidSpec(invalid);
  }

  // spec with a min larger than the max
  {
    QuantitySpec invalid = validSpec();
    invalid.minValue = invalid.maxValue.value() + 0.00001;
    checkInvalidSpec(invalid);
  }

  // spec with too large min value
  {
    QuantitySpec invalid = validSpec();
    invalid.elementType = sen::Int16Type::get();
    invalid.minValue = std::numeric_limits<int16_t>::lowest() - 1;
    checkInvalidSpec(invalid);
  }

  // spec with too large max value
  {
    QuantitySpec invalid = validSpec();
    invalid.elementType = sen::Int16Type::get();
    invalid.maxValue = std::numeric_limits<int16_t>::max() + 1;
    checkInvalidSpec(invalid);
  }
}

/// @test
/// Checks quantity instance comparison
/// @requirements(SEN-355)
TEST(QuantityType, comparison)
{
  // different types
  {
    const auto& spec = validSpec();
    auto type = QuantityType::make(spec);

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
    const auto& spec = validSpec();
    auto type1 = QuantityType::make(spec);
    auto type2 = QuantityType::make(spec);
    EXPECT_EQ(*type1, *type2);
  }

  // same instance
  {
    const auto& spec = validSpec();
    auto type1 = QuantityType::make(spec);
    EXPECT_EQ(*type1, *type1);
  }

  // different description
  {
    const auto& lhs = validSpec();
    QuantitySpec rhs = lhs;
    rhs.description = lhs.description + "X";
    EXPECT_NE(*QuantityType::make(lhs), *QuantityType::make(rhs));
  }

  // different element type
  {
    const auto& lhs = validSpec();
    QuantitySpec rhs = lhs;
    rhs.elementType = sen::UInt64Type::get();
    EXPECT_NE(*QuantityType::make(lhs), *QuantityType::make(rhs));
  }

  // different max
  {
    QuantitySpec lhs = validSpec();
    lhs.maxValue = 5.0;
    lhs.minValue = 1.0;

    QuantitySpec rhs = lhs;
    rhs.maxValue = *lhs.maxValue - 1.0;
    rhs.minValue = lhs.minValue;

    EXPECT_NE(*QuantityType::make(lhs), *QuantityType::make(rhs));
  }

  // different min
  {
    QuantitySpec lhs = validSpec();
    lhs.maxValue = 5.0;
    lhs.minValue = 1.0;

    QuantitySpec rhs = lhs;
    rhs.maxValue = lhs.maxValue;
    rhs.minValue = *lhs.minValue - 1.0;
    EXPECT_NE(*QuantityType::make(lhs), *QuantityType::make(rhs));
  }

  // no max
  {
    const auto& lhs = validSpec();
    QuantitySpec rhs = lhs;
    rhs.maxValue = std::nullopt;
    EXPECT_NE(*QuantityType::make(lhs), *QuantityType::make(rhs));
  }

  // no min
  {
    const auto& lhs = validSpec();
    QuantitySpec rhs = lhs;
    rhs.minValue = std::nullopt;
    EXPECT_NE(*QuantityType::make(lhs), *QuantityType::make(rhs));
  }

  // no max, no min
  {
    const auto& lhs = validSpec();
    QuantitySpec rhs = lhs;
    rhs.maxValue = std::nullopt;
    rhs.minValue = std::nullopt;
    EXPECT_NE(*QuantityType::make(lhs), *QuantityType::make(rhs));
  }

  // different unit
  {
    const auto& lhs = validSpec();
    QuantitySpec rhs = lhs;

    auto unit = Unit::make(UnitSpec {UnitCategory::temperature, "centigrade", "centigrades", "c", 1.0, 0.0, 0.0});
    rhs.unit = unit.get();

    EXPECT_NE(*QuantityType::make(lhs), *QuantityType::make(rhs));
  }

  // different unit, just
  {
    const auto& lhs = validSpec();
    QuantitySpec rhs = lhs;

    UnitSpec unitSpec2 = unitSpec();
    unitSpec2.f = unitSpec().f - 0.01;

    auto unit2 = Unit::make(unitSpec2);
    rhs.unit = unit2.get();

    EXPECT_NE(*QuantityType::make(lhs), *QuantityType::make(rhs));
  }
}
