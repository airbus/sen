// === unit.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/unit.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/result.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/unit_registry.h"

// std
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace sen
{

Result<float64_t, std::string> Unit::fromString(const std::string& str) const noexcept
{
  try
  {
    if (auto unitPos = str.find_first_not_of("-0123456789. ", 0); unitPos != std::string::npos)
    {
      auto unitString = str.substr(unitPos);
      auto value = std::stod(str.substr(0, unitPos));

      if (unitString == getAbbreviation() || unitString == getName() || unitString == getNamePlural())
      {
        return Ok(value);
      }

      auto availableUnits = UnitRegistry::get().getUnitsByCategory(getCategory());
      for (const auto& otherUnit: availableUnits)
      {
        if (unitString == otherUnit->getAbbreviation() || unitString == otherUnit->getName() ||
            unitString == otherUnit->getNamePlural())
        {
          value = convert(*otherUnit, *this, value);
          return Ok(value);
        }
      }

      std::string err;
      err.append("did not find any unit named '");
      err.append(unitString);
      err.append("'");
      return Err(err);
    }

    return Ok(std::stod(str));
  }
  catch (const std::invalid_argument& /*err*/)
  {
    std::string errStr;
    errStr.append("could not perform a valid conversion of the string '");
    errStr.append(str);
    errStr.append("' to a value of ");
    errStr.append(getNamePlural());
    return Err(errStr);
  }
  catch (const std::out_of_range& /*err*/)
  {
    std::string errStr;
    errStr.append("the numeric value contained in the string '");
    errStr.append(str);
    errStr.append("' did not fit the underlying storage type used for ");
    errStr.append(getNamePlural());
    return Err(errStr);
  }
}

std::string_view Unit::getCategoryString(UnitCategory category) noexcept
{
  switch (category)
  {
    case UnitCategory::length:
      return "length";
    case UnitCategory::mass:
      return "mass";
    case UnitCategory::time:
      return "time";
    case UnitCategory::angle:
      return "angle";
    case UnitCategory::temperature:
      return "temperature";
    case UnitCategory::density:
      return "density";
    case UnitCategory::pressure:
      return "pressure";
    case UnitCategory::area:
      return "area";
    case UnitCategory::force:
      return "force";
    case UnitCategory::frequency:
      return "frequency";
    case UnitCategory::velocity:
      return "velocity";
    case UnitCategory::angularVelocity:
      return "angular velocity";
    case UnitCategory::acceleration:
      return "acceleration";
    case UnitCategory::angularAcceleration:
      return "angular acceleration";
    default:
      return "unknown";
      SEN_UNREACHABLE();
  }
}

MemberHash Unit::getHash() const noexcept { return hash_; }

std::unique_ptr<Unit> Unit::make(UnitSpec spec) { return std::make_unique<Unit>(std::move(spec), Unit::Private()); }

Unit::Unit(UnitSpec spec, Private notUsable) noexcept: spec_(std::move(spec)), hash_ {impl::hash<UnitSpec>()(spec_)}
{
  std::ignore = notUsable;
}

UnitCategory Unit::getCategory() const noexcept { return spec_.category; }

std::string_view Unit::getName() const noexcept { return spec_.name; }

std::string_view Unit::getNamePlural() const noexcept { return spec_.namePlural; }

std::string_view Unit::getAbbreviation() const noexcept { return spec_.abbreviation; }

float64_t Unit::toSI(float64_t value) const noexcept { return ((value + spec_.y) * spec_.f) + spec_.x; }

float64_t Unit::fromSI(float64_t value) const noexcept { return ((value - spec_.x) / spec_.f) - spec_.y; }

float64_t Unit::convert(const Unit& from, const Unit& to, float64_t value) noexcept
{
  return to.fromSI(from.toSI(value));
}

bool Unit::operator==(const Unit& other) const noexcept { return spec_ == other.spec_; }

bool Unit::operator!=(const Unit& other) const noexcept { return !(*this == other); }

}  // namespace sen
