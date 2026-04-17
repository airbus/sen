// === unit_registry_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/unit_registry.h"

// google test
#include <gtest/gtest.h>

// std
#include <string>
#include <string_view>

using sen::Unit;
using sen::UnitCategory;
using sen::UnitRegistry;
using sen::UnitSpec;

namespace
{

void checkUnitSpec(const Unit& unit, const UnitSpec& spec)
{
  EXPECT_EQ(unit.getName(), spec.name);
  EXPECT_EQ(unit.getNamePlural(), spec.namePlural);
  EXPECT_EQ(unit.getAbbreviation(), spec.abbreviation);
  EXPECT_EQ(unit.getCategory(), spec.category);
}

void checkBuiltinUnit(std::string_view abbrev, float64_t val, float64_t toSiVal, float64_t fromSiVal)
{
  auto unit = UnitRegistry::get().searchUnitByAbbreviation(abbrev).value();
  EXPECT_TRUE(unit);
  EXPECT_NEAR(toSiVal, unit->toSI(val), 0.0001);
  EXPECT_NEAR(fromSiVal, unit->fromSI(val), 0.0001);
}

void checkBasicSIUnitWithPrefix(std::string_view abbrev, std::string_view prefix, float64_t factor)
{
  std::string str(prefix);
  str.append(abbrev);

  const auto val = 1234.56789;
  const auto toSi = val * factor;
  const auto fromSi = val / factor;

  checkBuiltinUnit(str, val, toSi, fromSi);
}

void checkBasicSIUnit(std::string_view abbrev)
{
  checkBasicSIUnitWithPrefix(abbrev, "", 1.0);
  checkBasicSIUnitWithPrefix(abbrev, "f", 1.0e-15);
  checkBasicSIUnitWithPrefix(abbrev, "p", 1.0e-12);
  checkBasicSIUnitWithPrefix(abbrev, "n", 1.0e-9);
  checkBasicSIUnitWithPrefix(abbrev, "u", 1.0e-6);
  checkBasicSIUnitWithPrefix(abbrev, "m", 1.0e-3);
  checkBasicSIUnitWithPrefix(abbrev, "c", 1.0e-2);
  checkBasicSIUnitWithPrefix(abbrev, "d", 1.0e-1);
  checkBasicSIUnitWithPrefix(abbrev, "da", 1.0e1);
  checkBasicSIUnitWithPrefix(abbrev, "h", 1.0e2);
  checkBasicSIUnitWithPrefix(abbrev, "k", 1.0e3);
  checkBasicSIUnitWithPrefix(abbrev, "M", 1.0e6);
  checkBasicSIUnitWithPrefix(abbrev, "G", 1.0e9);
  checkBasicSIUnitWithPrefix(abbrev, "T", 1.0e12);
  checkBasicSIUnitWithPrefix(abbrev, "P", 1.0e15);
}

void checkSIUnit(std::string_view abbrev)
{
  const auto val = 1234.56789;
  checkBuiltinUnit(abbrev, val, val, val);
}

void checkConversion(float64_t value, std::string_view from, float64_t expectation, std::string_view to)
{
  auto src = UnitRegistry::get().searchUnitByAbbreviation(from).value();
  auto tgt = UnitRegistry::get().searchUnitByAbbreviation(to).value();

  EXPECT_NEAR(expectation, Unit::convert(*src, *tgt, value), 0.000001);
}

void checkUnitInGroup(UnitRegistry::UnitList list, std::string name)
{
  bool found = false;

  for (const auto& unit: list)
  {
    if (unit->getName() == name)
    {
      found = true;
    }
  }

  EXPECT_TRUE(found);
}

}  // namespace

/// @test
/// Checks builtin unit creation
/// @requirements(SEN-894)
TEST(UnitRegistry, builtinUnits)
{
  checkBasicSIUnit("m");
  checkBasicSIUnit("s");
  checkBasicSIUnit("rad");
  checkBasicSIUnit("k");
  checkBasicSIUnit("g");

  checkSIUnit("hz");
  checkSIUnit("m_per_s");
  checkSIUnit("rad_per_s");
  checkSIUnit("m_per_s_sq");
  checkSIUnit("rad_per_s_sq");

  // min
  {
    checkBuiltinUnit("min", 1.0, 60.0, 0.01666);
    checkBuiltinUnit("min", 32.45, 1947.0, 0.54083);
  }

  // hour
  {
    checkBuiltinUnit("hour", 1.0, 3600.0, 2.7777e-4);
    checkBuiltinUnit("hour", 3.5, 12600.0, 9.7222e-4);
  }

  // day
  {
    checkBuiltinUnit("day", 1.0, 86400.0, 1.1574e-5);
    checkBuiltinUnit("day", 0.08, 6912.0, 9.2592e-9);
  }

  // week
  {
    checkBuiltinUnit("week", 1.0, 604800.0, 1.65343e-6);
    checkBuiltinUnit("week", 60480.0, 36578304000.0, 0.1);
  }

  // month
  {
    checkBuiltinUnit("month", 1.0, 2.628e+6, 3.80517e-10);
    checkBuiltinUnit("month", 2300.62, 6046029360, 0.0008754252202);
  }

  // year
  {
    checkBuiltinUnit("year", 1.0, 3.154e+7, 3.17057e-11);
    checkBuiltinUnit("year", 0.0, 0.0, 0.0);
  }

  // feet
  {
    checkBuiltinUnit("ft", 1.0, 0.3048, 3.28083);
    checkBuiltinUnit("ft", 345.678, 105.36265, 1134.11417);
  }

  // miles
  {
    checkBuiltinUnit("mi", 1.0, 1609.344, 0.000621);
    checkBuiltinUnit("mi", 345.678, 556314.81523, 0.21479);
  }

  // nautical miles
  {
    checkBuiltinUnit("nmi", 1.0, 1852.0, 1.0 / 1852.0);
    checkBuiltinUnit("nmi", 345.678, 640195.656, 0.18665);
  }

  // degrees
  {
    checkBuiltinUnit("deg", 1.0, 0.0174532, 57.295779);
    checkBuiltinUnit("deg", 180.0, 3.1415926, 10313.240312);
  }

  // arc minutes
  {
    checkBuiltinUnit("arcmin", 1.0, 0.00029, 3437.74677);
  }

  // arc second
  {
    checkBuiltinUnit("arcsec", 1.0, 4.84813e-06, 206264.80624);
  }

  // centigrades
  {
    checkBuiltinUnit("degC", 0.0, 273.15, -273.15);
    checkBuiltinUnit("degC", 100.0, 373.15, -173.15);
  }

  // fahrenheit
  {
    checkBuiltinUnit("degF", 0.0, 255.3722, -459.67);
    checkBuiltinUnit("degF", 100.0, 310.92777, -279.6699);
  }

  // km per hour
  {
    checkBuiltinUnit("kph", 100.0, 27.7778, 360.0);
  }

  // miles per hour
  {
    checkBuiltinUnit("mph", 100.0, 44.704, 223.69362);
  }

  // degrees per second
  {
    checkBuiltinUnit("deg_per_s", 9.0, 515.66201, 0.15708);
  }

  // revolutions per minute
  {
    checkBuiltinUnit("rpm", 9.0, 85.94366, 0.94247);
  }

  // pounds
  {
    checkBuiltinUnit("lb", 2.0, 907.18474, 0.0044);
  }
}

/// @test
/// Checks unit conversions
/// @requirements(SEN-894)
TEST(UnitRegistry, conversion)
{
  checkConversion(42.5, "ft", 0.0069946, "nmi");
  checkConversion(90.0, "kph", 55.923407, "mph");

  checkConversion(25.0, "m", 82.02099738, "ft");

  checkConversion(90.0, "m_per_s", 174.94600432, "kn");
  checkConversion(90.0, "m_per_s", 295.27559055, "ft_per_s");
  checkConversion(90.0, "m_per_s", 17716.53543307, "ft_per_min");
}

/// @test
/// Check that common length units are registered
/// @requirements(SEN-894)
TEST(UnitRegistry, getLengthUnits)
{
  const auto units = UnitRegistry::get().getUnitsByCategory(UnitCategory::length);

  checkUnitInGroup(units, "meter");
  checkUnitInGroup(units, "foot");
  checkUnitInGroup(units, "mile");
  checkUnitInGroup(units, "nauticalMile");
}

/// @test
/// Check that common mass units are registered
/// @requirements(SEN-894)
TEST(UnitRegistry, getMassUnits)
{
  const auto units = UnitRegistry::get().getUnitsByCategory(UnitCategory::mass);

  checkUnitInGroup(units, "gram");
  checkUnitInGroup(units, "pound");
}

/// @test
/// Check that common velocity units are registered
/// @requirements(SEN-894)
TEST(UnitRegistry, getVelUnits)
{
  const auto units = UnitRegistry::get().getUnitsByCategory(UnitCategory::velocity);

  checkUnitInGroup(units, "meters_per_second");
  checkUnitInGroup(units, "km_per_hour");
  checkUnitInGroup(units, "miles_per_hour");
}

/// @test
/// Check that common temperature units are registered
/// @requirements(SEN-894)
TEST(UnitRegistry, getTempUnits)
{
  const auto units = UnitRegistry::get().getUnitsByCategory(UnitCategory::temperature);

  checkUnitInGroup(units, "fahrenheit");
  checkUnitInGroup(units, "centigrade");
  checkUnitInGroup(units, "kelvin");
}

/// @test
/// Check that common time units are registered
/// @requirements(SEN-894)
TEST(UnitRegistry, getTimeUnits)
{
  const auto units = UnitRegistry::get().getUnitsByCategory(UnitCategory::time);

  checkUnitInGroup(units, "second");
  checkUnitInGroup(units, "min");
  checkUnitInGroup(units, "hour");
  checkUnitInGroup(units, "day");
}

/// @test
/// Check that common angle units are registered
/// @requirements(SEN-894)
TEST(UnitRegistry, getAngleUnits)
{
  const auto units = UnitRegistry::get().getUnitsByCategory(UnitCategory::angle);

  checkUnitInGroup(units, "radian");
  checkUnitInGroup(units, "degree");
}

/// @test
/// Check that common angular vel units are registered
/// @requirements(SEN-894)
TEST(UnitRegistry, getAngVelUnits)
{
  const auto units = UnitRegistry::get().getUnitsByCategory(UnitCategory::angularVelocity);

  checkUnitInGroup(units, "radians_per_second");
  checkUnitInGroup(units, "revolutions_per_min");
  checkUnitInGroup(units, "degrees_per_second");
}

/// @test
/// Check that common angular accel units are registered
/// @requirements(SEN-894)
TEST(UnitRegistry, getAngAccUnits)
{
  const auto units = UnitRegistry::get().getUnitsByCategory(UnitCategory::angularAcceleration);

  checkUnitInGroup(units, "radians_per_second_squared");
}

/// @test
/// Check that common frequency units are registered
/// @requirements(SEN-894)
TEST(UnitRegistry, getFrequencyUnits)
{
  const auto units = UnitRegistry::get().getUnitsByCategory(UnitCategory::frequency);

  checkUnitInGroup(units, "hertz");
}

/// @test
/// Check that common torque units are registered
/// @requirements(SEN-894)
TEST(UnitRegistry, getTorqueUnits)
{
  const auto units = UnitRegistry::get().getUnitsByCategory(UnitCategory::torque);

  checkUnitInGroup(units, "newton_meter");
}
