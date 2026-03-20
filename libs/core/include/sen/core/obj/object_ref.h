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

/// Object references may or may not hold an object instance. Their
/// value is set by a provider based on some defined
/// interest which was given during construction.
///
/// Methods of this class are thread-safe.
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
  /// True if the reference holds a valid pointer to an instance.
  [[nodiscard]] bool valid() const noexcept { return ptr_.load() != nullptr; }

  /// True if the reference holds a valid pointer to an instance.
  [[nodiscard]] operator bool() const noexcept { return valid(); }  // NOLINT(hicpp-explicit-conversions)

  /// Access the internal instance.
  [[nodiscard]] T* operator->() noexcept { return ptr_.load(); }

  /// Access the internal instance.
  [[nodiscard]] const T* operator->() const noexcept { return ptr_.load(); }

  /// Access the internal instance.
  [[nodiscard]] T& get() noexcept { return *ptr_.load(); }

  /// Access the internal instance.
  [[nodiscard]] const T& get() const noexcept { return *ptr_.load(); }

  /// Installs a function to be called when objects are added / discovered.
  /// Replaces any previously-installed function.
  Callback onAdded(Callback&& function) noexcept { return std::exchange(onAdded_, std::move(function)); }

  /// Installs a function to be called when objects are added / discovered.
  /// Replaces any previously-installed function.
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
    const Object* instance;
    if constexpr (!std::is_same_v<T, Object>)
    {
      instance = &ptr_.load()->asObject();
    }
    else
    {
      instance = ptr_.load();
    }
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
