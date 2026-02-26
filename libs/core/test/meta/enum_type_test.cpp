// === enum_type_test.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/hash32.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/native_types.h"

// google test
#include <gtest/gtest.h>

// std
#include <exception>
#include <tuple>
#include <vector>

using sen::Enumerator;
using sen::EnumSpec;
using sen::EnumType;

namespace
{

const std::vector<Enumerator>& validEnums()
{
  static const std::vector<Enumerator> validEnums = {{"one", 1, "first"}, {"two", 2, "second"}, {"three", 3, "third"}};
  return validEnums;
}

const EnumSpec& validSpec()
{
  static const EnumSpec validSpec = {"TestEnum", "ns.TestEnum", "Valid enum", validEnums(), sen::UInt8Type::get()};
  return validSpec;
}

void checkSpecData(const EnumType& type, const EnumSpec& spec)
{
  EXPECT_EQ(type.getName(), spec.name);
  EXPECT_EQ(type.getQualifiedName(), spec.qualifiedName);
  EXPECT_EQ(type.getDescription(), spec.description);
  EXPECT_EQ(type.getStorageType(), *spec.storageType);
  EXPECT_EQ(type.getEnums(), sen::makeConstSpan(spec.enums));
}

void checkInvalidSpec(const EnumSpec& spec) { EXPECT_THROW(std::ignore = EnumType::make(spec), std::exception); }

}  // namespace

/// @test
/// Checks enumerator comparison
/// @requirements(SEN-902)
TEST(EnumType, enumerator)
{
  // same
  {
    Enumerator lhs {"one", 1, "first"};
    Enumerator rhs {"one", 1, "first"};

    EXPECT_EQ(lhs, lhs);
    EXPECT_EQ(lhs, rhs);
  }

  // different by name
  {
    Enumerator lhs {"one", 1, "first"};
    Enumerator rhs {"oneX", 1, "first"};

    EXPECT_NE(lhs, rhs);
  }

  // different by key
  {
    Enumerator lhs {"one", 1, "first"};
    Enumerator rhs {"one", 2, "first"};

    EXPECT_NE(lhs, rhs);
  }

  // different by description
  {
    Enumerator lhs {"one", 1, "first"};
    Enumerator rhs {"one", 2, "firstX"};

    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks enum spec comparison
/// @requirements(SEN-902)
TEST(EnumType, specComparison)
{
  const auto& lhs = validSpec();

  // same
  {
    const EnumSpec& rhs = validSpec();
    EXPECT_EQ(lhs, rhs);
  }

  // different name
  {
    EnumSpec rhs = validSpec();
    rhs.name = validSpec().name + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different qualified name
  {
    EnumSpec rhs = validSpec();
    rhs.qualifiedName = validSpec().qualifiedName + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different description
  {
    EnumSpec rhs = validSpec();
    rhs.description = validSpec().description + "X";
    EXPECT_NE(lhs, rhs);
  }

  // extra enums
  {
    EnumSpec rhs = validSpec();

    std::vector<Enumerator> enums(validSpec().enums.begin(), validSpec().enums.end());
    enums.push_back({"four", 4, "fourth"});
    rhs.enums = enums;
    EXPECT_NE(lhs, rhs);
  }

  // fewer enums
  {
    EnumSpec rhs = validSpec();
    rhs.enums.pop_back();
    EXPECT_NE(lhs, rhs);
  }

  // different enums
  {
    EnumSpec rhs = validSpec();
    rhs.enums.front().name = lhs.enums.front().name + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different storage type
  {
    EnumSpec rhs = validSpec();
    rhs.storageType = sen::UInt16Type::get();
    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks enum type basic getters
/// @requirements(SEN-902)
TEST(EnumType, basicGetters)
{
  const auto& spec = validSpec();
  const auto type = EnumType::make(spec);

  EXPECT_EQ(type->getName(), "TestEnum");
  EXPECT_EQ(type->getQualifiedName(), "ns.TestEnum");
  EXPECT_EQ(type->getDescription(), "Valid enum");
  EXPECT_EQ(type->getEnums(), sen::Span<const Enumerator>(validEnums()));
}

/// @test
/// Checks enum type visitor method
/// @requirements(SEN-902)
TEST(EnumType, basicsBoolConversion)
{
  auto type = EnumType::make(validSpec());

  // things that enums are
  EXPECT_TRUE(type->isCustomType());
  EXPECT_TRUE(type->isEnumType());

  // things that enums aren't
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
  EXPECT_FALSE(type->isQuantityType());
  EXPECT_FALSE(type->isAliasType());
}

/// @test
/// Checks enum type visitor method
/// @requirements(SEN-902)
TEST(EnumType, basicsConversion)
{
  auto type = EnumType::make(validSpec());

  // things that enums are
  EXPECT_NE(type->asCustomType(), nullptr);
  EXPECT_NE(type->asEnumType(), nullptr);

  // things that enums aren't
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
  EXPECT_EQ(type->asQuantityType(), nullptr);
  EXPECT_EQ(type->asAliasType(), nullptr);
}

/// @test
/// Checks correct enum type instance from spec
/// @requirements(SEN-902)
TEST(EnumType, makeBasic)
{
  const auto& spec = validSpec();
  auto type = EnumType::make(spec);
  checkSpecData(*type, spec);
}

/// @test
/// Checks invalid enumerator creation
/// @requirements(SEN-902)
TEST(EnumType, makeInvalid)
{
  // spec with no name
  {
    EnumSpec invalid = validSpec();
    invalid.name = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid name
  {
    EnumSpec invalid = validSpec();
    invalid.name = "someEnum";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 2
  {
    EnumSpec invalid = validSpec();
    invalid.name = "4valid";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 3
  {
    EnumSpec invalid = validSpec();
    invalid.name = "valid type";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 4
  {
    EnumSpec invalid = validSpec();
    invalid.name = "·$/$";
    checkInvalidSpec(invalid);
  }

  // spec with no qualified name
  {
    EnumSpec invalid = validSpec();
    invalid.qualifiedName = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name
  {
    EnumSpec invalid = validSpec();
    invalid.qualifiedName = "·$$&%";
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name 2
  {
    EnumSpec invalid = validSpec();
    invalid.qualifiedName = "qualified name";
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name 3
  {
    EnumSpec invalid = validSpec();
    invalid.qualifiedName = "8ualifiedName";
    checkInvalidSpec(invalid);
  }

  // spec with no enumerators
  {
    EnumSpec invalid = validSpec();
    invalid.enums = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid enumerators (repeated names)
  {
    EnumSpec invalid = validSpec();
    invalid.enums = {{"one", 1, "first"}, {"two", 2, "second"}, {"one", 3, "third"}};
    checkInvalidSpec(invalid);
  }

  // spec with invalid enumerators (repeated keys)
  {
    EnumSpec invalid = validSpec();
    invalid.enums = {{"one", 1, "first"}, {"two", 2, "second"}, {"three", 1, "third"}};
    checkInvalidSpec(invalid);
  }
}

/// @test
/// Checks comparison between shared ptr enum type
/// @requirements(SEN-902)
TEST(EnumType, comparison)
{
  // different types
  {
    const auto& spec = validSpec();
    auto type = EnumType::make(spec);

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
    auto type1 = EnumType::make(spec);
    auto type2 = EnumType::make(spec);
    EXPECT_EQ(*type1, *type2);
  }

  // same instance
  {
    const auto& spec = validSpec();
    auto type1 = EnumType::make(spec);
    EXPECT_EQ(*type1, *type1);
  }

  // different name
  {
    const auto& lhs = validSpec();
    EnumSpec rhs = validSpec();
    rhs.name = validSpec().name + "X";

    EXPECT_NE(*EnumType::make(lhs), *EnumType::make(rhs));
  }

  // different qualified name
  {
    const auto& lhs = validSpec();
    EnumSpec rhs = validSpec();
    rhs.qualifiedName = validSpec().qualifiedName + "X";
    EXPECT_NE(*EnumType::make(lhs), *EnumType::make(rhs));
  }

  // different description
  {
    const auto& lhs = validSpec();
    EnumSpec rhs = validSpec();
    rhs.description = validSpec().description + "X";
    EXPECT_NE(*EnumType::make(lhs), *EnumType::make(rhs));
  }

  // extra enums
  {
    const auto& lhs = validSpec();
    EnumSpec rhs = validSpec();

    std::vector<Enumerator> enums(validSpec().enums.begin(), validSpec().enums.end());
    enums.push_back({"four", 4, "fourth"});
    rhs.enums = enums;
    EXPECT_NE(*EnumType::make(lhs), *EnumType::make(rhs));
  }

  // fewer enums
  {
    const auto& lhs = validSpec();
    EnumSpec rhs = validSpec();
    rhs.enums.pop_back();
    EXPECT_NE(*EnumType::make(lhs), *EnumType::make(rhs));
  }

  // different enums
  {
    const auto& lhs = validSpec();
    EnumSpec rhs = validSpec();
    rhs.enums.front().name = lhs.enums.front().name + "X";
    EXPECT_NE(*EnumType::make(lhs), *EnumType::make(rhs));
  }

  // different storage type
  {
    const auto& lhs = validSpec();
    EnumSpec rhs = validSpec();
    rhs.storageType = sen::UInt16Type::get();
    EXPECT_NE(*EnumType::make(lhs), *EnumType::make(rhs));
  }
}

/// @test
/// Checks enumerator getters
/// @requirements(SEN-902)
TEST(EnumType, getEnumFrom)
{
  const auto& spec = validSpec();
  const auto type = EnumType::make(spec);

  // all keys and names should be found
  {
    const auto enums = type->getEnums();
    for (const auto& item: enums)
    {
      EXPECT_NE(type->getEnumFromKey(item.key), nullptr);
      EXPECT_NE(type->getEnumFromName(item.name), nullptr);
    }
  }

  // test with invalid keys and names
  EXPECT_EQ(type->getEnumFromKey(42), nullptr);
  EXPECT_EQ(type->getEnumFromKey(0), nullptr);
  EXPECT_EQ(type->getEnumFromName("invalid"), nullptr);
  EXPECT_EQ(type->getEnumFromName(""), nullptr);
}

/// @test
/// Checks hash combine
/// @requirements(SEN-902)
TEST(EnumType, hashCombine)
{
  const auto& spec = validSpec();
  const auto type = EnumType::make(spec);

  const auto enums = type->getEnums();
  sen::impl::hash<sen::Enumerator> hash;
  for (const auto& item: enums)
  {
    EXPECT_EQ(hash(item), sen::hashCombine(sen::hashSeed, item.name, item.key));
  }
}
