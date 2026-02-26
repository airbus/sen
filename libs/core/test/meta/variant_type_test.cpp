// === variant_type_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/span.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/variant_type.h"

// google test
#include <gtest/gtest.h>

// std
#include <exception>
#include <tuple>
#include <vector>

using sen::BoolType;
using sen::Int32Type;
using sen::StringType;
using sen::VariantField;
using sen::VariantSpec;
using sen::VariantType;

namespace
{

const VariantSpec& validSpec()
{
  static const std::vector<VariantField> validFields = {
    {1, "a", Int32Type::get()}, {2, "b", StringType::get()}, {3, "c", BoolType::get()}};

  static const VariantSpec validSpec = {"TestVariant", "ns.TestVariant", "Valid variant", validFields};
  return validSpec;
}

const VariantSpec& validNestedSpec()
{
  static const std::vector<VariantField> nestedVariantField = {{1, "", VariantType::make(validSpec())},
                                                               {2, "", sen::TimestampType::get()},
                                                               {3, "", sen::DurationType::get()},
                                                               {4, "", sen::Int32Type::get()}};

  static const VariantSpec validNestedSpec = {
    "TestVariantOfVariant", "ns.TestVariantOfVariant", "Valid variant", nestedVariantField};

  return validNestedSpec;
}

void checkSpecData(const VariantType& type, const VariantSpec& spec)
{
  EXPECT_EQ(type.getName(), spec.name);
  EXPECT_EQ(type.getQualifiedName(), spec.qualifiedName);
  EXPECT_EQ(type.getDescription(), spec.description);
  EXPECT_EQ(type.getFields(), sen::makeConstSpan(spec.fields));
}

void checkInvalidSpec(const VariantSpec& spec) { EXPECT_THROW(std::ignore = VariantType::make(spec), std::exception); }

}  // namespace

/// @test
/// Checks variant field comparisons
/// @requirements(SEN-579)
TEST(VariantType, field)
{
  // same
  {
    VariantField lhs {1, "one", StringType::get()};
    VariantField rhs {1, "one", StringType::get()};

    EXPECT_EQ(lhs, lhs);
    EXPECT_EQ(lhs, rhs);
  }

  // different by key
  {
    VariantField lhs {1, "one", StringType::get()};
    VariantField rhs {2, "one", StringType::get()};

    EXPECT_NE(lhs, rhs);
  }

  // different by description
  {
    VariantField lhs {1, "one", StringType::get()};
    VariantField rhs {1, "oneX", StringType::get()};

    EXPECT_NE(lhs, rhs);
  }

  // different by type
  {
    VariantField lhs {1, "one", StringType::get()};
    VariantField rhs {1, "one", Int32Type::get()};

    EXPECT_NE(lhs, rhs);
  }

  // different by hash (still the same)
  {
    VariantField lhs {1, "one", StringType::get()};
    VariantField rhs {1, "one", StringType::get()};

    EXPECT_EQ(lhs, rhs);
  }
}

/// @test
/// Checks variant spec comparison
/// @requirements(SEN-579)
TEST(VariantSpec, specComparison)
{
  const auto& lhs = validSpec();

  // same
  {
    const VariantSpec& rhs = validSpec();
    EXPECT_EQ(lhs, rhs);
  }

  // different name
  {
    VariantSpec rhs = validSpec();
    rhs.name = validSpec().name + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different qualified name
  {
    VariantSpec rhs = validSpec();
    rhs.qualifiedName = validSpec().qualifiedName + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different description
  {
    VariantSpec rhs = validSpec();
    rhs.description = validSpec().description + "X";
    EXPECT_NE(lhs, rhs);
  }

  // extra fields
  {
    VariantSpec rhs = validSpec();
    rhs.fields.emplace_back(4, "four", StringType::get());
    EXPECT_NE(lhs, rhs);
  }

  // fewer fields
  {
    VariantSpec rhs = validSpec();
    rhs.fields.pop_back();
    EXPECT_NE(lhs, rhs);
  }

  // different fields
  {
    VariantSpec rhs = validSpec();
    rhs.fields.front().description = lhs.fields.front().description + "X";
    EXPECT_NE(lhs, rhs);
  }

  // no fields
  {
    VariantSpec rhs = validSpec();
    rhs.fields = {};
    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks variant instance type
/// @requirements(SEN-579)
TEST(VariantType, basicsBoolConversion)
{
  auto type = VariantType::make(validSpec());

  // things that variants are
  EXPECT_TRUE(type->isCustomType());
  EXPECT_TRUE(type->isVariantType());

  // check variant is considered as not bounded when some field is not bounded
  EXPECT_FALSE(type->isBounded());

  // things that variants aren't
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
  EXPECT_FALSE(type->isClassType());
  EXPECT_FALSE(type->isStructType());
  EXPECT_FALSE(type->isQuantityType());
  EXPECT_FALSE(type->isAliasType());
}

/// @test
/// Checks variant instance type
/// @requirements(SEN-579)
TEST(VariantType, basicsConversion)
{
  auto type = VariantType::make(validSpec());

  // things that variants are
  EXPECT_NE(type->asCustomType(), nullptr);
  EXPECT_NE(type->asVariantType(), nullptr);

  // check variant is considered as not bounded when some field is not bounded
  EXPECT_FALSE(type->isBounded());

  // things that variants aren't
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
  EXPECT_EQ(type->asClassType(), nullptr);
  EXPECT_EQ(type->asStructType(), nullptr);
  EXPECT_EQ(type->asQuantityType(), nullptr);
  EXPECT_EQ(type->asAliasType(), nullptr);
}

/// @test
/// Checks correct variant instance creation from spec
/// @requirements(SEN-579)
TEST(VariantType, makeBasic)
{
  // simple variant
  {
    const auto& spec = validSpec();
    auto type = VariantType::make(spec);
    checkSpecData(*type, spec);
  }

  // variant of variant
  {
    const auto& spec = validNestedSpec();
    auto type = VariantType::make(spec);
    checkSpecData(*type, spec);
  }

  // variant of variant with unsorted keys values
  {
    auto spec = validNestedSpec();
    spec.fields[0].key = 100;

    auto type = VariantType::make(spec);
    checkSpecData(*type, spec);
  }
}

/// @test
/// Checks invalid variant creation
/// @requirements(SEN-579)
TEST(VariantType, makeInvalid)
{
  // spec with no name
  {
    VariantSpec invalid = validSpec();
    invalid.name = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid name
  {
    VariantSpec invalid = validSpec();
    invalid.name = "someVariant";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 2
  {
    VariantSpec invalid = validSpec();
    invalid.name = "Some variant";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 3
  {
    VariantSpec invalid = validSpec();
    invalid.name = "6omeVariant";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 4
  {
    VariantSpec invalid = validSpec();
    invalid.name = "=)(!&";
    checkInvalidSpec(invalid);
  }

  // spec with no qualified name
  {
    VariantSpec invalid = validSpec();
    invalid.qualifiedName = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name
  {
    VariantSpec invalid = validSpec();
    invalid.name = "some variant";
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name 2
  {
    VariantSpec invalid = validSpec();
    invalid.name = "6omeVariant";
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name 3
  {
    VariantSpec invalid = validSpec();
    invalid.name = "$%/(!=";
    checkInvalidSpec(invalid);
  }

  // spec with invalid fields (repeated keys)
  {
    VariantSpec invalid = validSpec();
    invalid.fields = {{1, "a", Int32Type::get()}, {2, "b", StringType::get()}, {1, "c", BoolType::get()}};

    checkInvalidSpec(invalid);
  }

  // spec with invalid fields (repeated type)
  {
    VariantSpec invalid = validSpec();
    invalid.fields = {{1, "a", Int32Type::get()}, {2, "b", StringType::get()}, {1, "c", Int32Type::get()}};

    checkInvalidSpec(invalid);
  }
}

/// @test
/// Checks comparison of different variant types
/// @requirements(SEN-579)
TEST(VariantType, comparison)
{
  // different types
  {
    const auto& spec = validSpec();
    auto type = VariantType::make(spec);

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
    auto type1 = VariantType::make(spec);
    auto type2 = VariantType::make(spec);
    EXPECT_EQ(*type1, *type2);
  }

  // same instance
  {
    const auto& spec = validSpec();
    auto type1 = VariantType::make(spec);
    EXPECT_EQ(*type1, *type1);
  }

  // different name
  {
    const auto& lhs = validSpec();
    VariantSpec rhs = validSpec();
    rhs.name = validSpec().name + "X";

    EXPECT_NE(*VariantType::make(lhs), *VariantType::make(rhs));
  }

  // different qualified name
  {
    const auto& lhs = validSpec();
    VariantSpec rhs = validSpec();
    rhs.qualifiedName = validSpec().qualifiedName + "X";
    EXPECT_NE(*VariantType::make(lhs), *VariantType::make(rhs));
  }

  // different description
  {
    const auto& lhs = validSpec();
    VariantSpec rhs = validSpec();
    rhs.description = validSpec().description + "X";
    EXPECT_NE(*VariantType::make(lhs), *VariantType::make(rhs));
  }

  // extra fields
  {
    const auto& lhs = validSpec();
    VariantSpec rhs = validSpec();
    rhs.fields.emplace_back(4, "four", sen::UInt16Type::get());
    EXPECT_NE(*VariantType::make(lhs), *VariantType::make(rhs));
  }

  // fewer fields
  {
    const auto& lhs = validSpec();
    VariantSpec rhs = validSpec();
    rhs.fields.pop_back();
    EXPECT_NE(*VariantType::make(lhs), *VariantType::make(rhs));
  }

  // different fields
  {
    const auto& lhs = validSpec();
    VariantSpec rhs = validSpec();
    rhs.fields.front().type = sen::UInt8Type::get();
    EXPECT_NE(*VariantType::make(lhs), *VariantType::make(rhs));
  }

  // no fields in variant type
  {
    const auto& lhs = validNestedSpec();
    VariantSpec rhs = validNestedSpec();
    rhs.fields[0].type = sen::VariantType::make({"TestVariant", "ns.TestVariant", "Valid variant", {}});
    EXPECT_NE(*VariantType::make(lhs), *VariantType::make(rhs));
  }
}

/// @test
/// Checks variant field from key getter
/// @requirements(SEN-579)
TEST(VariantType, getFieldFromKey)
{
  // basic variant type
  {
    const auto& spec = validSpec();
    const auto type = VariantType::make(spec);

    // all keys and names should be found
    {
      const auto fields = type->getFields();
      for (const auto& item: fields)
      {
        EXPECT_NE(type->getFieldFromKey(item.key), nullptr);
      }
    }

    // test with invalid keys
    EXPECT_EQ(type->getFieldFromKey(42), nullptr);
    EXPECT_EQ(type->getFieldFromKey(0), nullptr);
  }

  // variant of variant and other types
  {
    const auto& spec = validNestedSpec();
    const auto type = VariantType::make(spec);

    // all keys and names should be found
    {
      const auto fields = type->getFields();
      for (const auto& item: fields)
      {
        EXPECT_NE(type->getFieldFromKey(item.key), nullptr);
      }
    }

    // test with invalid keys
    EXPECT_EQ(type->getFieldFromKey(32), nullptr);
    EXPECT_EQ(type->getFieldFromKey(0), nullptr);
  }
}
