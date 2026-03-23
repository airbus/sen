// === package_manager.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_KERNEL_PACKAGE_MANAGER_H
#define SEN_KERNEL_PACKAGE_MANAGER_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/meta/class_type.h"
#include "stl/sen/kernel/basic_types.stl.h"

// stl
#include <memory>
#include <string>
#include <vector>

namespace sen::kernel
{

/// Utility class for loading types from Sen packages.
class PackageManager
{
  SEN_NOCOPY_NOMOVE(PackageManager)

public:
  /// @param reg  Type registry that receives the types declared in each imported package.
  explicit PackageManager(CustomTypeRegistry& reg);
  ~PackageManager();

public:
  /// Imports the named packages, registering their types into the registry.
  /// @param packageNames  Names of the Sen packages to import (e.g. `"sen.geometry"`).
  /// @throws std::exception if any package cannot be found or loaded.
  void import(const std::vector<std::string>& packageNames);

  /// Returns the type registry containing all types loaded so far.
  /// @return Const reference to the internal `CustomTypeRegistry`; valid for the lifetime of this object.
  [[nodiscard]] const CustomTypeRegistry& getImportedTypes() const noexcept;

  /// Looks up a named type across all imported packages.
  /// @param typeName  Fully-qualified type name to search for.
  /// @return Non-owning pointer to the `Type`, or `nullptr` if not found.
  [[nodiscard]] const Type* lookupType(const std::string& typeName);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace sen::kernel

#endif  // SEN_KERNEL_PACKAGE_MANAGER_H
