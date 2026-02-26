// === property_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"
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
#include <tuple>

using sen::Constness;
using sen::EventSpec;
using sen::MethodSpec;
using sen::Property;
using sen::PropertyCategory;
using sen::PropertySpec;

namespace
{

enum PropertyType : u8
{
  property,
  getter,
  setter,
  event
};

const PropertySpec& getValidSpec()
{
  static MethodSpec getterSpec {{"getProp", "getter for prop", {}},
                                sen::StringType::get(),
                                Constness::constant,
                                sen::PropertyGetter {},
                                false,
                                true};

  static MethodSpec setterSpec {{"setProp", "setter for prop", {{"prop", "", sen::StringType::get()}}},
                                sen::VoidType::get(),
                                Constness::nonConstant,
                                sen::PropertySetter {},
                                false,
                                true};

  static EventSpec eventSpec {{"propChanged", "change event for prop", {}}};

  static PropertySpec spec {"prop",
                            "a property",
                            sen::StringType::get(),
                            PropertyCategory::dynamicRW,
                            sen::TransportMode::multicast,
                            false,
                            {}};

  return spec;
}

}  // namespace

/// @test
/// Checks property spec comparison
/// @requirements(SEN-355)
TEST(Property, specComparison)
{
  // same
  {
    const auto& lhs = getValidSpec();
    const auto& rhs = getValidSpec();

    EXPECT_EQ(lhs, lhs);
    EXPECT_EQ(lhs, rhs);
  }

  // different transport
  {
    const auto& lhs = getValidSpec();
    auto rhs = getValidSpec();

    rhs.transportMode = sen::TransportMode::unicast;
    EXPECT_NE(lhs, rhs);
  }

  // different name
  {
    const auto& lhs = getValidSpec();
    auto rhs = getValidSpec();

    rhs.name = lhs.name + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different description
  {
    const auto& lhs = getValidSpec();
    auto rhs = getValidSpec();

    rhs.description = lhs.description + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different category
  {
    const auto& lhs = getValidSpec();
    auto rhs = getValidSpec();

    rhs.category = PropertyCategory::staticRO;
    EXPECT_NE(lhs, rhs);
  }

  // different type
  {
    const auto& lhs = getValidSpec();
    auto rhs = getValidSpec();

    rhs.type = sen::UInt64Type::get();
    EXPECT_NE(lhs, rhs);
  }

  // different checkedSet value
  {
    const auto& lhs = getValidSpec();
    auto rhs = getValidSpec();

    rhs.checkedSet = true;
    EXPECT_NE(lhs, rhs);
  }

  // no name
  {
    const auto& lhs = getValidSpec();
    auto rhs = getValidSpec();

    rhs.name = {};
    EXPECT_NE(lhs, rhs);
  }

  // no name 2
  {
    const auto& lhs = getValidSpec();
    auto rhs = getValidSpec();

    rhs.name = "";
    EXPECT_NE(lhs, rhs);
  }

  // no category
  {
    const auto& lhs = getValidSpec();
    auto rhs = getValidSpec();

    rhs.category = {};
    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks property getters
/// @requirements(SEN-355)
TEST(Property, specCheck)
{
  const auto& spec = getValidSpec();
  const auto instance = Property::make(spec);

  EXPECT_EQ(spec.category, instance->getCategory());
  EXPECT_EQ(spec.description, instance->getDescription());
  EXPECT_EQ(spec.name, instance->getName());
  EXPECT_EQ(spec.type, instance->getType());
  EXPECT_EQ(spec.tags, instance->getTags());
  EXPECT_EQ(spec.checkedSet, instance->getCheckedSet());
  EXPECT_EQ(spec.transportMode, instance->getTransportMode());
}

/// @test
/// Checks creation of properties
/// @requirements(SEN-355)
TEST(Property, make)
{
  // valid
  {
    const auto& spec = getValidSpec();
    EXPECT_NO_THROW(std::ignore = Property::make(spec));
  }

  // no name
  {
    auto spec = getValidSpec();
    spec.name = {};
    EXPECT_THROW(std::ignore = Property::make(spec), std::exception);
  }

  // invalid name
  {
    auto spec = getValidSpec();
    spec.name = "SomeInvalidName";
    EXPECT_THROW(std::ignore = Property::make(spec), std::exception);
  }

  // invalid name 2
  {
    auto spec = getValidSpec();
    spec.name = "$&%/()";
    EXPECT_THROW(std::ignore = Property::make(spec), std::exception);
  }

  // valid undefined category
  {
    auto spec = getValidSpec();
    spec.category = static_cast<PropertyCategory>(-232);
    EXPECT_NE(spec.category, PropertyCategory::staticRO);
    EXPECT_NE(spec.category, PropertyCategory::dynamicRO);
    EXPECT_NE(spec.category, PropertyCategory::staticRW);
    EXPECT_NE(spec.category, PropertyCategory::dynamicRW);
    EXPECT_NO_THROW(std::ignore = Property::make(spec));
  }

  // valid undefined category
  {
    auto spec = getValidSpec();
    spec.transportMode = static_cast<sen::TransportMode>(-98);
    EXPECT_NE(spec.transportMode, sen::TransportMode::confirmed);
    EXPECT_NE(spec.transportMode, sen::TransportMode::multicast);
    EXPECT_NE(spec.transportMode, sen::TransportMode::unicast);
    EXPECT_NO_THROW(std::ignore = Property::make(spec));
  }

  // no tags
  {
    auto spec = getValidSpec();
    spec.tags = {};
    EXPECT_NO_THROW(std::ignore = Property::make(spec));
  }
}

/// @test
/// Checks property instance comparison
/// @requirements(SEN-355)
TEST(Property, comparison)
{
  // same content
  {
    const auto& spec = getValidSpec();
    auto type1 = Property::make(spec);
    auto type2 = Property::make(spec);
    EXPECT_EQ(*type1, *type2);
  }

  // same instance
  {
    const auto& spec = getValidSpec();
    auto type1 = Property::make(spec);
    EXPECT_EQ(*type1, *type1);
  }

  // different name
  {
    const auto& lhs = getValidSpec();
    PropertySpec rhs = getValidSpec();
    rhs.name = getValidSpec().name + "X";

    EXPECT_NE(*Property::make(lhs), *Property::make(rhs));
  }

  // different description
  {
    const auto& lhs = getValidSpec();
    PropertySpec rhs = getValidSpec();
    rhs.description = getValidSpec().description + "X";
    EXPECT_NE(*Property::make(lhs), *Property::make(rhs));
  }

  // different description 2
  {
    const auto& lhs = getValidSpec();
    PropertySpec rhs = getValidSpec();
    rhs.description = "";
    EXPECT_NE(*Property::make(lhs), *Property::make(rhs));
  }

  // different type
  {
    const auto& lhs = getValidSpec();
    PropertySpec rhs = getValidSpec();
    rhs.type = sen::UInt32Type::get();
    EXPECT_NE(*Property::make(lhs), *Property::make(rhs));
  }

  // different transport mode
  {
    const auto& lhs = getValidSpec();
    PropertySpec rhs = getValidSpec();
    rhs.transportMode = sen::TransportMode::unicast;

    EXPECT_NE(*Property::make(lhs), *Property::make(rhs));
  }

  // different checked set
  {
    const auto& lhs = getValidSpec();
    PropertySpec rhs = getValidSpec();
    rhs.checkedSet = !lhs.checkedSet;

    EXPECT_NE(*Property::make(lhs), *Property::make(rhs));
  }

  // different tags
  {
    const auto& lhs = getValidSpec();
    PropertySpec rhs = getValidSpec();
    rhs.tags = {"v1.0.0", "v2.0.0"};

    EXPECT_NE(*Property::make(lhs), *Property::make(rhs));
  }

  // different tags 2
  {
    const auto& lhs = getValidSpec();
    PropertySpec rhs = getValidSpec();
    rhs.tags = {"$/$/$·("};

    EXPECT_NE(*Property::make(lhs), *Property::make(rhs));
  }
}

/// @test
/// Checks notification event of a property
/// @requirements(SEN-355)
TEST(Property, event)
{
  // empty name
  {
    EXPECT_THROW(
      std::ignore = sen::Property::makeChangeNotificationEventName(
        {"", "", sen::StringType::get(), PropertyCategory::dynamicRO, sen::TransportMode::confirmed, false, {}}),
      std::exception);
  }

  // valid name
  {
    const auto result = sen::Property::makeChangeNotificationEventName(
      {"eventTest", "", sen::StringType::get(), PropertyCategory::dynamicRO, sen::TransportMode::confirmed, false, {}});
    EXPECT_EQ(result, "eventTestChanged");
  }
}

/// @test
/// Checks property getter comparison
/// @requirements(SEN-355)
TEST(Property, getter)
{
  // empty name
  {
    EXPECT_THROW(
      std::ignore = sen::Property::makeGetterMethodName(
        {"", "", sen::StringType::get(), PropertyCategory::dynamicRW, sen::TransportMode::unicast, true, {}}),
      std::exception);
  }
  // valid name
  {
    const auto result = sen::Property::makeGetterMethodName(
      {"eventGetter", "", sen::StringType::get(), PropertyCategory::dynamicRW, sen::TransportMode::unicast, true, {}});
    EXPECT_EQ(result, "getEventGetter");
  }
}

/// @test
/// Checks property setter
/// @requirements(SEN-355)
TEST(Property, setter)
{
  // empty name
  {
    EXPECT_THROW(
      std::ignore = sen::Property::makeSetterMethodName(
        {"", "", sen::DurationType::get(), PropertyCategory::staticRO, sen::TransportMode::confirmed, true, {}}),
      std::exception);
  }

  // valid name
  {
    const auto result = sen::Property::makeSetterMethodName({"eventSetter",
                                                             "",
                                                             sen::DurationType::get(),
                                                             PropertyCategory::staticRO,
                                                             sen::TransportMode::confirmed,
                                                             true,
                                                             {}});
    EXPECT_EQ(result, "setNextEventSetter");
  }
}
