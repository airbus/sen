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

/// Allows adding and receiving objects.
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
  /// Registers an object (including its type).
  /// It does nothing if the object is already registered.
  /// Returns false if one or more objects could not be added.
  /// This method is thread-safe.
  virtual bool add(const Span<NativeObjectPtr>& instances) = 0;

  /// Registers an object (including its type).
  /// It does nothing if the objects are already present.
  /// Returns false if the objects could not be added.
  /// This method is thread-safe.
  bool add(NativeObjectPtr instance) { return add(makeSpan<NativeObjectPtr>(instance)); }

  /// Convenience helper.
  template <typename T>
  bool add(const std::vector<std::shared_ptr<T>>& instances)
  {
    std::vector<NativeObjectPtr> vec(instances.begin(), instances.end());
    return add(makeSpan(vec));
  }

  /// Unregisters an object and notifies any registered listeners.
  /// Ignores objects that are not present.
  /// This method is thread-safe.
  virtual void remove(const Span<NativeObjectPtr>& instances) = 0;

  /// Unregisters an object.
  /// Does nothing if the object is not present.
  /// This method is thread-safe.
  void remove(NativeObjectPtr instance) { remove(makeSpan<NativeObjectPtr>(instance)); }

  /// Convenience helper.
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
