// === alias_type_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"

// google test
#include <gtest/gtest.h>

// std
#include <exception>
#include <memory>
#include <tuple>
#include <vector>

using sen::AliasSpec;
using sen::AliasType;

namespace
{
const AliasSpec& validSpec()
{
  static const AliasSpec validSpec = {"TestAlias", "ns.TestAlias", "Valid alias", sen::UInt8Type::get()};
  return validSpec;
}

// alias of enum
const AliasSpec& validSpec2()
{
  static const sen::UInt16Type otherUInt16Type;
  static const sen::EnumSpec enumSpec = {
    "TestEnum", "TestEnum", "", {{"meters", 1, ""}, {"miles", 2, ""}}, sen::makeNonOwningTypeHandle(&otherUInt16Type)};

  static const AliasSpec validSpec2 = {"TestAlias", "ns.TestAlias", "Valid alias", sen::EnumType::make(enumSpec)};
  return validSpec2;
}

void checkSpecData(const AliasType& type, const AliasSpec& spec)
{
  EXPECT_EQ(type.getName(), spec.name);
  EXPECT_EQ(type.getQualifiedName(), spec.qualifiedName);
  EXPECT_EQ(type.getDescription(), spec.description);
  EXPECT_EQ(*type.getAliasedType(), *spec.aliasedType);
}

void checkInvalidSpec(const AliasSpec& spec) { EXPECT_THROW(std::ignore = AliasType::make(spec), std::exception); }

}  // namespace

/// @test
/// Checks alias spec comparison
/// @requirements(SEN-1055)
TEST(AliasType, specComparison)
{
  const auto& lhs = validSpec();

  // same
  {
    const AliasSpec& rhs = validSpec();
    EXPECT_EQ(lhs, rhs);
  }

  // different name
  {
    AliasSpec rhs = validSpec();
    rhs.name = validSpec().name + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different qualified name
  {
    AliasSpec rhs = validSpec();
    rhs.qualifiedName = validSpec().qualifiedName + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different description
  {
    AliasSpec rhs = validSpec();
    rhs.description = validSpec().description + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different aliased type
  {
    AliasSpec rhs = validSpec();
    rhs.aliasedType = sen::UInt16Type::get();
    EXPECT_NE(lhs, rhs);
  }

  // different aliased type (enum)
  {
    std::vector<sen::Enumerator> enumerator {{"enumValue1", 1, ""}, {"enumValue2", 2, ""}};

    sen::Int16Type llhsInt;
    sen::UInt16Type rhsInt;

    auto llhs = AliasSpec {
      "TestEnum",
      "ns.TestEnum",
      "",
      sen::EnumType::make({"EnumTest", "ns.EnumTest", "", enumerator, sen::makeNonOwningTypeHandle(&llhsInt)})};
    auto rhs = AliasSpec {
      "TestEnum",
      "ns.TestEnum",
      "",
      sen::EnumType::make({"EnumTest", "ns.EnumTest", "", enumerator, sen::makeNonOwningTypeHandle(&rhsInt)})};

    EXPECT_NE(llhs, rhs);
  }

  // different aliased type (alias type)
  {
    auto llhs = AliasSpec {"TestAliasOfAlias", "ns.TestAliasOfAlias", "", sen::AliasType::make(validSpec())};

    auto rhsSpec = validSpec();
    rhsSpec.aliasedType = sen::UInt16Type::get();
    auto rhs = AliasSpec {"TestAliasOfAlias", "ns.TestAliasOfAlias", "", sen::AliasType::make(rhsSpec)};

    EXPECT_NE(llhs, rhs);
  }

  // different aliased type (alias type) 2
  {
    auto llhs = AliasSpec {"TestAliasOfAlias", "ns.TestAliasOfAlias", "", sen::AliasType::make(validSpec())};

    auto rhs = AliasSpec {"TestAliasOfAlias", "ns.TestAliasOfAlias", "", sen::AliasType::make(validSpec2())};

    EXPECT_NE(llhs, rhs);
  }
}

/// @test
/// Checks alias type visitor method to check wrapped type
/// @requirements(SEN-1055)
TEST(AliasType, basicsBoolConversion)
{
  auto type = AliasType::make(validSpec());

  // things that aliases are
  EXPECT_TRUE(type->isCustomType());
  EXPECT_TRUE(type->isAliasType());
  EXPECT_EQ(type->isBounded(), type->getAliasedType()->isBounded());

  // things that aliases aren't
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
  EXPECT_FALSE(type->isEnumType());
  EXPECT_FALSE(type->isDurationType());
  EXPECT_FALSE(type->isSequenceType());
  EXPECT_FALSE(type->isStructType());
  EXPECT_FALSE(type->isClassType());
  EXPECT_FALSE(type->isVariantType());
  EXPECT_FALSE(type->isQuantityType());
}

/// @test
/// Checks alias type visitor method to check wrapped type
/// @requirements(SEN-1055)
TEST(AliasType, basicsConversion)
{
  auto type = AliasType::make(validSpec());

  // things that aliases are
  EXPECT_NE(type->asCustomType(), nullptr);
  EXPECT_NE(type->asAliasType(), nullptr);
  EXPECT_EQ(type->isBounded(), type->getAliasedType()->isBounded());

  // things that aliases aren't
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
  EXPECT_EQ(type->asEnumType(), nullptr);
  EXPECT_EQ(type->asDurationType(), nullptr);
  EXPECT_EQ(type->asSequenceType(), nullptr);
  EXPECT_EQ(type->asStructType(), nullptr);
  EXPECT_EQ(type->asClassType(), nullptr);
  EXPECT_EQ(type->asVariantType(), nullptr);
  EXPECT_EQ(type->asQuantityType(), nullptr);
}

/// @test
/// Checks alias type getters
/// @requirements(SEN-1055)
TEST(AliasType, makeBasic)
{
  // basic alias
  {
    const auto& spec = validSpec();
    auto type = AliasType::make(spec);
    checkSpecData(*type, spec);
  }

  // alias of alias
  {
    const auto& spec = AliasSpec {"TestAliasOfAlias", "ns.TestAliasOfAlias", "", sen::AliasType::make(validSpec())};
    auto type = AliasType::make(spec);
    checkSpecData(*type, spec);
  }

  // alias of enum
  {
    const auto& spec = validSpec2();
    auto type = AliasType::make(spec);
    checkSpecData(*type, spec);
  }
}

/// @test
/// Checks invalid alias spec creation
/// @requirements(SEN-1055)
TEST(AliasType, makeInvalid)
{
  // spec with no name
  {
    AliasSpec invalid = validSpec();
    invalid.name = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid name
  {
    AliasSpec invalid = validSpec();
    invalid.name = "someEnum";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 2
  {
    AliasSpec invalid = validSpec();
    invalid.name = "23someAlias";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 3
  {
    AliasSpec invalid = validSpec();
    invalid.name = "some alias";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 4
  {
    AliasSpec invalid = validSpec();
    invalid.name = "$$($";
    checkInvalidSpec(invalid);
  }

  // spec with no qualified name
  {
    AliasSpec invalid = validSpec();
    invalid.qualifiedName = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name
  {
    AliasSpec invalid = validSpec();
    invalid.qualifiedName = "$&%(·)";
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name 2
  {
    AliasSpec invalid = validSpec();
    invalid.qualifiedName = "7type";
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name 3
  {
    AliasSpec invalid = validSpec();
    invalid.qualifiedName = "spaces in name";
    checkInvalidSpec(invalid);
  }
}

/// @test
/// Checks shared ptr alias type comparison
/// @requirements(SEN-1055)
TEST(AliasType, comparison)
{
  // different types
  {
    const auto& spec = validSpec();
    auto type = AliasType::make(spec);

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
    auto type1 = AliasType::make(spec);
    auto type2 = AliasType::make(spec);
    EXPECT_EQ(*type1, *type2);
  }

  // same instance
  {
    const auto& spec = validSpec();
    auto type1 = AliasType::make(spec);
    EXPECT_EQ(*type1, *type1);
  }

  // different name
  {
    const auto& lhs = validSpec();
    AliasSpec rhs = validSpec();
    rhs.name = validSpec().name + "X";

    EXPECT_NE(*AliasType::make(lhs), *AliasType::make(rhs));
  }

  // different qualified name
  {
    const auto& lhs = validSpec();
    AliasSpec rhs = validSpec();
    rhs.qualifiedName = validSpec().qualifiedName + "X";
    EXPECT_NE(*AliasType::make(lhs), *AliasType::make(rhs));
  }

  // different description
  {
    const auto& lhs = validSpec();
    AliasSpec rhs = validSpec();
    rhs.description = validSpec().description + "X";
    EXPECT_NE(*AliasType::make(lhs), *AliasType::make(rhs));
  }

  // different aliased type
  {
    const auto& lhs = validSpec();
    AliasSpec rhs = validSpec();
    rhs.aliasedType = sen::UInt16Type::get();
    EXPECT_NE(*AliasType::make(lhs), *AliasType::make(rhs));
  }

  // different aliased type 2
  {
    const auto& lhs = validSpec();
    const auto& rhs = validSpec2();

    EXPECT_NE(*AliasType::make(lhs), *AliasType::make(rhs));
  }

  // different aliased type of alias type
  {
    const auto lhs = AliasSpec {"TestAliasOfAlias", "ns.TestAliasOfAlias", "", AliasType::make(validSpec())};
    auto rhs = AliasSpec {"TestAliasOfAlias", "ns.TestAliasOfAlias", "", AliasType::make(validSpec())};
    rhs.aliasedType = sen::TimestampType::get();
    EXPECT_NE(*AliasType::make(lhs), *AliasType::make(rhs));
  }

  // different aliased type
  {
    const auto lhs = AliasSpec {"TestAliasOfAlias", "ns.TestAliasOfAlias", "", AliasType::make(validSpec())};
    auto rhs = AliasSpec {"TestAliasOfAlias", "ns.TestAliasOfAlias", "", AliasType::make(validSpec2())};

    EXPECT_NE(*AliasType::make(lhs), *AliasType::make(rhs));
  }

  // different aliased alias type description
  {
    const auto lhs = AliasSpec {"TestAliasOfAlias", "ns.TestAliasOfAlias", "", AliasType::make(validSpec())};
    auto rhs = AliasSpec {"TestAliasOfAlias", "ns.TestAliasOfAlias", "Z", AliasType::make(validSpec())};
    EXPECT_NE(*AliasType::make(lhs), *AliasType::make(rhs));
  }

  // different basic type (aliased inside or not)
  {
    const auto& lhs = validSpec();
    auto rhs = AliasSpec {"TestAlias", "ns.TestAlias", "Valid alias", AliasType::make(validSpec())};
    EXPECT_NE(*AliasType::make(lhs), *AliasType::make(rhs));
  }
}
