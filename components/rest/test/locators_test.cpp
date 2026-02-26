// === locators_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "locators.h"

// generated code
#include "stl/types.stl.h"

// sen
#include <sen/core/base/result.h>

// google test
#include <gtest/gtest.h>

using namespace sen::components::rest;  // NOLINT

/// @test
/// Verify bus locator building
/// @requirements(SEN-1061)
TEST(Rest, bus_locator)
{
  {
    sen::Result<BusLocator, LocatorError> ret = BusLocator::build("session", "bus");
    ASSERT_TRUE(ret.isOk());
    ASSERT_EQ(ret.getValue().session(), "session");
    ASSERT_EQ(ret.getValue().bus(), "bus");
    ASSERT_EQ(ret.getValue().toBusAddress(), "session.bus");
  }
  {
    sen::Result<BusLocator, LocatorError> ret = BusLocator::build("", "bus");
    ASSERT_TRUE(ret.isError());
    ASSERT_EQ(ret.getError().errorType, LocatorErrorType::emptySessionName);
  }
  {
    sen::Result<BusLocator, LocatorError> ret = BusLocator::build("session", "");
    ASSERT_TRUE(ret.isError());
    ASSERT_EQ(ret.getError().errorType, LocatorErrorType::emptyBusName);
  }
}

/// @test
/// Verify bus locator building from an interest
/// @requirements(SEN-1061)
TEST(Rest, bus_locator_from_interest)
{
  {
    Interest interest {"SELECT * FROM local.shell", "name"};
    sen::Result<BusLocator, LocatorError> ret = BusLocator::build(interest);
    ASSERT_TRUE(ret.isOk());
    ASSERT_EQ(ret.getValue().session(), "local");
    ASSERT_EQ(ret.getValue().bus(), "shell");
    ASSERT_EQ(ret.getValue().toBusAddress(), "local.shell");
  }
  {
    Interest interest {"query error", "name"};
    sen::Result<BusLocator, LocatorError> ret = BusLocator::build(interest);
    ASSERT_TRUE(ret.isError());
  }
}

/// @test
/// Verify object locator building from an interest
/// @requirements(SEN-1061)
TEST(Rest, object_locator_from_interest)
{
  Interest interest {"SELECT * FROM local.shell", "name"};
  auto busLocator = BusLocator::build(interest).getValue();

  sen::Result<ObjectLocator, LocatorError> ret = ObjectLocator::build(busLocator, "object");
  ASSERT_TRUE(ret.isOk());
  ASSERT_EQ(ret.getValue().session(), "local");
  ASSERT_EQ(ret.getValue().bus(), "shell");
  ASSERT_EQ(ret.getValue().object(), "object");
  ASSERT_EQ(ret.getValue().toObjectAddress(), "local.shell.object");
}

/// @test
/// Verify method locator building from an interest
/// @requirements(SEN-1061)
TEST(Rest, method_locator_from_interest)
{
  Interest interest {"SELECT * FROM local.shell", "name"};
  auto busLocator = BusLocator::build(interest).getValue();

  sen::Result<MethodLocator, LocatorError> ret = MethodLocator::build(busLocator, "object", "method");
  ASSERT_TRUE(ret.isOk());
  ASSERT_EQ(ret.getValue().session(), "local");
  ASSERT_EQ(ret.getValue().bus(), "shell");
  ASSERT_EQ(ret.getValue().object(), "object");
  ASSERT_EQ(ret.getValue().method(), "method");
}

/// @test
/// Verify property locator building from an interest
/// @requirements(SEN-1061)
TEST(Rest, property_locator_from_interest)
{

  Interest interest {"SELECT * FROM local.shell", "name"};
  auto busLocator = BusLocator::build(interest).getValue();

  sen::Result<PropertyLocator, LocatorError> ret = PropertyLocator::build(busLocator, "object", "property");
  ASSERT_TRUE(ret.isOk());
  ASSERT_EQ(ret.getValue().session(), "local");
  ASSERT_EQ(ret.getValue().bus(), "shell");
  ASSERT_EQ(ret.getValue().object(), "object");
  ASSERT_EQ(ret.getValue().property(), "property");
}

/// @test
/// Verify event locator building from an interest
/// @requirements(SEN-1061)
TEST(Rest, event_locator_from_interest)
{

  Interest interest {"SELECT * FROM local.shell", "name"};
  auto busLocator = BusLocator::build(interest).getValue();

  sen::Result<EventLocator, LocatorError> ret = EventLocator::build(busLocator, "object", "event");
  ASSERT_TRUE(ret.isOk());
  ASSERT_EQ(ret.getValue().session(), "local");
  ASSERT_EQ(ret.getValue().bus(), "shell");
  ASSERT_EQ(ret.getValue().object(), "object");
  ASSERT_EQ(ret.getValue().event(), "event");
}
