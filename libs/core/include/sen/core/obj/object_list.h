// === object_list.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_OBJECT_LIST_H
#define SEN_CORE_OBJ_OBJECT_LIST_H

// sen
#include "sen/core/base/assert.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/obj/object_provider.h"

// std
#include <list>

namespace sen
{

/// \addtogroup obj
/// @{

/// A list of objects that is managed by some
/// source based on user-expressed interests.
template <typename T>
class ObjectList final: public ObjectProviderListener
{
public:
  SEN_MOVE_ONLY(ObjectList)

  static constexpr std::size_t defaultListSizeHint = 10U;

public:  // types
  /// Controls whether objects whose class is a subclass of `T` are also accepted.
  enum class SearchMode
  {
    includeSubClasses,  ///< Accept instances of `T` and any subclass of `T`.
    ignoreSubClasses,   ///< Accept only instances whose exact class is `T`.
  };

  using TypedObjectList = std::list<T*>;
  using UntypedObjectList = std::list<std::shared_ptr<Object>>;

  struct Iterators
  {
    typename TypedObjectList::iterator typedBegin;
    typename TypedObjectList::iterator typedEnd;
    typename UntypedObjectList::iterator untypedBegin;
    typename UntypedObjectList::iterator untypedEnd;
  };

  using Callback = std::function<void(const Iterators& iterators)>;

public:  // special members
  /// Constructs a standalone list with pre-reserved internal storage.
  /// @param sizeHint Expected number of objects; pre-allocates the iterator map to avoid early rehashing.
  explicit ObjectList(std::size_t sizeHint = defaultListSizeHint) { iteratorMap_.reserve(sizeHint); }

  /// Constructs a list and immediately subscribes to a provider.
  /// @param provider  The object provider to listen to (existing objects are reported immediately).
  /// @param sizeHint  Expected number of objects; pre-allocates the iterator map.
  ObjectList(ObjectProvider& provider, std::size_t sizeHint): ObjectList(sizeHint) { provider.addListener(this, true); }

  ~ObjectList() override = default;

public:
  /// Installs a callback invoked whenever objects are added or newly discovered.
  /// The callback fires during `drainInputs()` with iterators spanning the newly-added objects.
  /// If objects are already present when the callback is installed, it is called immediately.
  /// @param function New callback; replaces any previously-installed one.
  /// @return The previously-installed callback (may be `nullptr`).
  [[nodiscard]] Callback onAdded(Callback&& function) noexcept;

  /// Installs a callback invoked whenever objects are removed.
  /// The callback fires during `drainInputs()` with iterators spanning the removed objects.
  /// @param function New callback; replaces any previously-installed one.
  /// @return The previously-installed callback (may be `nullptr`).
  [[nodiscard]] Callback onRemoved(Callback&& function) noexcept
  {
    return std::exchange(onRemoved_, std::move(function));
  }

public:  // multiple object lookup
  /// @return List of raw typed pointers to all currently-tracked objects.
  [[nodiscard]] const std::list<T*>& getObjects() const noexcept { return typedObjects_; }

  /// @return List of type-erased shared pointers to all currently-tracked objects.
  [[nodiscard]] const std::list<std::shared_ptr<Object>>& getUntypedObjects() const noexcept { return untypedObjects_; }

protected:  // implements ObjectProviderListener
  void onObjectsAdded(const ObjectAdditionList& additions) override;
  void onObjectsRemoved(const ObjectRemovalList& removals) override;

private:
  [[nodiscard]] T* getCastedObject(Object* object);

private:
  struct IteratorPair
  {
    typename TypedObjectList::const_iterator typed;
    typename UntypedObjectList::const_iterator untyped;
  };

private:
  TypedObjectList typedObjects_;
  UntypedObjectList untypedObjects_;
  std::unordered_map<ObjectId, IteratorPair> iteratorMap_;
  Callback onAdded_ = nullptr;
  Callback onRemoved_ = nullptr;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <typename T>
inline typename ObjectList<T>::Callback ObjectList<T>::onAdded(Callback&& function) noexcept
{
  auto previous = std::exchange(onAdded_, std::move(function));

  if (onAdded_ && !untypedObjects_.empty())
  {
    onAdded_({typedObjects_.begin(), typedObjects_.end(), untypedObjects_.begin(), untypedObjects_.end()});
  }

  return previous;
}

template <typename T>
inline T* ObjectList<T>::getCastedObject(Object* object)
{
  if constexpr (!std::is_same_v<T, Object>)
  {
    if (object == nullptr)
    {
      throwRuntimeError("Attempted to cast a null sen::Object pointer");
    }

    auto* castedObject = dynamic_cast<T*>(object);
    if (castedObject == nullptr)
    {
      std::string err;
      err.append("error casting object named '");
      err.append(object->getName());
      err.append("' of class '");
      err.append(object->getClass()->getQualifiedName());
      err.append("' to a native type '");
      err.append(typeid(T).name());
      err.append("' within an ObjectList");
      throwRuntimeError(err);
    }

    return castedObject;
  }
  else
  {
    return object;
  }
}

template <typename T>
inline void ObjectList<T>::onObjectsAdded(const ObjectAdditionList& additions)
{
  Iterators iterators;
  bool added = false;

  for (const auto& addition: additions)
  {
    // only react to discovered instances
    auto* instance = getObjectInstance(addition);
    if (instance == nullptr)
    {
      continue;
    }

    auto castedObject = getCastedObject(instance);
    auto lastTyped = typedObjects_.insert(typedObjects_.end(), castedObject);
    auto lastUntyped = untypedObjects_.insert(untypedObjects_.end(), instance->shared_from_this());

    if (!added)
    {
      iterators.untypedBegin = lastUntyped;
      iterators.typedBegin = lastTyped;
      added = true;
    }

    // store the object
    iteratorMap_.insert({instance->getId(), {lastTyped, lastUntyped}});
  }

  if (!added)
  {
    return;
  }

  if (onAdded_)
  {
    iterators.typedEnd = typedObjects_.end();
    iterators.untypedEnd = untypedObjects_.end();
    onAdded_(iterators);
  }
}

template <typename T>
inline void ObjectList<T>::onObjectsRemoved(const ObjectRemovalList& removals)
{
  TypedObjectList typedRemovals;
  UntypedObjectList untypedRemovals;

  for (const auto& removal: removals)
  {
    auto itr = iteratorMap_.find(removal.objectid);
    if (itr != iteratorMap_.end())
    {
      auto castedObject = *(itr->second.typed);
      auto nonCastedObject = *(itr->second.untyped);

      untypedObjects_.erase(itr->second.untyped);
      typedObjects_.erase(itr->second.typed);
      iteratorMap_.erase(itr);

      if (onRemoved_)
      {
        typedRemovals.push_back(castedObject);
        untypedRemovals.push_back(nonCastedObject);
      }
    }
  }

  if (onRemoved_)
  {
    Iterators iterators;
    iterators.typedBegin = typedRemovals.begin();
    iterators.typedEnd = typedRemovals.end();
    iterators.untypedBegin = untypedRemovals.begin();
    iterators.untypedEnd = untypedRemovals.end();

    onRemoved_(iterators);
  }
}

}  // namespace sen

#endif  // SEN_CORE_OBJ_OBJECT_LIST_H
