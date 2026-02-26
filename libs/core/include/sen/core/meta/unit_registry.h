// === unit_registry.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_UNIT_REGISTRY_H
#define SEN_CORE_META_UNIT_REGISTRY_H

// sen
#include "sen/core/base/class_helpers.h"  // for SEN_NOCOPY_NOMOVE
#include "sen/core/base/iterator_adapters.h"
#include "sen/core/meta/unit.h"  // for Unit, UnitCategory

// std
#include <memory>
#include <optional>
#include <string_view>  // for string_view
#include <vector>       // for vector

namespace sen
{

/// \addtogroup type_utils
/// @{

/// A registry of units.
class UnitRegistry final
{
  SEN_NOCOPY_NOMOVE(UnitRegistry)

public:
  using UnitList = std::vector<Unit*>;
  using UnitStorage = std::vector<std::unique_ptr<Unit>>;
  using UnitRange = sen::util::IteratorRange<util::SmartPtrIteratorAdapter<UnitStorage::const_iterator>>;

public:
  static const UnitRegistry& get();

public:
  /// The currently registered units.
  /// Does *not* include the builtin units.
  [[nodiscard]] UnitRange getUnits() const noexcept;

  /// Returns all registered units that belong to a given category.
  /// It also searches in the built-in units.
  [[nodiscard]] UnitList getUnitsByCategory(UnitCategory category) const;

  /// Tries to find a unit with a specific name (case sensitive)
  /// It also searches in the built-in units.
  /// Returns nullopt if the unit was not found, otherwise a valid pointer to the unit.
  [[nodiscard]] std::optional<const Unit*> searchUnitByName(std::string_view name) const;

  /// Tries to find a unit with a specific abbreviation (case sensitive).
  /// It also searches in the built-in units.
  /// Returns nullopt if the unit was not found, otherwise a valid pointer to the unit.
  [[nodiscard]] std::optional<const Unit*> searchUnitByAbbreviation(std::string_view abbrev) const;

private:
  UnitRegistry();
  ~UnitRegistry() = default;

private:
  UnitStorage units_;
};

/// @}

}  // namespace sen

#endif  // SEN_CORE_META_UNIT_REGISTRY_H
