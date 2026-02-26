// === type_registry.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_TYPE_REGISTRY_H
#define SEN_CORE_META_TYPE_REGISTRY_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/obj/native_object.h"

// std
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sen
{

/// \addtogroup type_utils
/// @{

/// A registry of custom types.
class CustomTypeRegistry
{
  SEN_NOCOPY_NOMOVE(CustomTypeRegistry)

public:
  using CustomTypeMap = std::unordered_map<std::string, ConstTypeHandle<CustomType>>;  ///< Key is the qualified name
  using CustomTypeHashMap = std::unordered_map<uint32_t, const CustomType*>;           ///< Key is the hash
  using CustomTypeList = std::vector<const CustomType*>;

public:
  CustomTypeRegistry() = default;
  ~CustomTypeRegistry() = default;

public:
  /// Adds a type and any dependent type to the registry.
  /// This method is thread-safe.
  void add(ConstTypeHandle<> type);

  /// Adds a class maker to the registry.
  void addInstanceMaker(const ClassType& type, InstanceMakerFunc maker);

  /// Searches for a type with a given qualified name. May return
  /// nullptr if there is no type found with such name.
  /// This method is thread-safe.
  [[nodiscard]] std::optional<ConstTypeHandle<>> get(const std::string& qualifiedName) const;

  /// Searches for a type with a given hash. May return
  /// nullptr if there is no type found with such hash.
  /// This method is thread-safe.
  [[nodiscard]] std::optional<ConstTypeHandle<>> get(uint32_t hash) const;

  /// Gets all the registered types.
  /// This method is thread-safe.
  [[nodiscard]] CustomTypeMap getAll() const;

  /// Gets all the registered types ordered from independent o dependent.
  [[nodiscard]] CustomTypeList getAllInOrder() const;

  /// Creates a local object.
  [[nodiscard]] std::shared_ptr<NativeObject> makeInstance(const ClassType* type,
                                                           const std::string& name,
                                                           const VarMap& properties) const;

private:
  void uncheckedAdd(ConstTypeHandle<CustomType> type);

private:
  CustomTypeMap types_;
  CustomTypeHashMap typesByHash_;
  CustomTypeList typesInDependencyOrder_;
  std::unordered_map<std::string, InstanceMakerFunc> instanceMakers_;
  mutable std::recursive_mutex usageMutex_;
};

/// Helper that iterates over dependent types.
/// The callback function is invoked for all used sub-types recursively.
void iterateOverDependentTypes(const Type& type, std::function<void(ConstTypeHandle<> type)>& callback);

/// @}

}  // namespace sen

#endif  // SEN_CORE_META_TYPE_REGISTRY_H
