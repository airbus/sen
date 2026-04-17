// === unit_registry.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/unit_registry.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/iterator_adapters.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/unit.h"

// std
#include <optional>
#include <ratio>
#include <string>
#include <string_view>
#include <utility>

namespace sen
{

namespace
{

constexpr float64_t piValue = 3.14159265358979323846264338327950288419716939937510;

/// Returns the ratio as a float64_t
template <typename R>
[[nodiscard]] constexpr float64_t fromRatio() noexcept
{
  return static_cast<float64_t>(R::num) / static_cast<float64_t>(R::den);
}

void throwIfRepeatedOrInvalid(const UnitRegistry::UnitStorage& list, const UnitSpec& spec)
{
  if (spec.name.empty())
  {
    std::string err;
    err.append("name is empty");
    sen::throwRuntimeError(err);
  }

  if (spec.namePlural.empty())
  {
    std::string err;
    err.append("plural name is empty");
    sen::throwRuntimeError(err);
  }

  if (spec.abbreviation.empty())
  {
    std::string err;
    err.append("abbreviation is empty");
    sen::throwRuntimeError(err);
  }

  for (const auto& item: list)
  {
    if (item->getName() == spec.name)
    {
      std::string err;
      err.append("name '");
      err.append(spec.name);
      err.append("' is already in use by another unit");
      sen::throwRuntimeError(err);
    }

    if (item->getNamePlural() == spec.namePlural)
    {
      std::string err;
      err.append("plural name '");
      err.append(spec.namePlural);
      err.append("' is already in use by another unit");
      sen::throwRuntimeError(err);
    }

    if (item->getAbbreviation() == spec.abbreviation)
    {
      std::string err;
      err.append("abbreviation '");
      err.append(spec.abbreviation);
      err.append("' is already in use by another unit");
      sen::throwRuntimeError(err);
    }
  }
}

/// Adds a unit to the list, ensuring that is unique.
/// Throws std::exception if it is not.
void addUnit(UnitRegistry::UnitStorage& list, UnitSpec spec)
{
  throwIfRepeatedOrInvalid(list, spec);
  list.push_back(Unit::make(std::move(spec)));
}

/// Adds a SI unit, prepending the prefixes to the name and abbreviation.
void registerSIUnitElement(UnitRegistry::UnitStorage& list,
                           UnitCategory category,
                           std::string_view name,
                           std::string_view namePlural,
                           std::string_view abbrev,
                           std::string_view prefix,
                           std::string_view abbrPrefix,
                           float64_t factor)
{
  std::string unitName(prefix);
  unitName.append(name);

  std::string unitNamePlural(prefix);
  unitNamePlural.append(namePlural);

  std::string unitAbbrev(abbrPrefix);
  unitAbbrev.append(abbrev);

  auto spec = UnitSpec {category, unitName, unitNamePlural, unitAbbrev, factor, 0.0, 0.0};

  addUnit(list, std::move(spec));
}

/// Adds the basic SI unit and the ones related to the metric prefixes.
void registerBasicUnit(UnitRegistry::UnitStorage& list,
                       UnitCategory cat,
                       const std::string& name,
                       const std::string& plural,
                       const std::string& abbrev)
{
  using namespace std::string_literals;

  addUnit(list, {cat, name, plural, abbrev, 1.0, 0.0, 0.0});

  // clang-format off
  registerSIUnitElement(list, cat, name, plural, abbrev, "femto", "f", fromRatio<std::femto>());
  registerSIUnitElement(list, cat, name, plural, abbrev, "pico",  "p", fromRatio<std::pico>());
  registerSIUnitElement(list, cat, name, plural, abbrev, "nano",  "n", fromRatio<std::nano>());
  registerSIUnitElement(list, cat, name, plural, abbrev, "micro", "u", fromRatio<std::micro>());
  registerSIUnitElement(list, cat, name, plural, abbrev, "milli", "m", fromRatio<std::milli>());
  registerSIUnitElement(list, cat, name, plural, abbrev, "centi", "c", fromRatio<std::centi>());
  registerSIUnitElement(list, cat, name, plural, abbrev, "deci",  "d", fromRatio<std::deci>());
  registerSIUnitElement(list, cat, name, plural, abbrev, "deca", "da", fromRatio<std::deca>());
  registerSIUnitElement(list, cat, name, plural, abbrev, "hecto", "h", fromRatio<std::hecto>());
  registerSIUnitElement(list, cat, name, plural, abbrev, "kilo",  "k", fromRatio<std::kilo>());
  registerSIUnitElement(list, cat, name, plural, abbrev, "mega",  "M", fromRatio<std::mega>());
  registerSIUnitElement(list, cat, name, plural, abbrev, "giga",  "G", fromRatio<std::giga>());
  registerSIUnitElement(list, cat, name, plural, abbrev, "tera",  "T", fromRatio<std::tera>());
  registerSIUnitElement(list, cat, name, plural, abbrev, "peta",  "P", fromRatio<std::peta>());
  // clang-format on
}

}  // namespace

const UnitRegistry& UnitRegistry::get()
{
  static UnitRegistry instance;
  return instance;
}

UnitRegistry::UnitRegistry()
{
  // clang-format off

  // basic SI units
  registerBasicUnit(units_, UnitCategory::length,          "meter",                      "meters",                     "m");
  registerBasicUnit(units_, UnitCategory::time,            "second",                     "seconds",                    "s");
  registerBasicUnit(units_, UnitCategory::angle,           "radian",                     "radians",                    "rad");
  registerBasicUnit(units_, UnitCategory::temperature,     "kelvin",                     "kelvin",                     "k");
  registerBasicUnit(units_, UnitCategory::mass,            "gram",                       "grams",                      "g");
  registerBasicUnit(units_, UnitCategory::angularVelocity, "radians_per_second",         "radians_per_second",         "rad_per_s");
  registerBasicUnit(units_, UnitCategory::density,         "grams_per_centimeters_cube", "grams_per_centimeters_cube", "g_per_cm3");
  registerBasicUnit(units_, UnitCategory::pressure,        "pascals",                    "pascals",                    "pa");
  registerBasicUnit(units_, UnitCategory::area,            "square_meter",               "square_meter",               "m_sq");
  registerBasicUnit(units_, UnitCategory::force,           "newton",                     "newton",                     "nw");

  // derived SI units
  addUnit(units_, {UnitCategory::frequency,           "hertz",                      "hertz",                      "hz",           1.0,      0.0, 0.0});
  addUnit(units_, {UnitCategory::velocity,            "meters_per_second",          "meters_per_second",          "m_per_s",      1.0,      0.0, 0.0});
  addUnit(units_, {UnitCategory::velocity,            "decimeters_per_second",      "decimeters_per_second",      "dm_per_s",     0.1,      0.0, 0.0});
  addUnit(units_, {UnitCategory::acceleration,        "meters_per_second_squared",  "meters_per_second_squared",  "m_per_s_sq",   1.0,      0.0, 0.0});
  addUnit(units_, {UnitCategory::angularAcceleration, "radians_per_second_squared", "radians_per_second_squared", "rad_per_s_sq", 1.0,      0.0, 0.0});
  addUnit(units_, {UnitCategory::time,                "min",                        "minutes",                    "min",          60.0,     0.0, 0.0});
  addUnit(units_, {UnitCategory::time,                "hour",                       "hours",                      "hour",         3600.0,   0.0, 0.0});
  addUnit(units_, {UnitCategory::time,                "day",                        "days",                       "day",          86400.0,  0.0, 0.0});
  addUnit(units_, {UnitCategory::time,                "week",                       "weeks",                      "week",         604800.0, 0.0, 0.0});
  addUnit(units_, {UnitCategory::time,                "month",                      "months",                     "month",        2.628e+6, 0.0, 0.0});
  addUnit(units_, {UnitCategory::time,                "year",                       "years",                      "year",         3.154e+7, 0.0, 0.0});
  addUnit(units_, {UnitCategory::torque,              "newton_meter",               "newton_meters",              "Nm",           1.0,      0.0, 0.0});

  // other length units
  addUnit(units_, {UnitCategory::length, "foot",         "feet",          "ft",  381.0 / 1250.0, 0.0, 0.0});         // NOLINT(readability-magic-numbers)
  addUnit(units_, {UnitCategory::length, "mile",         "miles",         "mi",  1609.344,       0.0, 0.0});         // NOLINT(readability-magic-numbers)
  addUnit(units_, {UnitCategory::length, "nauticalMile", "nauticalMiles", "nmi", 1852.0,         0.0, 0.0});         // NOLINT(readability-magic-numbers)

  // other angle units
  addUnit(units_, {UnitCategory::angle, "degree",    "degrees",    "deg",               piValue / 180.0, 0.0, 0.0}); // NOLINT(readability-magic-numbers)
  addUnit(units_, {UnitCategory::angle, "arcminute", "arcminutes", "arcmin",   piValue / (180.0 * 60.0), 0.0, 0.0}); // NOLINT(readability-magic-numbers)
  addUnit(units_, {UnitCategory::angle, "arcsecond", "arcseconds", "arcsec", piValue / (180.0 * 3600.0), 0.0, 0.0}); // NOLINT(readability-magic-numbers)

  // other temperature units
  addUnit(units_, {UnitCategory::temperature, "centigrade", "centigrades", "degC",       1.0, 273.15,   0.0});       // NOLINT(readability-magic-numbers)
  addUnit(units_, {UnitCategory::temperature, "fahrenheit", "fahrenheit",  "degF", 5.0 / 9.0, 273.15, -32.0});       // NOLINT(readability-magic-numbers)

  // other velocity units
  addUnit(units_, {UnitCategory::velocity, "km_per_hour",     "km_per_hour",    "kph", 5.0 / 18.0, 0.0, 0.0});       // NOLINT(readability-magic-numbers)
  addUnit(units_, {UnitCategory::velocity, "miles_per_hour",  "miles_per_hour", "mph",    0.44704, 0.0, 0.0});       // NOLINT(readability-magic-numbers)
  addUnit(units_, {UnitCategory::velocity, "knot",            "knots",          "kn",           1852.0 / 3600.0,       0.0, 0.0});  // NOLINT(readability-magic-numbers)
  addUnit(units_, {UnitCategory::velocity, "feet_per_second", "feets_per_second", "ft_per_s",    381.0 / 1250.0,        0.0, 0.0});  // NOLINT(readability-magic-numbers)
  addUnit(units_, {UnitCategory::velocity, "feet_per_minute", "feets_per_minute",  "ft_per_min",  381.0 / (60 * 1250.0), 0.0, 0.0});  // NOLINT(readability-magic-numbers)

  // other angular velocity units
  addUnit(units_, {UnitCategory::angularVelocity, "degrees_per_second", "degrees_per_second",   "deg_per_s",      180.0 / piValue, 0.0, 0.0}); // NOLINT
  addUnit(units_, {UnitCategory::angularVelocity, "revolutions_per_min", "revolutions_per_min", "rpm",       60.0 / (2.0 * piValue), 0.0, 0.0}); // NOLINT

  // other mass units
  addUnit(units_, {UnitCategory::mass, "pound",    "pounds",    "lb",               453.59237, 0.0, 0.0});          // NOLINT(readability-magic-numbers)

  // other density units
  addUnit(units_, {UnitCategory::density, "kilograms_per_meters_cube",    "kilograms_per_meters_cube",    "kg_per_m3", 0.001, 0.0, 0.0});          // NOLINT(readability-magic-numbers)

  // clang-format on
}

UnitRegistry::UnitRange UnitRegistry::getUnits() const noexcept
{
  return util::makeRange(util::SmartPtrIteratorAdapter(units_.begin()), util::SmartPtrIteratorAdapter(units_.end()));
}

UnitRegistry::UnitList UnitRegistry::getUnitsByCategory(UnitCategory category) const
{
  UnitList result;

  for (const auto& item: units_)
  {
    if (item->getCategory() == category)
    {
      result.push_back(item.get());
    }
  }

  return result;
}

std::optional<const Unit*> UnitRegistry::searchUnitByName(std::string_view name) const
{
  for (const auto& item: units_)
  {
    if (item->getName() == name)
    {
      return item.get();
    }
  }

  return std::nullopt;
}

std::optional<const Unit*> UnitRegistry::searchUnitByAbbreviation(std::string_view abbrev) const
{
  // search in our list
  for (const auto& item: units_)
  {
    if (item->getAbbreviation() == abbrev)
    {
      return item.get();
    }
  }

  return std::nullopt;
}

}  // namespace sen
