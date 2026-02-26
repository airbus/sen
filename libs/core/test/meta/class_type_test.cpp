// === class_type_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/hash32.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"

// google test
#include <gtest/gtest.h>

// std
#include <exception>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

using sen::ClassSpec;
using sen::ClassType;
using sen::Constness;
using sen::PropertyCategory;
using sen::TransportMode;

namespace
{

const auto& properties()
{
  static const auto properties = std::vector<sen::PropertySpec> {
    {"property1", "", sen::StringType::get(), PropertyCategory::staticRO, TransportMode::confirmed, false, {}},
    {"property2", "", sen::TimestampType::get(), PropertyCategory::dynamicRW, TransportMode::unicast, true, {}},
    {"property3", "", sen::UInt32Type::get(), PropertyCategory::staticRW, TransportMode::multicast, true, {}}};
  return properties;
}

const auto& property1ExpectedId()
{
  static const auto property1ExpectedId = sen::hashCombine(sen::propertyHashSeed, std::string("property1"));

  return property1ExpectedId;
}

const auto& property2ExpectedId()
{
  static const auto property2ExpectedId = sen::hashCombine(sen::propertyHashSeed, std::string("property2"));
  return property2ExpectedId;
}

const auto& property3ExpectedId()
{
  static const auto property3ExpectedId = sen::hashCombine(sen::propertyHashSeed, std::string("property3"));
  return property3ExpectedId;
}

const auto& methods()
{
  static const auto methods = std::vector<sen::MethodSpec> {{{"first", "", {}, TransportMode::confirmed},
                                                             sen::TimestampType::get(),
                                                             Constness::constant,
                                                             sen::PropertyGetter {},
                                                             false,
                                                             false},
                                                            {{"second", "", {}, TransportMode::multicast},
                                                             sen::DurationType::get(),
                                                             Constness::nonConstant,
                                                             sen::PropertySetter {},
                                                             true,
                                                             true}};
  return methods;
}

const u32& firstMethodExpectedId()
{
  static const auto firstMethodExpectedId = sen::hashCombine(sen::methodHashSeed, std::string("first"));
  return firstMethodExpectedId;
}

const u32& secondMethodExpectedId()
{
  static const auto secondMethodExpectedId = sen::hashCombine(sen::methodHashSeed, std::string("second"));
  return secondMethodExpectedId;
}

std::vector<sen::EventSpec> events()
{
  auto events = std::vector<sen::EventSpec> {{
    {"first", "", {}, TransportMode::confirmed},
    {"second", "", {}, TransportMode::unicast},
  }};
  return events;
}

const u32& firstEventExpectedId()
{
  static const auto firstEventExpectedId = sen::hashCombine(sen::eventHashSeed, std::string("first"));
  return firstEventExpectedId;
}

const u32& secondEventExpectedId()
{
  static const auto secondEventExpectedId = sen::hashCombine(sen::eventHashSeed, std::string("second"));
  return secondEventExpectedId;
}

const ClassSpec& validSpec()
{
  static const ClassSpec validSpec = {
    "TestClass", "TestClass", "", properties(), methods(), events(), {}, {}, true, {}, {}};
  return validSpec;
}

void checkSpecData(const ClassType& type, const ClassSpec& spec)  // NOLINT(readability-function-size)
{
  EXPECT_EQ(type.getName(), spec.name);
  EXPECT_EQ(type.getQualifiedName(), spec.qualifiedName);
  EXPECT_EQ(type.getDescription(), spec.description);
  EXPECT_EQ(type.isInterface(), spec.isInterface);

  if (spec.constructor.has_value())
  {
    EXPECT_EQ(type.getConstructor(), sen::Method::make(*spec.constructor).get());
  }

  auto mode = ClassType::SearchMode::includeParents;
  if (spec.parents.empty())
  {
    mode = ClassType::SearchMode::doNotIncludeParents;
  }

  for (const auto& prop: spec.properties)
  {
    EXPECT_EQ(*type.searchPropertyByName(prop.name, mode), *sen::Property::make(prop).get());
  }

  for (const auto& meth: spec.methods)
  {
    EXPECT_EQ(*type.searchMethodByName(meth.callableSpec.name, mode), *sen::Method::make(meth).get());
  }

  for (const auto& event: spec.events)
  {
    EXPECT_EQ(*type.searchEventByName(event.callableSpec.name, mode), *sen::Event::make(event).get());
  }
}

void checkInvalidSpec(const ClassSpec& spec) { EXPECT_THROW(std::ignore = ClassType::make(spec), std::exception); }

}  // namespace

/// @test
/// Checks class spec comparison
/// @requirements(SEN-355)
TEST(ClassType, specComparison)
{
  const auto& lhs = validSpec();

  // same
  {
    const auto& rhs = validSpec();

    EXPECT_EQ(lhs, lhs);
    EXPECT_EQ(lhs, rhs);
  }

  // different name
  {
    auto rhs = validSpec();
    rhs.name = lhs.name + 'X';

    EXPECT_NE(lhs, rhs);
  }

  // different qualified name
  {
    auto rhs = validSpec();
    rhs.qualifiedName = lhs.qualifiedName + 'X';

    EXPECT_NE(lhs, rhs);
  }

  // different description
  {
    auto rhs = validSpec();
    rhs.description = lhs.description + 'X';

    EXPECT_NE(lhs, rhs);
  }

  // different is interface
  {
    auto rhs = validSpec();
    rhs.isInterface = !lhs.isInterface;

    EXPECT_NE(lhs, rhs);
  }

  // different parents
  {
    auto rhs = validSpec();
    rhs.parents = {};

    EXPECT_EQ(lhs, rhs);
  }

  // different constructor
  {
    auto rhs = validSpec();
    rhs.constructor = std::optional<sen::MethodSpec> {
      {{"constructor", "", {{"arg1", "", sen::DurationType::get()}}, TransportMode::confirmed},
       sen::UInt64Type::get(),
       Constness::constant,
       sen::NonPropertyRelated {},
       false,
       true}};

    EXPECT_NE(lhs, rhs);
  }

  // different methods
  {
    auto rhs = validSpec();
    rhs.methods.pop_back();

    EXPECT_NE(lhs, rhs);
  }

  // different events()
  {
    auto rhs = validSpec();
    rhs.events.pop_back();

    EXPECT_NE(lhs, rhs);
  }

  // different properties
  {
    auto rhs = validSpec();
    rhs.properties.pop_back();

    EXPECT_NE(lhs, rhs);
  }

  // different remote proxy
  {
    auto rhs = validSpec();
    rhs.remoteProxyMaker = {};

    EXPECT_EQ(lhs, rhs);
  }

  // different local proxy
  {
    auto rhs = validSpec();
    rhs.localProxyMaker = {};

    EXPECT_EQ(lhs, rhs);
  }

  // no name
  {
    auto rhs = validSpec();
    rhs.name = {};

    EXPECT_NE(lhs, rhs);
  }

  // no qualified name
  {
    auto rhs = validSpec();
    rhs.qualifiedName = {};

    EXPECT_NE(lhs, rhs);
  }

  // no parents
  {
    auto rhs = validSpec();
    ClassSpec parentSpec {"TestClass", "TestClass", "", {}, {}, {}, std::nullopt, {}, true, {}, {}};
    rhs.parents = {ClassType::make(parentSpec)};

    EXPECT_NE(lhs, rhs);
  }

  // empty
  {
    auto rhs = validSpec();
    rhs = {};

    EXPECT_NE(lhs, rhs);
  }

  // both empty
  {
    ClassSpec llhs = {};
    ClassSpec rhs = {};

    EXPECT_EQ(llhs, rhs);
  }
}

/// @test
/// Checks basics class type methods
/// @requirements(SEN-355)
TEST(ClassType, basicsBoolConversion)
{
  auto type = ClassType::make(validSpec());

  // things that structs are
  EXPECT_TRUE(type->isCustomType());
  EXPECT_TRUE(type->isClassType());
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
  EXPECT_FALSE(type->isStructType());
  EXPECT_FALSE(type->isVariantType());
  EXPECT_FALSE(type->isQuantityType());
  EXPECT_FALSE(type->isAliasType());

  // is interface (in this particular case)
  EXPECT_EQ(type->isInterface(), true);
  // does not inherit from empty parent
  EXPECT_EQ(type->isSameOrInheritsFrom({}), false);
  // does not inherit from no-existing parent
  EXPECT_EQ(type->isSameOrInheritsFrom("badParentClass"), false);
  // is same
  EXPECT_EQ(type->isSameOrInheritsFrom("TestClass"), true);
}

/// @test
/// Checks basics class type methods
/// @requirements(SEN-355)
TEST(ClassType, basicsConversion)
{
  auto type = ClassType::make(validSpec());

  // things that structs are
  EXPECT_NE(type->asCustomType(), nullptr);
  EXPECT_NE(type->asClassType(), nullptr);
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
  EXPECT_EQ(type->asStructType(), nullptr);
  EXPECT_EQ(type->asVariantType(), nullptr);
  EXPECT_EQ(type->asQuantityType(), nullptr);
  EXPECT_EQ(type->asAliasType(), nullptr);
}

/// @test
/// Checks correct class instance creation
/// @requirements(SEN-355)
TEST(ClassType, makeValid)
{
  // basic
  {
    const auto& spec = validSpec();
    auto type = ClassType::make(spec);
    checkSpecData(*type, spec);
  }

  // inherits from parent class
  {
    const ClassSpec parentSpec {
      "TestClass", "TestClass", "", properties(), methods(), events(), std::nullopt, {}, false, {}, {}};
    const auto spec = ClassSpec {
      "TestClassChild", "TestClassChild", "", {}, {}, {}, std::nullopt, {ClassType::make(parentSpec)}, false, {}, {}};

    const auto type = ClassType::make(spec);
    checkSpecData(*type, spec);
  }

  // multiple inheritance
  {
    const ClassSpec grandpaSpec {
      "TestClassGrandpa", "TestClassGrandpa", "", {}, {}, events(), std::nullopt, {}, false, {}, {}};
    const ClassSpec parentSpec {"TestClassParent",
                                "TestClassParent",
                                "",
                                {},
                                methods(),
                                {},
                                std::nullopt,
                                {ClassType::make(grandpaSpec)},
                                false,
                                {},
                                {}};
    const auto spec = ClassSpec {"TestClassChild",
                                 "TestClassChild",
                                 "",
                                 properties(),
                                 {},
                                 {},
                                 std::nullopt,
                                 {ClassType::make(parentSpec)},
                                 false,
                                 {},
                                 {}};

    const auto type = ClassType::make(spec);
    checkSpecData(*type, spec);
  }
}

/// @test
/// Checks incorrect class instance creation
/// @requirements(SEN-355)
TEST(ClassType, makeInvalid)
{
  // spec with no name
  {
    ClassSpec invalid = validSpec();
    invalid.name = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid name
  {
    ClassSpec invalid = validSpec();
    invalid.name = "someStruct";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 2
  {
    ClassSpec invalid = validSpec();
    invalid.name = "·&·&$/";
    checkInvalidSpec(invalid);
  }

  // spec with no qualified name
  {
    ClassSpec invalid = validSpec();
    invalid.qualifiedName = {};
    checkInvalidSpec(invalid);
  }

  // spec of interface class with parent
  {
    const ClassSpec parentSpec {"TestClass", "ns.TestClass", "", {}, {}, {}, std::nullopt, {}, false, {}, {}};
    const auto invalid = ClassSpec {"TestClassChild",
                                    "ns.TestClassChild",
                                    "",
                                    properties(),
                                    methods(),
                                    events(),
                                    std::nullopt,
                                    {ClassType::make(parentSpec)},
                                    true,
                                    {},
                                    {}};

    checkInvalidSpec(invalid);
  }

  // repeated properties() method id in child
  {
    const ClassSpec parentSpec {"TestClass", "ns.TestClass", "", properties(), {}, {}, std::nullopt, {}, false, {}, {}};
    const auto invalid = ClassSpec {
      "TestClassChild",
      "ns.TestClassChild",
      "",
      {{"property19", "", sen::StringType::get(), PropertyCategory::staticRO, TransportMode::confirmed, false, {}}},
      methods(),
      events(),
      std::nullopt,
      {ClassType::make(parentSpec)},
      true,
      {},
      {}};

    checkInvalidSpec(invalid);
  }

  // repeated properties() id in child
  {
    const ClassSpec parentSpec {"TestClass", "ns.TestClass", "", {}, {}, {}, std::nullopt, {}, false, {}, {}};
    const auto invalid = ClassSpec {
      "TestClassChild",
      "ns.TestClassChild",
      "",
      {{"property19", "", sen::StringType::get(), PropertyCategory::staticRO, TransportMode::confirmed, false, {}},
       {"property20", "", sen::StringType::get(), PropertyCategory::dynamicRO, TransportMode::multicast, false, {}}},
      methods(),
      events(),
      std::nullopt,
      {ClassType::make(parentSpec)},
      true,
      {},
      {}};

    checkInvalidSpec(invalid);
  }

  // parent properties() in child
  {
    const ClassSpec parentSpec {"TestClass", "ns.TestClass", "", properties(), {}, {}, std::nullopt, {}, false, {}, {}};
    const auto invalid = ClassSpec {"TestClassChild",
                                    "ns.TestClassChild",
                                    "",
                                    properties(),
                                    methods(),
                                    events(),
                                    std::nullopt,
                                    {ClassType::make(parentSpec)},
                                    true,
                                    {},
                                    {}};

    checkInvalidSpec(invalid);
  }

  // repeated parent method id in child
  {
    const ClassSpec parentSpec {"TestClass", "ns.TestClass", "", {}, methods(), {}, std::nullopt, {}, false, {}, {}};
    const auto invalid = ClassSpec {"TestClassChild",
                                    "ns.TestClassChild",
                                    "",
                                    properties(),
                                    {{{"zero", "", {}, TransportMode::multicast},
                                      sen::TimestampType::get(),
                                      Constness::constant,
                                      sen::PropertyGetter {},
                                      false,
                                      false}},
                                    events(),
                                    std::nullopt,
                                    {ClassType::make(parentSpec)},
                                    true,
                                    {},
                                    {}};

    checkInvalidSpec(invalid);
  }

  // repeated method id in child
  {
    const ClassSpec parentSpec {"TestClass", "ns.TestClass", "", {}, {}, {}, std::nullopt, {}, false, {}, {}};
    const auto invalid = ClassSpec {"TestClassChild",
                                    "ns.TestClassChild",
                                    "",
                                    properties(),
                                    {{{"zero", "", {}, TransportMode::multicast},
                                      sen::TimestampType::get(),
                                      Constness::constant,
                                      sen::PropertyGetter {},
                                      false,
                                      false},
                                     {{"first", "", {}, TransportMode::multicast},
                                      sen::TimestampType::get(),
                                      Constness::constant,
                                      sen::PropertyGetter {},
                                      false,
                                      false}},
                                    events(),
                                    std::nullopt,
                                    {ClassType::make(parentSpec)},
                                    true};

    checkInvalidSpec(invalid);
  }

  // parent methods in child
  {
    const ClassSpec parentSpec {"TestClass", "ns.TestClass", "", {}, methods(), {}, std::nullopt, {}, false, {}, {}};
    const auto invalid = ClassSpec {"TestClassChild",
                                    "ns.TestClassChild",
                                    "",
                                    properties(),
                                    methods(),
                                    events(),
                                    std::nullopt,
                                    {ClassType::make(parentSpec)},
                                    true,
                                    {},
                                    {}};

    checkInvalidSpec(invalid);
  }

  // repeated parent event id in child
  {
    const ClassSpec parentSpec {"TestClass", "ns.TestClass", "", {}, {}, events(), std::nullopt, {}, false, {}, {}};
    const auto invalid = ClassSpec {"TestClassChild",
                                    "ns.TestClassChild",
                                    "",
                                    properties(),
                                    methods(),
                                    {{"zero", "", {}, TransportMode::confirmed}},
                                    std::nullopt,
                                    {ClassType::make(parentSpec)},
                                    true,
                                    {},
                                    {}};

    checkInvalidSpec(invalid);
  }

  // repeated event id in child
  {
    const ClassSpec parentSpec {"TestClass", "ns.TestClass", "", {}, {}, {}, std::nullopt, {}, false, {}, {}};
    const auto invalid =
      ClassSpec {"TestClassChild",
                 "ns.TestClassChild",
                 "",
                 properties(),
                 methods(),
                 {{"first", "", {}, TransportMode::confirmed}, {"second", "", {}, TransportMode::confirmed}},
                 std::nullopt,
                 {ClassType::make(parentSpec)},
                 true,
                 {},
                 {}};

    checkInvalidSpec(invalid);
  }

  // parent events() in child
  {
    const ClassSpec parentSpec {"TestClass", "ns.TestClass", "", {}, {}, events(), std::nullopt, {}, false, {}, {}};
    const auto invalid = ClassSpec {"TestClassChild",
                                    "ns.TestClassChild",
                                    "",
                                    properties(),
                                    methods(),
                                    events(),
                                    std::nullopt,
                                    {ClassType::make(parentSpec)},
                                    true,
                                    {},
                                    {}};

    checkInvalidSpec(invalid);
  }

  // spec with invalid constructor (return type)
  {
    ClassSpec invalid = validSpec();
    invalid.constructor = {{{"$&/()", "", {}, TransportMode::unicast},
                            sen::UInt8Type::get(),
                            Constness::nonConstant,
                            sen::NonPropertyRelated {},
                            true}};
    checkInvalidSpec(invalid);
  }
}

/// @test
/// Checks class instance comparison
/// @requirements(SEN-355)
TEST(ClassType, comparison)
{
  const auto& lhs = validSpec();

  // different types
  {
    auto type = ClassType::make(lhs);

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
    EXPECT_NE(type, sen::TimestampType::get());
    EXPECT_NE(type, sen::DurationType::get());
  }

  // same
  {
    const auto& rhs = validSpec();
    auto type1 = ClassType::make(lhs);
    auto type2 = ClassType::make(rhs);

    EXPECT_EQ(*type1, *type1);
    EXPECT_EQ(*type1, *type2);
  }

  // different name
  {
    auto rhs = validSpec();
    rhs.name = lhs.name + 'X';

    EXPECT_NE(ClassType::make(lhs), ClassType::make(rhs));
  }

  // different qualified name
  {
    auto rhs = validSpec();
    rhs.qualifiedName = lhs.qualifiedName + 'X';

    EXPECT_NE(ClassType::make(lhs), ClassType::make(rhs));
  }

  // different description
  {
    auto rhs = validSpec();
    rhs.description = lhs.description + 'X';

    EXPECT_NE(ClassType::make(lhs), ClassType::make(rhs));
  }

  // different is interface
  {
    auto rhs = validSpec();
    rhs.isInterface = !lhs.isInterface;

    EXPECT_NE(ClassType::make(lhs), ClassType::make(rhs));
  }

  // different parents
  {
    auto rhs = validSpec();
    rhs.parents = {};

    EXPECT_EQ(ClassType::make(lhs), ClassType::make(rhs));
  }

  // different constructor
  {
    auto rhs = validSpec();
    rhs.constructor = std::optional<sen::MethodSpec> {
      {{"constructor", "", {{"arg1", "", sen::DurationType::get()}}, TransportMode::confirmed},
       sen::VoidType::get(),
       Constness::constant,
       sen::NonPropertyRelated {},
       false,
       true}};

    EXPECT_NE(ClassType::make(lhs), ClassType::make(rhs));
  }

  // different methods
  {
    auto rhs = validSpec();
    rhs.methods.pop_back();

    EXPECT_NE(ClassType::make(lhs), ClassType::make(rhs));
  }

  // different events()
  {
    auto rhs = validSpec();
    rhs.events.pop_back();

    EXPECT_NE(ClassType::make(lhs), ClassType::make(rhs));
  }

  // different properties()
  {
    auto rhs = validSpec();
    rhs.properties.pop_back();

    EXPECT_NE(ClassType::make(lhs), ClassType::make(rhs));
  }

  // different remote proxy
  {
    auto rhs = validSpec();
    rhs.remoteProxyMaker = {};

    EXPECT_EQ(ClassType::make(lhs), ClassType::make(rhs));
  }

  // different local proxy
  {
    auto rhs = validSpec();
    rhs.localProxyMaker = {};

    EXPECT_EQ(ClassType::make(lhs), ClassType::make(rhs));
  }

  // no parents
  {
    auto rhs = validSpec();
    rhs.isInterface = false;
    ClassSpec parentSpec {"TestClass", "TestClass", "", {}, {}, {}, std::nullopt, {}, true, {}, {}};
    rhs.parents = {ClassType::make(parentSpec)};

    EXPECT_NE(ClassType::make(lhs), ClassType::make(rhs));
  }
}

/// @test
/// Checks correctness of search properties(), methods and events() by ID
/// @requirements(SEN-355)
TEST(ClassType, searchByID)
{
  auto spec = validSpec();

  // property search
  {
    auto type = ClassType::make(spec);

    // the following properties() with this ids should be found
    EXPECT_EQ(*type->searchPropertyById(property1ExpectedId(), ClassType::SearchMode::doNotIncludeParents),
              *sen::Property::make(properties()[0]).get());
    EXPECT_EQ(*type->searchPropertyById(property2ExpectedId(), ClassType::SearchMode::doNotIncludeParents),
              *sen::Property::make(properties()[1]).get());
    EXPECT_EQ(*type->searchPropertyById(property3ExpectedId(), ClassType::SearchMode::doNotIncludeParents),
              *sen::Property::make(properties()[2]).get());

    // the following properties() with this ids should not be found
    EXPECT_EQ(type->searchPropertyById(99, ClassType::SearchMode::doNotIncludeParents), nullptr);
    EXPECT_EQ(type->searchPropertyById(0, ClassType::SearchMode::doNotIncludeParents), nullptr);
    EXPECT_EQ(type->searchPropertyById(16, ClassType::SearchMode::doNotIncludeParents), nullptr);
    EXPECT_EQ(type->searchPropertyById(26, ClassType::SearchMode::doNotIncludeParents), nullptr);
  }

  // method search
  {
    auto type = ClassType::make(spec);

    // search method the following methods with this ids should be found
    EXPECT_EQ(*type->searchMethodById(firstMethodExpectedId(), ClassType::SearchMode::doNotIncludeParents),
              *sen::Method::make(methods()[0]).get());
    EXPECT_EQ(*type->searchMethodById(secondMethodExpectedId(), ClassType::SearchMode::doNotIncludeParents),
              *sen::Method::make(methods()[1]).get());

    // the following methods with this ids should not be found
    EXPECT_EQ(type->searchMethodById(2, ClassType::SearchMode::doNotIncludeParents), nullptr);
    EXPECT_EQ(type->searchMethodById(60, ClassType::SearchMode::doNotIncludeParents), nullptr);
    EXPECT_EQ(type->searchMethodById(76, ClassType::SearchMode::doNotIncludeParents), nullptr);
  }

  // event search
  {
    auto type = ClassType::make(spec);

    // the following events() with this ids should be found
    EXPECT_EQ(*type->searchEventById(firstEventExpectedId(), ClassType::SearchMode::doNotIncludeParents),
              *sen::Event::make(events()[0]).get());
    EXPECT_EQ(*type->searchEventById(secondEventExpectedId(), ClassType::SearchMode::doNotIncludeParents),
              *sen::Event::make(events()[1]).get());

    // the following events() with this ids should not be found
    EXPECT_EQ(type->searchEventById(0, ClassType::SearchMode::doNotIncludeParents), nullptr);
    EXPECT_EQ(type->searchEventById(3, ClassType::SearchMode::doNotIncludeParents), nullptr);
    EXPECT_EQ(type->searchEventById(4, ClassType::SearchMode::doNotIncludeParents), nullptr);
  }

  // search parents ids
  {
    ClassSpec parentSpec {"TestClass", "ns.TestClass", "", {}, {}, {}, std::nullopt, {}, false, {}, {}};
    sen::PropertySpec parentProperty = {
      "propertyParent", "", sen::StringType::get(), PropertyCategory::staticRW, TransportMode::confirmed, false, {}};
    const auto expectedParentPropertyId = sen::hashCombine(sen::propertyHashSeed, std::string("propertyParent"));
    parentSpec.properties.push_back(parentProperty);

    sen::MethodSpec parentMethod = {{"zero", "", {}, TransportMode::multicast},
                                    sen::DurationType::get(),
                                    Constness::nonConstant,
                                    sen::PropertySetter {},
                                    false,
                                    true};
    const auto expectedParentMethodId = sen::hashCombine(sen::methodHashSeed, std::string("zero"));

    parentSpec.methods.push_back(parentMethod);

    sen::EventSpec parentEvent = {"zero", "desc", {}, TransportMode::confirmed};
    const auto expectedParentEventId = sen::hashCombine(sen::eventHashSeed, std::string("zero"));
    parentSpec.events.push_back(parentEvent);

    spec.parents = {ClassType::make(parentSpec)};
    spec.isInterface = false;
    auto type = ClassType::make(spec);

    // ensure we can find parent property, method and event by id using the search mode with include parents
    EXPECT_EQ(*type->searchPropertyById(expectedParentPropertyId, ClassType::SearchMode::includeParents),
              *sen::Property::make(parentProperty).get());
    EXPECT_EQ(*type->searchMethodById(expectedParentMethodId, ClassType::SearchMode::includeParents),
              *sen::Method::make(parentMethod).get());
    EXPECT_EQ(*type->searchEventById(expectedParentEventId, ClassType::SearchMode::includeParents),
              *sen::Event::make(parentEvent).get());

    // ensure we can not find parent property, method and event by id using the search mode that does not include
    // parents
    EXPECT_EQ(type->searchPropertyById(43, ClassType::SearchMode::doNotIncludeParents), nullptr);
    EXPECT_EQ(type->searchMethodById(15, ClassType::SearchMode::doNotIncludeParents), nullptr);
    EXPECT_EQ(type->searchEventById(0, ClassType::SearchMode::doNotIncludeParents), nullptr);
  }
}

/// @test
/// Checks correctness of compute methods
/// @requirements(SEN-355)
TEST(ClassType, compute)
{
  const auto type = sen::ClassType::make(validSpec());

  // compute method hash
  {
    EXPECT_NO_THROW(std::ignore = type->computeMethodHash(1, "myMethod"));
  }

  // compute event hash
  {
    EXPECT_NO_THROW(std::ignore = type->computeEventHash(2, "myEvent"));
  }

  // compute getter
  {
    const std::string getterExpectedPrefix = "senGetType";
    EXPECT_EQ(type->computeTypeGetterFuncName("methodName").substr(0, getterExpectedPrefix.length()),
              getterExpectedPrefix);
  }

  // compute instance maker func
  {
    const std::string makerExpectedPrefix = "senMakeInstance";
    EXPECT_EQ(type->computeInstanceMakerFuncName("exampleText").substr(0, makerExpectedPrefix.length()),
              makerExpectedPrefix);
  }
}
