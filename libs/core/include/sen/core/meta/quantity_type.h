// === quantity_type.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_QUANTITY_TYPE_H
#define SEN_CORE_META_QUANTITY_TYPE_H

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/unit.h"

// std
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace sen
{

/// \addtogroup types
/// @{

/// Data of a quantity type.
struct QuantitySpec final
{
  QuantitySpec(std::string name,
               std::string qualifiedName,
               std::string description,
               ConstTypeHandle<NumericType> elementType,
               std::optional<const Unit*> unit = std::nullopt,
               std::optional<float64_t> minValue = std::nullopt,
               std::optional<float64_t> maxValue = std::nullopt)
    : name(std::move(name))
    , qualifiedName(std::move(qualifiedName))
    , description(std::move(description))
    , elementType(std::move(elementType))
    , unit(unit)
    , minValue(minValue)
    , maxValue(maxValue)
  {
    SEN_ASSERT(!unit || unit.value() != nullptr);
  }

  // Comparison operators.
  friend bool operator==(const QuantitySpec& lhs, const QuantitySpec& rhs) noexcept
  {
    return lhs.description == rhs.description &&
           (lhs.unit ? (rhs.unit && *lhs.unit == *rhs.unit) : rhs.unit == nullptr) &&
           lhs.elementType == rhs.elementType && lhs.maxValue == rhs.maxValue && lhs.minValue == rhs.minValue;
  }

  friend bool operator!=(const QuantitySpec& lhs, const QuantitySpec& rhs) noexcept { return !(lhs == rhs); }

public:
  std::string name;                          // NOLINT(misc-non-private-member-variables-in-classes)
  std::string qualifiedName;                 // NOLINT(misc-non-private-member-variables-in-classes)
  std::string description;                   // NOLINT(misc-non-private-member-variables-in-classes)
  ConstTypeHandle<NumericType> elementType;  // NOLINT(misc-non-private-member-variables-in-classes)
  std::optional<const Unit*> unit;           // NOLINT(misc-non-private-member-variables-in-classes)
  std::optional<float64_t> minValue;         // NOLINT(misc-non-private-member-variables-in-classes)
  std::optional<float64_t> maxValue;         // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Represents a quantity.
class QuantityType: public CustomType
{
  SEN_META_TYPE(QuantityType)
  SEN_PRIVATE_TAG

public:  // special members
  /// Copies the spec into a member.
  QuantityType(QuantitySpec spec, Private notUsable) noexcept;
  ~QuantityType() override = default;

public:
  /// Factory function that validates the spec and creates a quantity type.
  /// Throws std::exception if the spec is not valid.
  [[nodiscard]] static TypeHandle<QuantityType> make(QuantitySpec spec);

  /// Gets the type of the numeric value.
  [[nodiscard]] ConstTypeHandle<NumericType> getElementType() const noexcept;

  /// Gets the unit of the measurement.
  [[nodiscard]] std::optional<const Unit*> getUnit() const noexcept;

  /// Gets the unit of the measurement, failing if no unit was there.
  [[nodiscard]] const Unit& getUnit(sen::Unit::EnsureTag) const;

  /// The maximum value, if any.
  [[nodiscard]] std::optional<float64_t> getMaxValue() const noexcept;

  /// The minimum value, if any.
  [[nodiscard]] std::optional<float64_t> getMinValue() const noexcept;

public:  // Type
  [[nodiscard]] std::string_view getQualifiedName() const noexcept final;
  [[nodiscard]] std::string_view getName() const noexcept final;
  [[nodiscard]] std::string_view getDescription() const noexcept final;
  [[nodiscard]] bool equals(const Type& other) const noexcept override;
  [[nodiscard]] bool isBounded() const noexcept final;

protected:
  /// The spec of this unit.
  [[nodiscard]] const QuantitySpec& getSpec() const noexcept;

  /// To allow subclasses to instantiate.
  explicit QuantityType(QuantitySpec spec) noexcept;

private:
  QuantitySpec spec_;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

/// Hash for the QuantitySpec
template <>
struct impl::hash<QuantitySpec>
{
  u32 operator()(const QuantitySpec& spec) const noexcept
  {
    return hashCombine(hashSeed,
                       spec.name,
                       spec.qualifiedName,
                       spec.elementType->getHash(),
                       spec.unit ? spec.unit.value()->getHash() : nonPresentTypeHash,
                       spec.minValue.has_value() ? spec.minValue.value() : nonPresentTypeHash,
                       spec.maxValue.has_value() ? spec.maxValue.value() : nonPresentTypeHash);
  }
};

}  // namespace sen

#endif  // SEN_CORE_META_QUANTITY_TYPE_H
