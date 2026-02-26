// === event_test.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/span.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/type.h"

// google test
#include <gtest/gtest.h>

// std
#include <exception>
#include <tuple>
#include <vector>

using sen::Arg;
using sen::Event;
using sen::EventSpec;
using sen::TransportMode;

namespace
{
const EventSpec& validSpec()
{
  static const std::vector<Arg> args = {{"one", "first", sen::StringType::get()},
                                        {"two", "second", sen::Int64Type::get()},
                                        {"three", "third", sen::Int64Type::get()}};
  static const EventSpec validSpec = {{"name", "description", args, TransportMode::confirmed}};
  return validSpec;
}

void checkSpecData(const Event& type, const EventSpec& spec)
{
  EXPECT_EQ(type.getName(), spec.callableSpec.name);
  EXPECT_EQ(type.getDescription(), spec.callableSpec.description);
  EXPECT_EQ(type.getTransportMode(), spec.callableSpec.transportMode);
  EXPECT_EQ(type.getArgs(), sen::makeConstSpan(spec.callableSpec.args));
}

void checkInvalidSpec(const EventSpec& spec) { EXPECT_THROW(std::ignore = Event::make(spec), std::exception); }

}  // namespace

/// @test
/// Checks comparison between event specs
/// @requirements(SEN-574)
TEST(Event, specComparison)
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
    EventSpec rhs = validSpec();
    rhs.callableSpec.name = validSpec().callableSpec.name + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different transport mode
  {
    EventSpec rhs = validSpec();
    rhs.callableSpec.transportMode = TransportMode::unicast;
    EXPECT_NE(lhs, rhs);
  }

  // different description
  {
    EventSpec rhs = validSpec();
    rhs.callableSpec.description = validSpec().callableSpec.description + "X";
    EXPECT_NE(lhs, rhs);
  }

  // extra arg
  {
    EventSpec rhs = validSpec();
    rhs.callableSpec.args.emplace_back("four", "fourth", sen::StringType::get());
    EXPECT_NE(lhs, rhs);
  }

  // fewer arg
  {
    EventSpec rhs = validSpec();
    rhs.callableSpec.args.pop_back();
    EXPECT_NE(lhs, rhs);
  }

  // no args
  {
    EventSpec rhs = validSpec();
    rhs.callableSpec.args = {};
    EXPECT_NE(lhs, rhs);
  }

  // no spec
  {
    EventSpec rhs = validSpec();
    rhs = {};
    EXPECT_NE(lhs, rhs);
  }

  // no callable spec
  {
    EventSpec rhs = validSpec();
    rhs.callableSpec = {};
    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks correct event instance creation from spec
/// @requirements(SEN-574)
TEST(Event, makeBasic)
{
  // simple
  {
    const auto& spec = validSpec();
    const auto type = Event::make(spec);
    checkSpecData(*type, spec);
  }
}

/// @test
/// Checks invalid creation of event
/// @requirements(SEN-574)
TEST(Event, makeInvalid)
{
  // spec with no name
  {
    EventSpec invalid = validSpec();
    invalid.callableSpec.name = {};
    checkInvalidSpec(invalid);
  }

  // spec with invalid name
  {
    EventSpec invalid = validSpec();
    invalid.callableSpec.name = "SomeCallableEvent";
    checkInvalidSpec(invalid);
  }

  // spec with invalid name 2
  {
    EventSpec invalid = validSpec();
    invalid.callableSpec.name = "$&$&$/·(";
    checkInvalidSpec(invalid);
  }

  // spec with repeated argument
  {
    EventSpec invalid = validSpec();
    invalid.callableSpec.args.emplace_back("three", "third", sen::Int64Type::get());

    checkInvalidSpec(invalid);
  }
}

/// @test
/// Checks correct event type comparison
/// @requirements(SEN-574)
TEST(Event, comparison)
{
  // same content
  {
    const auto& spec = validSpec();
    auto type1 = Event::make(spec);
    auto type2 = Event::make(spec);
    EXPECT_EQ(*type1, *type2);
  }

  // same instance
  {
    const auto& spec = validSpec();
    auto type1 = Event::make(spec);
    EXPECT_EQ(*type1, *type1);
  }

  // different name
  {
    const auto& lhs = validSpec();
    EventSpec rhs = validSpec();
    rhs.callableSpec.name = validSpec().callableSpec.name + "X";

    EXPECT_NE(*Event::make(lhs), *Event::make(rhs));
  }

  // different qualified name
  {
    const auto& lhs = validSpec();
    EventSpec rhs = validSpec();
    rhs.callableSpec.transportMode = TransportMode::multicast;
    EXPECT_NE(*Event::make(lhs), *Event::make(rhs));
  }

  // different description
  {
    const auto& lhs = validSpec();
    EventSpec rhs = validSpec();
    rhs.callableSpec.description = validSpec().callableSpec.description + "X";
    EXPECT_NE(*Event::make(lhs), *Event::make(rhs));
  }

  // extra fields
  {
    const auto& lhs = validSpec();
    EventSpec rhs = validSpec();
    rhs.callableSpec.args.emplace_back("four", "fourth", sen::Int64Type::get());
    EXPECT_NE(*Event::make(lhs), *Event::make(rhs));
  }

  // fewer fields
  {
    const auto& lhs = validSpec();
    EventSpec rhs = validSpec();
    rhs.callableSpec.args.pop_back();
    EXPECT_NE(*Event::make(lhs), *Event::make(rhs));
  }
}
