// === object_source.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_OBJECT_SOURCE_H
#define SEN_CORE_OBJ_OBJECT_SOURCE_H

// sen
#include "sen/core/base/span.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_filter.h"
#include "sen/core/obj/object_provider.h"

// std
#include <mutex>
#include <vector>

namespace sen::kernel::impl
{
class Runner;
}  // namespace sen::kernel::impl

namespace sen
{

/// \addtogroup obj
/// @{

/// Bus-scoped registry of live `NativeObject` instances.
///
/// An `ObjectSource` is the entry point for publishing objects onto a bus and for
/// subscribing to objects published by others (via the inherited `ObjectFilter`).
/// Obtain one through `KernelApi::getSource()`.
///
/// Typical usage inside a component:
/// @code{.cpp}
/// auto source = api.getSource("session.bus");
/// source->add(std::make_shared<MyObject>("my_object"));
/// // ... later, when shutting down:
/// source->remove(myObjectPtr);
/// @endcode
class ObjectSource: public ObjectFilter
{
public:
  SEN_NOCOPY_NOMOVE(ObjectSource)

public:
  using NativeObjectPtr = std::shared_ptr<NativeObject>;

public:
  explicit ObjectSource(const ObjectOwnerId& id): ObjectFilter(id) {}
  ~ObjectSource() override = default;

public:
  /// Publishes a batch of objects onto the bus, registering their types if not already known.
  /// Objects that are already registered are silently skipped.
  /// Thread-safe.
  /// @param instances Span of shared pointers to the objects to publish.
  /// @return `true` if all objects were added successfully, `false` if any failed.
  virtual bool add(const Span<NativeObjectPtr>& instances) = 0;

  /// Publishes a single object onto the bus.
  /// Thread-safe.
  /// @param instance Shared pointer to the object to publish.
  /// @return `true` on success, `false` if the object could not be added.
  bool add(NativeObjectPtr instance) { return add(makeSpan<NativeObjectPtr>(instance)); }

  /// Publishes a typed vector of objects onto the bus.
  /// @tparam T Concrete `NativeObject` subclass.
  /// @param instances Vector of shared pointers to publish.
  /// @return `true` if all objects were added successfully.
  template <typename T>
  bool add(const std::vector<std::shared_ptr<T>>& instances)
  {
    std::vector<NativeObjectPtr> vec(instances.begin(), instances.end());
    return add(makeSpan(vec));
  }

  /// Un-publishes a batch of objects from the bus and notifies all subscribers.
  /// Objects that are not currently registered are silently ignored.
  /// Thread-safe.
  /// @param instances Span of shared pointers to the objects to remove.
  virtual void remove(const Span<NativeObjectPtr>& instances) = 0;

  /// Un-publishes a single object from the bus.
  /// Thread-safe.
  /// @param instance Shared pointer to the object to remove.
  void remove(NativeObjectPtr instance) { remove(makeSpan<NativeObjectPtr>(instance)); }

  /// Un-publishes a typed vector of objects from the bus.
  /// @tparam T Concrete `NativeObject` subclass.
  /// @param instances Vector of shared pointers to remove.
  template <typename T>
  void remove(const std::vector<std::shared_ptr<T>>& instances)
  {
    std::vector<NativeObjectPtr> vec(instances.begin(), instances.end());
    remove(makeSpan(vec));
  }

protected:
  friend class kernel::impl::Runner;
  virtual void flushOutputs() = 0;
  virtual void drainInputs() = 0;
};

/// @}

}  // namespace sen

#endif  // SEN_CORE_OBJ_OBJECT_SOURCE_H
