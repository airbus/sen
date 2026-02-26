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
  explicit PackageManager(CustomTypeRegistry& reg);
  ~PackageManager();

public:
  /// Imports a set of packages. Throws std::exception on failure.
  void import(const std::vector<std::string>& packageNames);

  /// The types loaded so far.
  [[nodiscard]] const CustomTypeRegistry& getImportedTypes() const noexcept;

  /// Look for a manually-defined type inside the imported packages.
  [[nodiscard]] const Type* lookupType(const std::string& typeName);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace sen::kernel

#endif  // SEN_KERNEL_PACKAGE_MANAGER_H
