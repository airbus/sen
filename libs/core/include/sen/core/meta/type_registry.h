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

/// Thread-safe registry that maps qualified type names and hashes to `CustomType` instances.
/// Built during STL resolution and shared across the kernel for serialisation, reflection,
/// and dynamic object instantiation.
class CustomTypeRegistry
{
  SEN_NOCOPY_NOMOVE(CustomTypeRegistry)

public:
  using CustomTypeMap = std::unordered_map<std::string, ConstTypeHandle<CustomType>>;  ///< Key is the qualified name.
  using CustomTypeHashMap = std::unordered_map<uint32_t, const CustomType*>;           ///< Key is the type hash.
  using CustomTypeList = std::vector<const CustomType*>;

public:
  CustomTypeRegistry() = default;
  ~CustomTypeRegistry() = default;

public:
  /// Registers a type and all of its transitive dependencies.
  /// If a type with the same qualified name is already registered, it is silently skipped.
  /// Thread-safe.
  /// @param type The type (and implicitly its dependencies) to register.
  void add(ConstTypeHandle<> type);

  /// Registers a factory function that can create instances of the given class type.
  /// @param type   The class type whose instances the factory creates.
  /// @param maker  Callable that constructs a new instance given a name and property map.
  void addInstanceMaker(const ClassType& type, InstanceMakerFunc maker);

  /// Looks up a type by its fully-qualified name (e.g. `"sen.geometry.Point"`).
  /// Thread-safe.
  /// @param qualifiedName Dot-separated package and type name.
  /// @return Handle to the type, or `std::nullopt` if not registered.
  [[nodiscard]] std::optional<ConstTypeHandle<>> get(const std::string& qualifiedName) const;

  /// Looks up a type by its compile-time hash.
  /// Thread-safe.
  /// @param hash 32-bit hash as returned by `Type::getHash()`.
  /// @return Handle to the type, or `std::nullopt` if not registered.
  [[nodiscard]] std::optional<ConstTypeHandle<>> get(uint32_t hash) const;

  /// Returns a snapshot copy of all registered types keyed by qualified name.
  /// Thread-safe.
  /// @return Map from qualified name to type handle.
  [[nodiscard]] CustomTypeMap getAll() const;

  /// Returns all registered types in dependency order (independent types first).
  /// Useful for code generation or serialisation schemas that must declare base types before derived types.
  /// @return List of raw type pointers in topologically sorted order.
  [[nodiscard]] CustomTypeList getAllInOrder() const;

  /// Constructs a new `NativeObject` instance of the given class type.
  /// Requires a factory to have been registered via `addInstanceMaker()`.
  /// @param type       Descriptor of the class to instantiate.
  /// @param name       Instance name assigned to the new object.
  /// @param properties Initial property values for the new object.
  /// @return Shared pointer to the newly constructed object.
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

/// Traverses all transitive dependencies of `type` and invokes `callback` for each one.
/// The traversal is depth-first; a type is visited once regardless of how many times it appears.
/// @param type     Root type whose dependency graph should be walked.
/// @param callback Called with a handle to each discovered dependent type.
void iterateOverDependentTypes(const Type& type, std::function<void(ConstTypeHandle<> type)>& callback);

/// @}

}  // namespace sen

#endif  // SEN_CORE_META_TYPE_REGISTRY_H
