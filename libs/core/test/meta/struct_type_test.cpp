// === struct_type_test.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/span.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/time_types.h"

// google test
#include <gtest/gtest.h>

// std
#include <exception>
#include <tuple>
#include <vector>

using sen::BoolType;
using sen::Int32Type;
using sen::StringType;
using sen::StructField;
using sen::StructSpec;
using sen::StructType;

namespace
{
const std::vector<StructField>& validFields()
{
  static const std::vector<StructField> validFields = {
    {"a", "first", Int32Type::get()}, {"b", "second", StringType::get()}, {"c", "third", BoolType::get()}};
  return validFields;
}

const StructSpec& validSpec()
{
  static const StructSpec validSpec = {"TestStruct", "ns.TestStruct", "Valid struct", validFields()};
  return validSpec;
}

const std::vector<StructField>& validChildFields()
{
  static const std::vector<StructField> validChildFields = {};
  return validChildFields;
}

const StructSpec& validChildSpec()
{
  static const StructSpec validChildSpec = {
    "TestChildStruct", "ns.TestChildStruct", "Valid child", validChildFields(), StructType::make(validSpec())};
  return validChildSpec;
}

const std::vector<StructField>& validGrandchildFields()
{
  static const std::vector<StructField> validGrandchildFields = {{"d", "", sen::UInt8Type::get()},
                                                                 {"e", "", sen::TimestampType::get()}};
  return validGrandchildFields;
}

const StructSpec& validGrandchildSpec()
{
  static const StructSpec validGrandchildSpec = {"TestGrandchildStruct",
                                                 "ns.TestGrandchildStruct",
                                                 "Valid grandchild",
                                                 validGrandchildFields(),
                                                 StructType::make(validChildSpec())};
  return validGrandchildSpec;
}

void checkSpecData(const StructType& type, const StructSpec& spec)
{
  EXPECT_EQ(type.getName(), spec.name);
  EXPECT_EQ(type.getQualifiedName(), spec.qualifiedName);
  EXPECT_EQ(type.getDescription(), spec.description);

  EXPECT_EQ(type.getFields(), sen::makeConstSpan(spec.fields));

  if (type.getParent())
  {
    EXPECT_EQ(*type.getParent(), *spec.parent);
  }
}

void checkInvalidSpec(const StructSpec& spec) { EXPECT_THROW(std::ignore = StructType::make(spec), std::exception); }

}  // namespace

/// @test
/// Checks struct field comparison
/// @requirements(SEN-579)
TEST(StructType, field)
{
  // same
  {
    StructField lhs {"one", "first", StringType::get()};
    StructField rhs {"one", "first", StringType::get()};

    EXPECT_EQ(lhs, lhs);
    EXPECT_EQ(lhs, rhs);
  }

  // different by name
  {
    StructField lhs {"one", "first", StringType::get()};
    StructField rhs {"oneX", "first", StringType::get()};

    EXPECT_NE(lhs, rhs);
  }

  // different by description
  {
    StructField lhs {"one", "first", StringType::get()};
    StructField rhs {"one", "firstX", StringType::get()};

    EXPECT_NE(lhs, rhs);
  }

  // different by type
  {
    StructField lhs {"one", "first", StringType::get()};
    StructField rhs {"one", "first", Int32Type::get()};

    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks struct spec comparison
/// @requirements(SEN-579)
TEST(StructSpec, specComparison)
{
  const auto& lhs = validSpec();

  // same
  {
    const StructSpec& rhs = validSpec();
    EXPECT_EQ(lhs, rhs);
  }

  // different name
  {
    StructSpec rhs = validSpec();
    rhs.name = validSpec().name + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different qualified name
  {
    StructSpec rhs = validSpec();
    rhs.qualifiedName = validSpec().qualifiedName + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different description
  {
    StructSpec rhs = validSpec();
    rhs.description = validSpec().description + "X";
    EXPECT_NE(lhs, rhs);
  }

  // extra fields
  {
    StructSpec rhs = validSpec();
    rhs.fields.emplace_back("four", "fourth", StringType::get());
    EXPECT_NE(lhs, rhs);
  }

  // fewer fields
  {
    StructSpec rhs = validSpec();
    rhs.fields.pop_back();
    EXPECT_NE(lhs, rhs);
  }

  // no fields
  {
    StructSpec rhs = validSpec();
    rhs.fields = {};
    EXPECT_NE(lhs, rhs);
  }

  // different fields
  {
    StructSpec rhs = validSpec();
    rhs.fields.front().name = lhs.fields.front().name + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different hash (should be the same)
  {
    const StructSpec& rhs = validSpec();
    EXPECT_EQ(lhs, rhs);
  }

  // one with no parent
  {
    const std::vector<StructField> fields = {{"d", "first", Int32Type::get()}};
    const StructSpec parentSpec = {"TestParent", "ns.TestParent", "Valid parent", fields};

    auto parent = StructType::make(parentSpec);

    StructSpec rhs = validSpec();
    rhs.parent = parent;

    EXPECT_NE(lhs, rhs);
  }

  // different parents
  {
    const std::vector<StructField> fields = {{"d", "first", Int32Type::get()}};

    const StructSpec parent1Spec = {"TestParent", "ns.TestParent", "Valid parent", fields};
    const StructSpec parent2Spec = {"TestParent", "ns.TestParent2", "Valid parent", fields};

    StructSpec lhs = validSpec();
    StructSpec rhs = validSpec();

    lhs.parent = StructType::make(parent1Spec);
    rhs.parent = StructType::make(parent2Spec);

    EXPECT_NE(lhs, rhs);
  }

  // two levels of inheritance
  {
    const StructSpec lhsBaseBase = {"TestBase", "TestBase", "", {{"zero", "0", sen::DurationType::get()}}};
    const StructSpec lhsBase = {
      "TestMid", "TestMid", "", {{"first", "1", sen::TimestampType::get()}}, sen::StructType::make(lhsBaseBase)};
    const StructSpec lhs = {
      "TestChild", "TestChild", "", {{"second", "2", sen::Int32Type::get()}}, sen::StructType::make(lhsBase)};

    const StructSpec rhsBaseBase = {"TestBase", "TestBase", "", {{"zero", "3", sen::DurationType::get()}}};
    const StructSpec rhsBase = {
      "TestMid", "TestMid", "", {{"first", "1", sen::TimestampType::get()}}, sen::StructType::make(rhsBaseBase)};
    const StructSpec rhs = {
      "TestChild", "TestChild", "", {{"second", "2", sen::Int32Type::get()}}, sen::StructType::make(rhsBase)};

    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks struct instance type
/// @requirements(SEN-579)
TEST(StructType, basicsBoolConversion)
{
  auto type = StructType::make(validSpec());

  // things that structs are
  EXPECT_TRUE(type->isCustomType());
  EXPECT_TRUE(type->isStructType());
  EXPECT_EQ(type->isBounded(), false);

  // things that structs aren't
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
  EXPECT_FALSE(type->isVariantType());
  EXPECT_FALSE(type->isQuantityType());
  EXPECT_FALSE(type->isAliasType());
}

/// @test
/// Checks struct instance type
/// @requirements(SEN-579)
TEST(StructType, basicsConversion)
{
  auto type = StructType::make(validSpec());

  // things that structs are
  EXPECT_NE(type->asCustomType(), nullptr);
  EXPECT_NE(type->asStructType(), nullptr);
  EXPECT_EQ(type->isBounded(), false);

  // things that structs aren't
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
  EXPECT_EQ(type->asVariantType(), nullptr);
  EXPECT_EQ(type->asQuantityType(), nullptr);
  EXPECT_EQ(type->asAliasType(), nullptr);
}

/// @test
/// Checks correct struct instance creation
/// @requirements(SEN-579)
TEST(StructType, makeBasic)
{
  const auto& spec = validSpec();
  auto type = StructType::make(spec);
  checkSpecData(*type, spec);
}

/// @test
/// Checks invalid struct creation
/// @requirements(SEN-579)
TEST(StructType, makeInvalid)
{
  // spec with no name
  {
    StructSpec invalid = validSpec();
    invalid.name = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid name
  {
    StructSpec invalid = validSpec();
    invalid.name = "someStruct";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 2
  {
    StructSpec invalid = validSpec();
    invalid.name = "Some Struct";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 3
  {
    StructSpec invalid = validSpec();
    invalid.name = "1234";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 4
  {
    StructSpec invalid = validSpec();
    invalid.name = "$&%/()";
    checkInvalidSpec(invalid);
  }

  // spec with no qualified name
  {
    StructSpec invalid = validSpec();
    invalid.qualifiedName = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name
  {
    StructSpec invalid = validSpec();
    invalid.name = "$/·()";
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name 2
  {
    StructSpec invalid = validSpec();
    invalid.name = "0Struct";
    checkInvalidSpec(invalid);
  }

  // spec with invalid qualified name 3
  {
    StructSpec invalid = validSpec();
    invalid.name = "invalid name";
    checkInvalidSpec(invalid);
  }

  // spec with invalid fields (repeated names)
  {
    StructSpec invalid = validSpec();
    invalid.fields = {
      {"a", "first", Int32Type::get()}, {"a", "second", StringType::get()}, {"c", "third", BoolType::get()}};

    checkInvalidSpec(invalid);
  }

  // spec with invalid fields (invalid name)
  {
    StructSpec invalid = validSpec();
    invalid.fields = {{"AnInvalidField", "first", Int32Type::get()},
                      {"b", "second", StringType::get()},
                      {"c", "third", BoolType::get()}};

    checkInvalidSpec(invalid);
  }

  // spec with invalid fields (empty name)
  {
    StructSpec invalid = validSpec();
    invalid.fields.front().name = {};
    checkInvalidSpec(invalid);
  }

  // parent with a field with the same name
  {
    const std::vector<StructField> fields = {{"a", "first", Int32Type::get()}};
    const StructSpec parentSpec = {"TestParent", "ns.TestParent", "Valid parent", fields};

    StructSpec invalid = validSpec();
    auto parent = StructType::make(parentSpec);
    invalid.parent = parent;

    checkInvalidSpec(invalid);
  }

  // parent with a field with the same name but different type and description
  {
    const std::vector<StructField> fields = {{"a", "second", sen::UInt8Type::get()}};
    const StructSpec parentSpec = {"TestParent", "ns.TestParent", "Valid parent", fields};

    StructSpec invalid = validSpec();
    auto parent = StructType::make(parentSpec);
    invalid.parent = parent;

    checkInvalidSpec(invalid);
  }

  // repeated field in top base spec in 2 level inheritance
  {
    const StructSpec specBaseBase = {"TestSpec", "TestSpec", "", {{"second", "2", sen::DurationType::get()}}};
    const StructSpec specBase = {
      "TestSpec", "TestSpec", "", {{"first", "1", sen::TimestampType::get()}}, sen::StructType::make(specBaseBase)};
    const StructSpec spec = {
      "TestSpec", "TestSpec", "", {{"second", "2", sen::Int32Type::get()}}, sen::StructType::make(specBase)};

    const StructSpec& invalid = spec;
    checkInvalidSpec(invalid);
  }
}

/// @test
/// Checks struct instance comparison
/// @requirements(SEN-579)
TEST(StructType, comparison)
{
  // different types
  {
    const auto& spec = validSpec();
    auto type = StructType::make(spec);

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
    EXPECT_NE(*type, *sen::TimestampType::get());
    EXPECT_NE(*type, *sen::DurationType::get());
  }

  // same content
  {
    const auto& spec = validSpec();
    auto type1 = StructType::make(spec);
    auto type2 = StructType::make(spec);
    EXPECT_EQ(type1, type2);
  }

  // same instance
  {
    const auto& spec = validSpec();
    auto type1 = StructType::make(spec);
    EXPECT_EQ(*type1, *type1);
  }

  // different name
  {
    const auto& lhs = validSpec();
    StructSpec rhs = validSpec();
    rhs.name = validSpec().name + "X";

    EXPECT_NE(*StructType::make(lhs), *StructType::make(rhs));
  }

  // different qualified name
  {
    const auto& lhs = validSpec();
    StructSpec rhs = validSpec();
    rhs.qualifiedName = validSpec().qualifiedName + "X";
    EXPECT_NE(*StructType::make(lhs), *StructType::make(rhs));
  }

  // different description
  {
    const auto& lhs = validSpec();
    StructSpec rhs = validSpec();
    rhs.description = validSpec().description + "X";
    EXPECT_NE(*StructType::make(lhs), *StructType::make(rhs));
  }

  // extra fields
  {
    const auto& lhs = validSpec();
    StructSpec rhs = validSpec();
    rhs.fields.emplace_back("four", "fourth", Int32Type::get());
    EXPECT_NE(*StructType::make(lhs), *StructType::make(rhs));
  }

  // fewer fields
  {
    const auto& lhs = validSpec();
    StructSpec rhs = validSpec();
    rhs.fields.pop_back();
    EXPECT_NE(*StructType::make(lhs), *StructType::make(rhs));
  }

  // different fields
  {
    const auto& lhs = validSpec();
    StructSpec rhs = validSpec();
    rhs.fields.front().name = lhs.fields.front().name + "X";
    EXPECT_NE(*StructType::make(lhs), *StructType::make(rhs));
  }

  // no field
  {
    const auto& lhs = validSpec();
    StructSpec rhs = validSpec();
    rhs.fields = {};
    EXPECT_NE(*StructType::make(lhs), *StructType::make(rhs));
  }

  // different parent
  {
    const auto& lhs = validSpec();
    StructSpec rhs = validSpec();

    const std::vector<StructField> fields = {{"e", "e", Int32Type::get()}};
    const StructSpec parentSpec = {"TestParent", "ns.TestParent", "Valid parent", fields};
    auto parent = StructType::make(parentSpec);
    rhs.parent = parent;

    EXPECT_NE(*StructType::make(lhs), *StructType::make(rhs));
  }

  // different field in parent
  {
    const std::vector<StructField> fields1 = {{"d", "first", sen::UInt32Type::get()}};
    const StructSpec parent1Spec = {"TestParent", "ns.TestParent", "Valid parent", fields1};

    const std::vector<StructField> fields2 = {{"d", "first", Int32Type::get()}};
    const StructSpec parent2Spec = {"TestParent", "ns.TestParent", "Valid parent", fields2};

    StructSpec llhs = validSpec();
    StructSpec rhs = validSpec();

    auto lhsParent = StructType::make(parent1Spec);
    auto rhsParent = StructType::make(parent2Spec);

    llhs.parent = lhsParent;
    rhs.parent = rhsParent;

    EXPECT_NE(*StructType::make(llhs), *StructType::make(rhs));
  }

  // same parents and grandparents
  {
    auto lhsGrandpa = StructType::make(validSpec());
    auto rhsGrandpa = StructType::make(validSpec());

    auto lhsParent = StructType::make(validChildSpec());
    auto rhsParent = StructType::make(validChildSpec());

    auto lhs = StructType::make(validGrandchildSpec());
    auto rhs = StructType::make(validGrandchildSpec());

    EXPECT_EQ(lhs, rhs);
  }

  // same grandparent but different parent
  {
    // modify rhs parent
    auto rhsGrandpa = StructType::make(validSpec());
    const std::vector<StructField> fields2 = {{"z", "last", Int32Type::get()}};
    const StructSpec parent2Spec("TestParent", "ns.TestParent", "Valid parent", fields2, rhsGrandpa);
    auto rhsParent = StructType::make(parent2Spec);

    // modify rhs grandchild parent reference
    auto rhsGrandchildSpec = validGrandchildSpec();
    rhsGrandchildSpec.parent = rhsParent;

    auto lhs = StructType::make(validGrandchildSpec());
    auto rhs = StructType::make(rhsGrandchildSpec);

    EXPECT_NE(*lhs, *rhs);
  }

  // same parent but different grandparent fields
  {
    auto lhsGrandpa = StructType::make({"TestStruct", "ns.TestStruct", "Valid struct", validFields()});
    auto rhsGrandpa = StructType::make({"TestStruct", "ns.TestStruct", "Valid struct", {}});

    auto lhsParent =
      StructType::make({"TestChildStruct", "ns.TestChildStruct", "Valid child", validChildFields(), lhsGrandpa});
    auto rhsParent =
      StructType::make({"TestChildStruct", "ns.TestChildStruct", "Valid child", validChildFields(), rhsGrandpa});

    auto lhs = StructType::make(
      {"TestGrandchildStruct", "ns.TestGrandchildStruct", "Valid grandchild", validGrandchildFields(), lhsParent});
    auto rhs = StructType::make(
      {"TestGrandchildStruct", "ns.TestGrandchildStruct", "Valid grandchild", validGrandchildFields(), rhsParent});

    EXPECT_NE(*lhs, *rhs);
  }

  // different description in grandparent field
  {
    const StructSpec lhsBaseBase = {"TestBase", "TestBase", "", {{"zero", "0", sen::DurationType::get()}}};
    const StructSpec lhsBase = {
      "TestMid", "TestMid", "", {{"first", "1", sen::TimestampType::get()}}, sen::StructType::make(lhsBaseBase)};
    const StructSpec lhs = {
      "TestChild", "TestChild", "", {{"second", "2", sen::Int32Type::get()}}, sen::StructType::make(lhsBase)};

    const StructSpec rhsBaseBase = {"TestBase", "TestBase", "", {{"zero", "3", sen::DurationType::get()}}};
    const StructSpec rhsBase = {
      "TestMid", "TestMid", "", {{"first", "1", sen::TimestampType::get()}}, sen::StructType::make(rhsBaseBase)};
    const StructSpec rhs = {
      "TestChild", "TestChild", "", {{"second", "2", sen::Int32Type::get()}}, sen::StructType::make(rhsBase)};

    EXPECT_NE(*StructType::make(lhs), *StructType::make(rhs));
  }
}

/// @test
/// Checks local field getter
/// @requirements(SEN-579)
TEST(StructType, getFieldFromLocal)
{
  // basic struct
  {
    const auto& spec = validSpec();
    const auto type = StructType::make(spec);

    // all keys and names should be found
    {
      const auto fields = type->getFields();
      for (const auto& item: fields)
      {
        EXPECT_NE(type->getFieldFromName(item.name), nullptr);
      }
    }

    // test with invalid names
    EXPECT_EQ(type->getFieldFromName("invalid"), nullptr);
    EXPECT_EQ(type->getFieldFromName(""), nullptr);
  }

  // inherited struct with empty fields but with fields in parent
  {
    const auto& spec = validChildSpec();
    const auto type = StructType::make(spec);

    // all keys and names should be found
    {
      const auto fields = type->getFields();
      EXPECT_EQ(fields.size(), 0U);
    }

    // test trying to get parent field by name
    EXPECT_NE(type->getFieldFromName("a"), nullptr);
    EXPECT_NE(type->getFieldFromName("b"), nullptr);
    EXPECT_NE(type->getFieldFromName("c"), nullptr);

    // test invalid field names
    EXPECT_EQ(type->getFieldFromName("does_not_exist"), nullptr);
    EXPECT_EQ(type->getFieldFromName(""), nullptr);
    EXPECT_EQ(type->getFieldFromName({}), nullptr);
  }
}

/// @test
/// Checks parent field getter
/// @requirements(SEN-579)
TEST(StructType, getFieldFromParent)
{
  StructSpec parentSpec = {"Parent", "ns.Parent", "Valid struct", {}, {}};
  parentSpec.fields = {{"a", "a", Int32Type::get()},
                       {"b", "b", Int32Type::get()},
                       {"c", "c", Int32Type::get()},
                       {"d", "d", Int32Type::get()}};

  auto parentType = StructType::make(parentSpec);
  EXPECT_EQ(parentType->getFields().size(), 4U);

  StructSpec childSpec = {"Child", "ns.Child", "Valid struct", {}, parentType};
  childSpec.fields = {{"e", "e", Int32Type::get()},
                      {"f", "f", Int32Type::get()},
                      {"g", "h", Int32Type::get()},
                      {"h", "h", Int32Type::get()}};

  auto childType = StructType::make(childSpec);
  // ensure child only gets its local fields
  EXPECT_EQ(childType->getFields().size(), 4U);

  // all keys and names should be found
  {
    // search for the parent fields
    for (const auto& parentField: parentSpec.fields)
    {
      EXPECT_NE(parentType->getFieldFromName(parentField.name), nullptr);

      // here child should be able to find parent field by name
      EXPECT_NE(childType->getFieldFromName(parentField.name), nullptr);
    }

    // search for the child fields
    for (const auto& childField: childSpec.fields)
    {
      // parent type does not hold child fields
      EXPECT_EQ(parentType->getFieldFromName(childField.name), nullptr);

      // here we should be able to find also self-child fields
      EXPECT_NE(childType->getFieldFromName(childField.name), nullptr);
    }
  }
}
