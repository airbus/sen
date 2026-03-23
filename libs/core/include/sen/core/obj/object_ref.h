// === object_ref.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_OBJECT_REF_H
#define SEN_CORE_OBJ_OBJECT_REF_H

// sen
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object_provider.h"

namespace sen
{

/// \addtogroup obj
/// @{

/// Holds a type-safe pointer to at most one discovered object instance.
/// The reference is populated automatically by an `ObjectProvider` once an object
/// matching the configured `Interest` is discovered, and cleared on removal.
///
/// Methods of this class are thread-safe.
/// @tparam T  Sen class type of the expected object; use `Object` to accept any type.
template <typename T>
class ObjectRef final: public ObjectProviderListener
{
public:  // types
  using Callback = std::function<void()>;

public:
  SEN_NOCOPY_NOMOVE(ObjectRef)

public:  // special members
  ObjectRef() = default;
  ~ObjectRef() override = default;

public:
  /// Returns `true` if the reference currently holds a valid object pointer.
  /// @return `true` when an instance has been discovered and not yet removed.
  [[nodiscard]] bool valid() const noexcept { return ptr_.load() != nullptr; }

  /// Implicit boolean conversion â€” equivalent to `valid()`.
  /// @return `true` when the reference is non-null.
  [[nodiscard]] operator bool() const noexcept { return valid(); }  // NOLINT(hicpp-explicit-conversions)

  /// Provides pointer-style access to the held object instance.
  /// @return Non-owning pointer to the instance; undefined behaviour if `!valid()`.
  [[nodiscard]] T* operator->() noexcept { return ptr_.load(); }

  /// Provides const pointer-style access to the held object instance.
  /// @return Non-owning const pointer to the instance; undefined behaviour if `!valid()`.
  [[nodiscard]] const T* operator->() const noexcept { return ptr_.load(); }

  /// Returns a mutable reference to the held object instance.
  /// @return Reference to the instance; undefined behaviour if `!valid()`.
  [[nodiscard]] T& get() noexcept { return *ptr_.load(); }

  /// Returns a const reference to the held object instance.
  /// @return Const reference to the instance; undefined behaviour if `!valid()`.
  [[nodiscard]] const T& get() const noexcept { return *ptr_.load(); }

  /// Installs a callback invoked each time an object is discovered and assigned to this reference.
  /// Replaces any previously-installed callback.
  /// @param function  Callable invoked (with no arguments) when a new object is assigned.
  /// @return The previously-installed callback, or an empty function if none was set.
  Callback onAdded(Callback&& function) noexcept { return std::exchange(onAdded_, std::move(function)); }

  /// Installs a callback invoked each time the held object is removed from the provider.
  /// Replaces any previously-installed callback.
  /// @param function  Callable invoked (with no arguments) when the object is cleared.
  /// @return The previously-installed callback, or an empty function if none was set.
  Callback onRemoved(Callback&& function) noexcept { return std::exchange(onRemoved_, std::move(function)); }

protected:  // implements ObjectProviderListener
  void onObjectsAdded(const ObjectAdditionList& additions) override;
  void onObjectsRemoved(const ObjectRemovalList& removals) override;

private:
  [[nodiscard]] T* getCastedObject(Object* object);

private:
  std::atomic<T*> ptr_ = nullptr;
  Callback onAdded_ = nullptr;
  Callback onRemoved_ = nullptr;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <typename T>
inline T* ObjectRef<T>::getCastedObject(Object* object)
{
  if constexpr (!std::is_same_v<T, Object>)
  {
    auto castedObject = dynamic_cast<T*>(object);
    if (!castedObject)
    {
      throwRuntimeError("invalid cast");
    }
    return castedObject;
  }
  else
  {
    return object;
  }
}

template <typename T>
inline void ObjectRef<T>::onObjectsAdded(const ObjectAdditionList& additions)
{
  for (const auto& addition: additions)
  {
    // only react to discovered instances
    auto* instance = getObjectInstance(addition);
    if (instance == nullptr)
    {
      continue;
    }

    // that are of our type
    if (auto* castedObject = getCastedObject(instance); castedObject)
    {
      ptr_.store(castedObject);

      if (onAdded_)
      {
        onAdded_();
      }
      return;
    }
  }
}

template <typename T>
inline void ObjectRef<T>::onObjectsRemoved(const ObjectRemovalList& removals)
{
  if (ptr_.load() != nullptr)
  {
    const auto* instance = ptr_.load()->asObject();
    for (const auto& removal: removals)
    {
      if (instance->getId() == removal.objectid)
      {
        ptr_.store(nullptr);

        if (onRemoved_)
        {
          onRemoved_();
        }
        return;
      }
    }
  }
}

}  // namespace sen

#endif  // SEN_CORE_OBJ_OBJECT_REF_H
