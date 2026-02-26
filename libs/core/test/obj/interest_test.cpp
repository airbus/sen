// === interest_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/hash32.h"
#include "sen/core/lang/vm.h"
#include "sen/core/meta/class_type.h"
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
  BusSpec spec {"mySession", "myBus"};

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
/// Checks comparison between different type conditions
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

    // TODO SEN-473: Check this, why the same?
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
/// Checks comparison between different interests
/// @requirements(SEN-363)
TEST(Interest, compare)
{
  // same
  {
    auto lhs = Interest::make("SELECT * FROM se.env WHERE -10 < 0", sen::CustomTypeRegistry());
    const auto& rhs = lhs;

    EXPECT_EQ(lhs, rhs);
  }

  // different types
  {
    auto types = sen::CustomTypeRegistry();
    types.add(sen::UInt8Type::get());
    types.add(sen::Int32Type::get());

    auto lhs = Interest::make("SELECT se.Aircraft FROM se.env WHERE -10 < 0", types);

    types.add(sen::StringType::get());
    auto rhs = Interest::make("SELECT se.Aircraft FROM se.env WHERE -10 < 0", types);

    EXPECT_NE(lhs, rhs);
  }

  // empty type vs no empty type
  {
    auto types = sen::CustomTypeRegistry();
    types.add(sen::UInt8Type::get());
    types.add(sen::Int32Type::get());

    auto lhs = Interest::make("SELECT se.Aircraft FROM se.env WHERE -10 < 0", types);
    auto rhs = Interest::make("SELECT se.Aircraft FROM se.env WHERE -10 < 0", sen::CustomTypeRegistry());
  }

  // different query condition
  {
    auto types = sen::CustomTypeRegistry();
    types.add(sen::UInt8Type::get());
    types.add(sen::Int32Type::get());

    auto lhs = Interest::make("SELECT se.Aircraft FROM se.env WHERE -10 < 1", types);
    auto rhs = Interest::make("SELECT se.Aircraft FROM se.env WHERE -10 < 0", types);

    EXPECT_NE(lhs, rhs);
  }

  // different object selection
  {
    auto types = sen::CustomTypeRegistry();
    types.add(sen::UInt8Type::get());

    auto lhs = Interest::make("SELECT se.Aircraft FROM se.env WHERE -10 < 1", sen::CustomTypeRegistry());
    auto rhs = Interest::make("SELECT * FROM se.env WHERE -10 < 1", sen::CustomTypeRegistry());

    EXPECT_NE(lhs, rhs);
  }

  // different session
  {
    auto types = sen::CustomTypeRegistry();
    types.add(sen::UInt8Type::get());

    auto lhs = Interest::make("SELECT * FROM se.env WHERE -10 < 1", sen::CustomTypeRegistry());
    auto rhs = Interest::make("SELECT * FROM ses.env WHERE -10 < 1", sen::CustomTypeRegistry());

    EXPECT_NE(lhs, rhs);
  }

  // different bus
  {
    auto lhs = Interest::make("SELECT * FROM se.env WHERE -10 < 1", sen::CustomTypeRegistry());
    auto rhs = Interest::make("SELECT * FROM se.env2 WHERE -10 < 1", sen::CustomTypeRegistry());

    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks correct behavior of type registry getter
/// @requirements(SEN-363)
TEST(Interest, getTypeRegistry)
{
  sen::CustomTypeRegistry types;

  // empty
  {
    auto interest = Interest::make("SELECT * FROM se.env WHERE -10 < 1", types);

    EXPECT_EQ(interest->getTypeCondition(), sen::TypeCondition {});
  }

  // basic
  {
    types.add(sen::UInt8Type::get());
    auto interest = Interest::make("SELECT * FROM se.env WHERE -10 < 1", types);

    EXPECT_EQ(interest->getTypeCondition(), sen::TypeCondition {});
  }

  // basic 2
  {
    types.add(sen::UInt8Type::get());
    types.add(sen::Float64Type::get());
    types.add(sen::Int64Type::get());
    auto interest = Interest::make("SELECT * FROM se.env WHERE -10 < 1", types);

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
    auto interest = Interest::make("SELECT * FROM se.env WHERE -10 < 1", types);
    EXPECT_EQ(interest->getQueryString(), "SELECT * FROM se.env WHERE -10 < 1");
  }

  {
    types.add(sen::Int64Type::get());
    auto interest = Interest::make("SELECT rpr.Aircraft FROM se.env", types);
    EXPECT_EQ(interest->getQueryString(), "SELECT rpr.Aircraft FROM se.env");
  }
}

/// @test
/// Checks correct behavior of query code getter
/// @requirements(SEN-363)
TEST(Interest, getQueryCode)
{
  const auto program = "SELECT rpr.Aircraft FROM se.env WHERE 3.14 IN (3.14, 2.68, 0.014)";
  auto interest = Interest::make(program, sen::CustomTypeRegistry());

  sen::lang::VM vm;
  auto statement = vm.parse(program);
  auto compileResult = vm.compile(statement);

  sen::lang::Chunk queryCode = std::move(compileResult).getValue();
  EXPECT_TRUE(queryCode.isValid());
  EXPECT_EQ(interest->getQueryCode().count(), queryCode.count());
}

/// @test
/// Checks correct behavior of bus condition getter
/// @requirements(SEN-363)
TEST(Interest, getBusCondition)
{
  const auto program = "SELECT rpr.Aircraft FROM se.env WHERE 10 - 20 = -10";
  auto interest = Interest::make(program, sen::CustomTypeRegistry());

  auto busSpec = interest->getBusCondition();
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
    auto interest = Interest::make(program, sen::CustomTypeRegistry());

    EXPECT_EQ(interest->getId().get(), sen::crc32(interest->getQueryString()));
  }

  {
    const auto program = "SELECT * FROM se.env WHERE (50 + 20.50) = 70.50";
    auto interest = Interest::make(program, sen::CustomTypeRegistry());

    EXPECT_EQ(interest->getId().get(), sen::crc32(interest->getQueryString()));
  }
}

/// @test
/// Checks correct behavior of get or compute var info list
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
