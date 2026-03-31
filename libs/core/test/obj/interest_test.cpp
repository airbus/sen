// === interest_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/hash32.h"
#include "sen/core/lang/vm.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/variant_type.h"
#include "sen/core/obj/interest.h"

// google test
#include <gtest/gtest.h>

// std
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sen
{
bool operator==(const TypeCondition& lhs, const TypeCondition& rhs) noexcept;
bool operator!=(const TypeCondition& lhs, const TypeCondition& rhs) noexcept;
}  // namespace sen

using sen::BusSpec;
using sen::ClassSpec;
using sen::ClassType;
using sen::Constness;
using sen::Interest;
using sen::PropertyCategory;
using sen::StructField;
using sen::StructSpec;
using sen::StructType;
using sen::TransportMode;
using sen::VariantField;
using sen::VariantSpec;
using sen::VariantType;

namespace
{

StructSpec validStructSpec()
{
  const std::vector<StructField> validStructFields = {
    {"x", "", sen::Float32Type::get()}, {"y", "", sen::Float32Type::get()}, {"z", "", sen::Float32Type::get()}};

  const StructSpec structSpec = {"TestStruct", "ns.TestStruct", "Valid struct", validStructFields};

  return structSpec;
}

VariantSpec validVariantSpec()
{
  const std::vector<VariantField> validVariantFields = {{1, "", sen::Float64Type::get()},
                                                        {2, "", sen::Float32Type::get()},
                                                        {3, "", sen::Int64Type::get()},
                                                        {4, "", sen::Int32Type::get()}};

  const VariantSpec variantSpec = {"TestVariant", "ns.TestVariant", "Valid variant", validVariantFields};

  return variantSpec;
}

ClassSpec validClassSpec()
{
  const auto properties = std::vector<sen::PropertySpec> {
    {"objectID", "", sen::Int16Type::get(), PropertyCategory::staticRO, TransportMode::confirmed, false, {}},
    {"direction",
     "",
     VariantType::make(validVariantSpec()),
     PropertyCategory::dynamicRW,
     TransportMode::unicast,
     true,
     {}},
    {"coordinates",
     "",
     StructType::make(validStructSpec()),
     PropertyCategory::staticRW,
     TransportMode::multicast,
     true,
     {}}};

  const auto methods = std::vector<sen::MethodSpec> {{{"first", "", {}, TransportMode::confirmed},
                                                      sen::UInt64Type::get(),
                                                      Constness::constant,
                                                      sen::PropertyGetter {},
                                                      false,
                                                      false},
                                                     {{"second", "", {}, TransportMode::multicast},
                                                      sen::Float64Type::get(),
                                                      Constness::nonConstant,
                                                      sen::PropertySetter {},
                                                      true,
                                                      true}};

  const auto events = std::vector<sen::EventSpec> {{
    {"first", "", {}, TransportMode::confirmed},
    {"second", "", {}, TransportMode::unicast},
  }};

  const ClassSpec validSpec = {"TestClass", "TestClass", "", properties, methods, events, {}, {}, true, {}, {}};

  return validSpec;
}

}  // namespace

/// @test
/// Check asString method for bus spec
/// @requirements(SEN-363)
TEST(BusSpec, asString)
{
  const BusSpec spec {"mySession", "myBus"};

  EXPECT_EQ(sen::asString(spec), "mySession.myBus");
}

/// @test
/// Check extraction of qualified names in type conditions
/// @requirements(SEN-363)
TEST(TypeCondition, ExtractQualifiedName)
{
  sen::TypeCondition cond;

  // empty variant
  {
    EXPECT_EQ(sen::extractQualifiedTypeName(cond), std::string_view {});
  }

  // string
  {
    cond = std::string("qualifiedNameExample");
    EXPECT_EQ(sen::extractQualifiedTypeName(cond), "qualifiedNameExample");
  }

  // class type
  {
    const auto type = ClassType::make(validClassSpec());
    cond = type;
    EXPECT_EQ(sen::extractQualifiedTypeName(cond), type->getQualifiedName());
  }
}

/// @test
/// Checks comparison between different type conditions natively using the hidden
/// operator implementations located inside the cpp file
/// @requirements(SEN-363)
TEST(TypeCondition, CustomCompareOperators)
{
  // Same strings
  {
    sen::TypeCondition condAlpha {std::string("qualifiedNameExample")};
    sen::TypeCondition condBeta {std::string("qualifiedNameExample")};

    EXPECT_TRUE(sen::operator==(condAlpha, condBeta));
    EXPECT_FALSE(sen::operator!=(condAlpha, condBeta));
  }

  // Same class type
  {
    const auto classAlpha = ClassType::make(validClassSpec());
    const auto classBeta = ClassType::make(validClassSpec());

    sen::TypeCondition condAlpha {classAlpha};
    sen::TypeCondition condBeta {classBeta};

    EXPECT_TRUE(sen::operator==(condAlpha, condBeta));
    EXPECT_FALSE(sen::operator!=(condAlpha, condBeta));
  }

  // Different names
  {
    sen::TypeCondition condAlpha {std::string("nameA")};
    sen::TypeCondition condBeta {std::string("nameB")};

    EXPECT_FALSE(sen::operator==(condAlpha, condBeta));
    EXPECT_TRUE(sen::operator!=(condAlpha, condBeta));
  }
}

/// @test
/// Checks comparison between different type conditions using the standard behavior
/// @requirements(SEN-363)
TEST(TypeCondition, compare)
{
  // same
  {
    sen::TypeCondition lhs {std::in_place_type<std::string>, "qualifiedNameExample"};
    const auto& rhs = lhs;

    EXPECT_EQ(lhs, lhs);
    EXPECT_EQ(lhs, rhs);
  }

  // same 2
  {
    const auto lhsSharedPtr = ClassType::make(validClassSpec());
    const auto rhsSharedPtr = ClassType::make(validClassSpec());
    sen::TypeCondition lhs {std::in_place_type<sen::TypeHandle<const ClassType>>, lhsSharedPtr};
    sen::TypeCondition rhs {std::in_place_type<sen::TypeHandle<const ClassType>>, rhsSharedPtr};

    EXPECT_EQ(std::get<sen::TypeHandle<const ClassType>>(lhs), std::get<sen::TypeHandle<const ClassType>>(lhs));
    EXPECT_EQ(std::get<sen::TypeHandle<const ClassType>>(lhs), std::get<sen::TypeHandle<const ClassType>>(rhs));
  }

  // same (empty)
  {
    sen::TypeCondition lhs;
    sen::TypeCondition rhs;

    EXPECT_EQ(lhs, lhs);
    EXPECT_EQ(lhs, rhs);
  }

  // different (string)
  {
    sen::TypeCondition lhs {std::in_place_type<std::string>, "qualifiedNameExample"};
    sen::TypeCondition rhs {std::in_place_type<std::string>, "qualifiedNameEExample"};

    EXPECT_NE(lhs, rhs);
  }

  // different (empty)
  {
    sen::TypeCondition lhs {std::in_place_type<std::string>, "qualifiedNameExample"};
    sen::TypeCondition rhs;

    EXPECT_NE(lhs, rhs);
  }

  // different (class type)
  {
    const auto lhsSharedPtr = ClassType::make(validClassSpec());
    sen::TypeCondition lhs {std::in_place_type<sen::TypeHandle<const ClassType>>, lhsSharedPtr};

    auto rhsSpec = validClassSpec();
    rhsSpec.qualifiedName = "anotherQualifiedName";

    const auto rhsSharedPtr = ClassType::make(rhsSpec);
    sen::TypeCondition rhs {std::in_place_type<sen::TypeHandle<const ClassType>>, rhsSharedPtr};

    EXPECT_NE(std::get<sen::TypeHandle<const ClassType>>(lhs), std::get<sen::TypeHandle<const ClassType>>(rhs));
  }

  // different type
  {
    sen::TypeCondition lhs = std::string("qualifiedNameExample");
    sen::TypeCondition rhs = ClassType::make(validClassSpec());

    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks correct instance of interest class
/// @requirements(SEN-363)
TEST(Interest, make)
{
  // basic
  {
    auto types = sen::CustomTypeRegistry();

    types.add(sen::UInt8Type::get());
    types.add(sen::Int32Type::get());
    types.add(sen::Float32Type::get());

    EXPECT_NO_THROW(std::ignore = Interest::make("SELECT rpr.Aircraft FROM se.env WHERE 30 > 0", types));
  }

  // empty types
  {
    EXPECT_NO_THROW(std::ignore =
                      Interest::make("SELECT rpr.Aircraft FROM se.env WHERE 30 - 20 >= 10", sen::CustomTypeRegistry()));
  }

  // no condition
  {
    auto types = sen::CustomTypeRegistry();
    types.add(sen::Int32Type::get());

    EXPECT_NO_THROW(std::ignore = Interest::make("SELECT myObject FROM some.bus", types));
  }

  // empty query
  {
    auto types = sen::CustomTypeRegistry();
    types.add(sen::Int32Type::get());

    EXPECT_ANY_THROW(std::ignore = Interest::make("", types));
  }

  // empty
  {
    EXPECT_ANY_THROW(std::ignore = Interest::make("", sen::CustomTypeRegistry()));
  }
}

/// @test
/// Validates that an exception is thrown when a non-class type is used in the query.
/// Ensures type safety during parsing and evaluation.
/// @requirements(SEN-363)
TEST(Interest, make_ThrowsOnNonClassType)
{
  sen::CustomTypeRegistry types;
  types.add(StructType::make(validStructSpec()));

  EXPECT_ANY_THROW({ std::ignore = Interest::make("SELECT ns.TestStruct FROM some.bus", types); });
}

/// @test
/// Validates that an exception is thrown when the query contains a compilation error
/// inside the VM.
/// @requirements(SEN-363)
TEST(Interest, make_ThrowsOnCompileError)
{
  const sen::CustomTypeRegistry types;

  EXPECT_ANY_THROW({ std::ignore = Interest::make("SELECT rpr.Aircraft FROM se.env WHERE 30 > ", types); });
}

/// @test
/// Checks comparison between different interests. Validates the design decision that
/// equality is strictly defined by the query string, ignoring the TypeRegistry
/// @requirements(SEN-363)
TEST(Interest, compare)
{
  // same string, same object
  {
    auto lhs = Interest::make("SELECT * FROM se.env WHERE -10 < 0", sen::CustomTypeRegistry());
    const auto& rhs = lhs;

    EXPECT_EQ(*lhs, *rhs);
    EXPECT_FALSE(*lhs != *rhs);
  }

  // same string, different TypeRegistry
  {
    auto types1 = sen::CustomTypeRegistry();
    types1.add(sen::UInt8Type::get());
    types1.add(sen::Int32Type::get());
    auto lhs = Interest::make("SELECT se.Aircraft FROM se.env WHERE -10 < 0", types1);

    auto types2 = sen::CustomTypeRegistry();
    types2.add(sen::StringType::get());
    auto rhs = Interest::make("SELECT se.Aircraft FROM se.env WHERE -10 < 0", types2);

    EXPECT_EQ(*lhs, *rhs);
    EXPECT_FALSE(*lhs != *rhs);
  }

  // same string, empty TypeRegistry vs populated TypeRegistry -> MUST be equal
  {
    auto types = sen::CustomTypeRegistry();
    types.add(sen::UInt8Type::get());
    types.add(sen::Int32Type::get());

    auto lhs = Interest::make("SELECT se.Aircraft FROM se.env WHERE -10 < 0", types);
    auto rhs = Interest::make("SELECT se.Aircraft FROM se.env WHERE -10 < 0", sen::CustomTypeRegistry());

    EXPECT_EQ(*lhs, *rhs);
  }

  // different query condition
  {
    auto types = sen::CustomTypeRegistry();
    types.add(sen::UInt8Type::get());
    types.add(sen::Int32Type::get());

    auto lhs = Interest::make("SELECT se.Aircraft FROM se.env WHERE -10 < 1", types);
    auto rhs = Interest::make("SELECT se.Aircraft FROM se.env WHERE -10 < 0", types);

    EXPECT_NE(*lhs, *rhs);
    EXPECT_TRUE(*lhs != *rhs);
  }

  // different object selection
  {
    auto lhs = Interest::make("SELECT se.Aircraft FROM se.env WHERE -10 < 1", sen::CustomTypeRegistry());
    auto rhs = Interest::make("SELECT * FROM se.env WHERE -10 < 1", sen::CustomTypeRegistry());

    EXPECT_NE(*lhs, *rhs);
  }

  // different session
  {
    auto lhs = Interest::make("SELECT * FROM se.env WHERE -10 < 1", sen::CustomTypeRegistry());
    auto rhs = Interest::make("SELECT * FROM ses.env WHERE -10 < 1", sen::CustomTypeRegistry());

    EXPECT_NE(*lhs, *rhs);
  }

  // different bus
  {
    auto lhs = Interest::make("SELECT * FROM se.env WHERE -10 < 1", sen::CustomTypeRegistry());
    auto rhs = Interest::make("SELECT * FROM se.env2 WHERE -10 < 1", sen::CustomTypeRegistry());

    EXPECT_NE(*lhs, *rhs);
  }
}

/// @test
/// Checks correct behavior of type registry getter
/// @requirements(SEN-363)
TEST(Interest, getTypeCondition)
{
  sen::CustomTypeRegistry types;

  // empty
  {
    const auto interest = Interest::make("SELECT * FROM se.env WHERE -10 < 1", types);

    EXPECT_EQ(interest->getTypeCondition(), sen::TypeCondition {});
  }

  // basic
  {
    types.add(sen::UInt8Type::get());
    const auto interest = Interest::make("SELECT * FROM se.env WHERE -10 < 1", types);

    EXPECT_EQ(interest->getTypeCondition(), sen::TypeCondition {});
  }

  // basic 2
  {
    types.add(sen::UInt8Type::get());
    types.add(sen::Float64Type::get());
    types.add(sen::Int64Type::get());
    const auto interest = Interest::make("SELECT * FROM se.env WHERE -10 < 1", types);

    EXPECT_EQ(interest->getTypeCondition(), sen::TypeCondition {});
  }
}

/// @test
/// Checks correct behavior of query string getter
/// @requirements(SEN-363)
TEST(Interest, getQueryStr)
{
  sen::CustomTypeRegistry types;

  {
    types.add(sen::Float64Type::get());
    const auto interest = Interest::make("SELECT * FROM se.env WHERE -10 < 1", types);

    EXPECT_EQ(interest->getQueryString(), "SELECT * FROM se.env WHERE -10 < 1");
  }

  {
    types.add(sen::Int64Type::get());
    const auto interest = Interest::make("SELECT rpr.Aircraft FROM se.env", types);

    EXPECT_EQ(interest->getQueryString(), "SELECT rpr.Aircraft FROM se.env");
  }
}

/// @test
/// Checks correct behavior of query code getter
/// @requirements(SEN-363)
TEST(Interest, getQueryCode)
{
  const auto program = "SELECT rpr.Aircraft FROM se.env WHERE 3.14 IN (3.14, 2.68, 0.014)";
  const auto interest = Interest::make(program, sen::CustomTypeRegistry());

  const sen::lang::VM vm;
  const auto statement = vm.parse(program);
  auto compileResult = vm.compile(statement);

  const sen::lang::Chunk queryCode = std::move(compileResult).getValue();
  EXPECT_TRUE(queryCode.isValid());
  EXPECT_EQ(interest->getQueryCode().count(), queryCode.count());
}

/// @test
/// Checks correct behavior of bus condition getter
/// @requirements(SEN-363)
TEST(Interest, getBusCondition)
{
  const auto program = "SELECT rpr.Aircraft FROM se.env WHERE 10 - 20 = -10";
  const auto interest = Interest::make(program, sen::CustomTypeRegistry());

  const auto busSpec = interest->getBusCondition();
  EXPECT_EQ(busSpec->sessionName, "se");
  EXPECT_EQ(busSpec->busName, "env");
}

/// @test
/// Checks correct behavior of id getter
/// @requirements(SEN-363)
TEST(Interest, getID)
{
  {
    const auto program = "SELECT rpr.Aircraft FROM se.env WHERE 10 - 20 = -10";
    const auto interest = Interest::make(program, sen::CustomTypeRegistry());

    EXPECT_EQ(interest->getId().get(), sen::crc32(interest->getQueryString()));
  }

  {
    const auto program = "SELECT * FROM se.env WHERE (50 + 20.50) = 70.50";
    const auto interest = Interest::make(program, sen::CustomTypeRegistry());

    EXPECT_EQ(interest->getId().get(), sen::crc32(interest->getQueryString()));
  }
}

/// @test
/// Validates robust extraction for queries holding no explicit logical conditions
/// @requirements(SEN-363)
TEST(Interest, getVarInfoList_EmptyCode)
{
  const auto type = ClassType::make(validClassSpec());

  const auto interest = Interest::make(R"(SELECT rpr.Aircraft FROM se.env)", sen::CustomTypeRegistry());
  const auto varList = interest->getOrComputeVarInfoList(type.type());

  EXPECT_TRUE(varList.empty());
}

/// @test
/// Checks variant field introspection to ensure paths like VariantProperty.TypeName resolve correctly
/// @requirements(SEN-363)
TEST(Interest, getVarInfoList_Variant)
{
  const auto type = ClassType::make(validClassSpec());

  const auto interest =
    Interest::make(R"(SELECT rpr.Aircraft FROM se.env WHERE direction.f64 > 10.0)", sen::CustomTypeRegistry());
  const auto varList = interest->getOrComputeVarInfoList(type.type());

  EXPECT_FALSE(varList.empty());
  EXPECT_EQ(varList.size(), 1);
  EXPECT_EQ(varList[0].fieldIndexes.size(), 1);
  EXPECT_EQ(varList[0].fieldIndexes[0], 0);
}

/// @test
/// Rejects attempts to traverse missing properties nested inside a variant
/// @requirements(SEN-363)
TEST(Interest, getVarInfoList_MissingVariantField)
{
  const auto type = ClassType::make(validClassSpec());

  const auto interest = Interest::make(R"(SELECT rpr.Aircraft FROM se.env WHERE direction.NonExistentField > 10.0)",
                                       sen::CustomTypeRegistry());

  EXPECT_ANY_THROW({ std::ignore = interest->getOrComputeVarInfoList(type.type()); });
}

/// @test
/// Detects queries attempting to dot-traverse native types holding no inner fields
/// @requirements(SEN-363)
TEST(Interest, getVarInfoList_InvalidPropertyField)
{
  const auto type = ClassType::make(validClassSpec());

  const auto interest =
    Interest::make(R"(SELECT rpr.Aircraft FROM se.env WHERE objectID.something > 10.0)", sen::CustomTypeRegistry());

  EXPECT_ANY_THROW({ std::ignore = interest->getOrComputeVarInfoList(type.type()); });
}

/// @test
/// Checks correct behavior of get or compute var info list under standard conditions
/// @requirements(SEN-363)
TEST(Interest, getVarInfoList)
{
  const auto type = ClassType::make(validClassSpec());

  // empty var list (no variables)
  {
    auto interest =
      Interest::make("SELECT rpr.Aircraft FROM se.env WHERE 3.14 IN (3.14, 2.68, 0.014)", sen::CustomTypeRegistry());

    auto varList = interest->getOrComputeVarInfoList(type.type());
    EXPECT_TRUE(varList.empty());
  }

  // built-in vars
  {
    auto interest = Interest::make(R"(SELECT rpr.Aircraft FROM se.env WHERE name = "helloWorld" AND id >= 8090)",
                                   sen::CustomTypeRegistry());

    auto varList = interest->getOrComputeVarInfoList(type.type());
    EXPECT_FALSE(varList.empty());
    EXPECT_EQ(varList.size(), 2);
  }

  // vars
  {
    auto interest = Interest::make(R"(SELECT rpr.Aircraft FROM se.env WHERE objectID = "aircraft1" OR direction = 34.53
                                                            OR coordinates = "local")",
                                   sen::CustomTypeRegistry());

    auto varList = interest->getOrComputeVarInfoList(type.type());
    EXPECT_FALSE(varList.empty());
    EXPECT_EQ(varList.size(), 3);
  }

  // struct field vars
  {
    auto interest = Interest::make(R"(SELECT rpr.Aircraft FROM se.env WHERE objectID = "aircraft1" OR direction = 34.53
                                                            OR coordinates.x > 25.00 OR coordinates.z < 0.00)",
                                   sen::CustomTypeRegistry());

    auto varList = interest->getOrComputeVarInfoList(type.type());
    EXPECT_FALSE(varList.empty());
    EXPECT_EQ(varList.size(), 4);
  }

  // vars not found
  {
    auto interest = Interest::make(R"(SELECT rpr.Aircraft FROM se.env WHERE objectID = "aircraft1" OR direction = 34.53
                                                            OR coordinates = "local" OR munition = "full")",
                                   sen::CustomTypeRegistry());

    EXPECT_ANY_THROW(std::ignore = interest->getOrComputeVarInfoList(type.type()));
  }

  // bad struct field
  {
    auto interest = Interest::make(R"(SELECT rpr.Aircraft FROM se.env WHERE objectID = "aircraft1" OR direction = 34.53
                                                            OR coordinates.xCoord > 25.00 OR coordinates.yCoord < 0.00)",
                                   sen::CustomTypeRegistry());

    EXPECT_ANY_THROW(std::ignore = interest->getOrComputeVarInfoList(type.type()));
  }
}
