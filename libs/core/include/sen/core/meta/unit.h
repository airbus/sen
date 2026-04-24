// === unit.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_UNIT_H
#define SEN_CORE_META_UNIT_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/type.h"

// std
#include <memory>
#include <string>
#include <string_view>

namespace sen
{

/// \addtogroup type_utils
/// @{

/// Type of measurement
enum class UnitCategory
{
  // Base measurements
  length,
  mass,
  time,
  angle,
  temperature,
  density,
  pressure,
  area,
  force,

  // Derived measurements
  frequency,
  velocity,
  angularVelocity,
  acceleration,
  angularAcceleration,
  torque
};

/// Data about a given unit definition.
/// Given f, x, y:
///
/// SI(val)   = ((val + y) * f) + x
/// Unit(val) = ((val - x) / f) - y
struct UnitSpec
{
  // Comparison operators
  friend bool operator==(const UnitSpec& lhs, const UnitSpec& rhs) noexcept
  {
    return lhs.category == rhs.category && lhs.name == rhs.name && lhs.namePlural == rhs.namePlural &&
           lhs.abbreviation == rhs.abbreviation && lhs.f == rhs.f && lhs.x == rhs.x && lhs.y == rhs.y;
  }

  friend bool operator!=(const UnitSpec& lhs, const UnitSpec& rhs) noexcept { return !(lhs == rhs); }

public:
  UnitCategory category;     ///< The type of measurement.
  std::string name;          ///< Plain name (i.e. "meter").
  std::string namePlural;    ///< Plural name (i.e. "meters").
  std::string abbreviation;  ///< Unique abbreviation (i.e. "m").
  float64_t f;               ///< Multiplication factor to convert to SI.
  float64_t x;               ///< Offset to convert to SI.
  float64_t y;               ///< Offset to convert to SI.
};

/// Represents the definition of a unit.
class Unit
{
  SEN_COPY_MOVE(Unit)
  SEN_PRIVATE_TAG

public:
  /// Constructor is only usable by this class.
  /// Use make (see below).
  Unit(UnitSpec spec, Private notUsable) noexcept;
  ~Unit() noexcept = default;

public:
  /// Moves the spec into a member.
  static std::unique_ptr<Unit> make(UnitSpec spec);

  struct EnsureTag
  {
  };
  /// Tag to request that a unit is required.
  static constexpr inline EnsureTag ensurePresent {};

public:
  /// Measurement type.
  [[nodiscard]] UnitCategory getCategory() const noexcept;

  /// Plain name.
  [[nodiscard]] std::string_view getName() const noexcept;

  /// Plural name.
  [[nodiscard]] std::string_view getNamePlural() const noexcept;

  /// Unique abbreviation.
  [[nodiscard]] std::string_view getAbbreviation() const noexcept;

  /// Converts 'value' given in this unit to an equivalent SI unit.
  [[nodiscard]] float64_t toSI(float64_t value) const noexcept;

  /// Converts 'value' given in SI to this unit.
  [[nodiscard]] float64_t fromSI(float64_t value) const noexcept;

  /// Parses a string, searching for a valid unit specification within the same category.
  /// Returns a valid value if the string contains a number and it also converts the value
  /// from the specified unit.
  [[nodiscard]] Result<float64_t, std::string> fromString(const std::string& str) const noexcept;

  /// Converts a a value between units.
  [[nodiscard]] static float64_t convert(const Unit& from, const Unit& to, float64_t value) noexcept;

  /// A string that describes the category.
  [[nodiscard]] static std::string_view getCategoryString(UnitCategory category) noexcept;

  /// Returns a unique hash computed for the UnitSpec at compilation time
  [[nodiscard]] MemberHash getHash() const noexcept;

public:  /// Comparison
  [[nodiscard]] bool operator==(const Unit& other) const noexcept;
  [[nodiscard]] bool operator!=(const Unit& other) const noexcept;

private:
  UnitSpec spec_;
  MemberHash hash_;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

/// Hash for the UnitSpec
template <>
struct impl::hash<UnitSpec>
{
  u32 operator()(const UnitSpec& spec) const noexcept
  {
    return hashCombine(hashSeed, spec.category, spec.name, spec.namePlural, spec.abbreviation, spec.f, spec.x, spec.y);
  }
};

}  // namespace sen

#endif  // SEN_CORE_META_UNIT_H
