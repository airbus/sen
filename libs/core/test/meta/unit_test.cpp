// === unit_test.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/meta/unit.h"

// google test
#include <gtest/gtest.h>

using sen::Unit;
using sen::UnitCategory;
using sen::UnitSpec;

namespace
{

const UnitSpec& validSpec()
{
  static const UnitSpec validSpec = {UnitCategory::length, "meter", "meters", "m", 1.0, 0.0, 0.0};
  return validSpec;
}

void checkSpecData(const Unit& unit, const UnitSpec& spec)
{
  EXPECT_EQ(unit.getCategory(), spec.category);
  EXPECT_EQ(unit.getName(), spec.name);
  EXPECT_EQ(unit.getNamePlural(), spec.namePlural);
  EXPECT_EQ(unit.getAbbreviation(), spec.abbreviation);
}

void checkFromSI(float64_t val, const Unit& unit, const UnitSpec& spec)
{
  EXPECT_EQ(unit.fromSI(val), ((val - spec.x) / spec.f) - spec.y);
}

void checkToSI(float64_t val, const Unit& unit, const UnitSpec& spec)
{
  EXPECT_EQ(unit.toSI(val), ((val + spec.y) * spec.f) + spec.x);
}

void checkToFromSI(float64_t val, const Unit& unit, const UnitSpec& spec)
{
  checkFromSI(val, unit, spec);
  checkToSI(val, unit, spec);

  EXPECT_DOUBLE_EQ(unit.fromSI(unit.toSI(val)), val);
}

}  // namespace

/// @test
/// Checks unit spec comparison
/// @requirements(SEN-894)
TEST(Unit, specComparison)
{
  const auto& lhs = validSpec();

  // same
  {
    const UnitSpec& rhs = validSpec();
    EXPECT_EQ(lhs, rhs);
  }

  // different name
  {
    UnitSpec rhs = validSpec();
    rhs.name = validSpec().name + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different plural name
  {
    UnitSpec rhs = validSpec();
    rhs.namePlural = validSpec().namePlural + "X";
    EXPECT_NE(lhs, rhs);
  }

  // different category
  {
    UnitSpec rhs = validSpec();
    rhs.category = UnitCategory::temperature;
    EXPECT_NE(lhs, rhs);
  }

  // different abbreviation
  {
    UnitSpec rhs = validSpec();
    rhs.abbreviation = validSpec().abbreviation + "X";
    EXPECT_NE(lhs, rhs);
  }
}

/// @test
/// Checks comparison between different units
/// @requirements(SEN-894)
TEST(Unit, comparison)
{
  // same content
  {
    const auto& spec = validSpec();
    auto type1 = Unit::make(spec);
    auto type2 = Unit::make(spec);

    EXPECT_EQ(*type1, *type2);
  }

  // same instance
  {
    const auto& spec = validSpec();
    auto type1 = Unit::make(spec);
    EXPECT_EQ(*type1, *type1);
  }

  // different name
  {
    const auto& lhs = validSpec();
    UnitSpec rhs = validSpec();
    rhs.name = validSpec().name + "X";

    auto type1 = Unit::make(lhs);
    auto type2 = Unit::make(rhs);

    EXPECT_NE(*type1, *type2);
  }

  // different plural name
  {
    const auto& lhs = validSpec();
    UnitSpec rhs = validSpec();
    rhs.namePlural = validSpec().namePlural + "X";

    auto type1 = Unit::make(lhs);
    auto type2 = Unit::make(rhs);

    EXPECT_NE(*type1, *type2);
  }

  // different abbreviation
  {
    const auto& lhs = validSpec();
    UnitSpec rhs = validSpec();
    rhs.abbreviation = validSpec().abbreviation + "X";

    auto type1 = Unit::make(lhs);
    auto type2 = Unit::make(rhs);

    EXPECT_NE(*type1, *type2);
  }

  // different category
  {
    const auto& lhs = validSpec();
    UnitSpec rhs = validSpec();
    rhs.category = UnitCategory::temperature;

    auto type1 = Unit::make(lhs);
    auto type2 = Unit::make(rhs);

    EXPECT_NE(*type1, *type2);
  }
}

/// @test
/// Checks correct unit conversion from/to international system
/// @requirements(SEN-894)
TEST(Unit, toAndFromSI)
{
  const UnitSpec spec = {UnitCategory::length, "unit", "units", "u", 123.45, -67.8, 9.1011};
  auto unit = Unit::make(spec);

  for (auto item: {0.0, 14.4, 23.52, 10002.34, 99999999.99999})
  {
    checkToFromSI(item, *unit, spec);
    checkToFromSI(-item, *unit, spec);
  }
}

/// @test
/// Checks fromString with a numeric-only string
/// @requirements(SEN-894)
TEST(Unit, fromStringNumericOnly)
{
  const UnitSpec spec = {UnitCategory::length, "testunit", "testunits", "tu", 1.0, 0.0, 0.0};
  auto unit = Unit::make(spec);

  auto result = unit->fromString("10.5");
  EXPECT_TRUE(result.isOk());
  EXPECT_DOUBLE_EQ(result.getValue(), 10.5);
}

/// @test
/// Checks fromString with a value that causes out of range
/// @requirements(SEN-894)
TEST(Unit, fromStringOutOfRange)
{
  const UnitSpec spec = {UnitCategory::length, "testunit2", "testunits2", "tu2", 1.0, 0.0, 0.0};
  auto unit = Unit::make(spec);

  const std::string testNumber(500, '9');
  auto result = unit->fromString(testNumber);
  EXPECT_TRUE(result.isError());
  EXPECT_NE(result.getError().find("did not fit the underlying storage type"), std::string::npos);
}

/// @test
/// Checks getCategoryString for all unit categories
/// @requirements(SEN-894)
TEST(Unit, getCategoryString)
{
  EXPECT_EQ(Unit::getCategoryString(UnitCategory::length), "length");
  EXPECT_EQ(Unit::getCategoryString(UnitCategory::mass), "mass");
  EXPECT_EQ(Unit::getCategoryString(UnitCategory::time), "time");
  EXPECT_EQ(Unit::getCategoryString(UnitCategory::angle), "angle");
  EXPECT_EQ(Unit::getCategoryString(UnitCategory::temperature), "temperature");
  EXPECT_EQ(Unit::getCategoryString(UnitCategory::density), "density");
  EXPECT_EQ(Unit::getCategoryString(UnitCategory::pressure), "pressure");
  EXPECT_EQ(Unit::getCategoryString(UnitCategory::area), "area");
  EXPECT_EQ(Unit::getCategoryString(UnitCategory::force), "force");
  EXPECT_EQ(Unit::getCategoryString(UnitCategory::frequency), "frequency");
  EXPECT_EQ(Unit::getCategoryString(UnitCategory::velocity), "velocity");
  EXPECT_EQ(Unit::getCategoryString(UnitCategory::angularVelocity), "angular velocity");
  EXPECT_EQ(Unit::getCategoryString(UnitCategory::acceleration), "acceleration");
  EXPECT_EQ(Unit::getCategoryString(UnitCategory::angularAcceleration), "angular acceleration");
}
