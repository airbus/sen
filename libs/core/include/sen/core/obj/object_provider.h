// === object_provider.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_OBJ_OBJECT_PROVIDER_H
#define SEN_CORE_OBJ_OBJECT_PROVIDER_H

// sen
#include "sen/core/meta/type.h"
#include "sen/core/obj/detail/proxy_object.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"

// std
#include <variant>

namespace sen
{

/// \addtogroup obj
/// @{

// Forward declarations
class ObjectProvider;

/// Identifies the owner of an object
SEN_STRONG_TYPE(ObjectOwnerId, uint32_t)

template <>
struct ShouldBePassedByValue<ObjectOwnerId>: std::true_type
{
};

/// Holds information about an object that has been discovered.
struct ObjectInstanceDiscovery
{
  std::shared_ptr<Object> instance;
  ObjectId id;
  InterestId interestId;
  ObjectOwnerId ownerId;
};

/// Holds information about a remote object that has been discovered.
struct RemoteObjectDiscovery
{
  ObjectId id;
  std::string name;
  ConstTypeHandle<ClassType> type;
  std::function<std::shared_ptr<impl::ProxyObject>(impl::WorkQueue*, const std::string&, ObjectOwnerId)> proxyMaker;
  InterestId interestId;
  ObjectOwnerId ownerId;
};

/// Holds information about an object that is
/// already present and we explicitly asked for.
using ObjectAddition = std::variant<ObjectInstanceDiscovery, RemoteObjectDiscovery>;

/// Holds information about an object that has been removed.
struct ObjectRemoval
{
  InterestId interestId;
  ObjectId objectid;
  ObjectOwnerId ownerId;
};

/// Sequence of object additions.
using ObjectAdditionList = std::vector<ObjectAddition>;

/// Sequence of object removals.
using ObjectRemovalList = std::vector<ObjectRemoval>;

// Helpers to extract information out of the previous containers
[[nodiscard]] inline ObjectId getObjectId(const ObjectAddition& discovery)
{
  return std::holds_alternative<ObjectInstanceDiscovery>(discovery) ? std::get<ObjectInstanceDiscovery>(discovery).id
                                                                    : std::get<RemoteObjectDiscovery>(discovery).id;
}

[[nodiscard]] inline ObjectOwnerId getObjectOwnerId(const ObjectAddition& discovery)
{
  return std::holds_alternative<ObjectInstanceDiscovery>(discovery)
           ? std::get<ObjectInstanceDiscovery>(discovery).ownerId
           : std::get<RemoteObjectDiscovery>(discovery).ownerId;
}

[[nodiscard]] inline InterestId getInterestId(const ObjectAddition& discovery)
{
  return std::holds_alternative<ObjectInstanceDiscovery>(discovery)
           ? std::get<ObjectInstanceDiscovery>(discovery).interestId
           : std::get<RemoteObjectDiscovery>(discovery).interestId;
}

[[nodiscard]] inline Object* getObjectInstance(const ObjectAddition& discovery)
{
  return std::holds_alternative<ObjectInstanceDiscovery>(discovery)
           ? std::get<ObjectInstanceDiscovery>(discovery).instance.get()
           : nullptr;
}

[[nodiscard]] ObjectRemoval makeRemoval(const ObjectAddition& discovery);

/// Allows reacting to objects being added or removed to an object provider.
/// It automatically unregisters itself from all the providers upon destruction.
class ObjectProviderListener
{
  SEN_MOVE_ONLY(ObjectProviderListener)

public:
  ObjectProviderListener() = default;
  virtual ~ObjectProviderListener();

public:  // implementation & optimization-related (they return nullptr by default)
  [[nodiscard]] virtual kernel::impl::RemoteParticipant* isRemoteParticipant() noexcept;
  [[nodiscard]] virtual kernel::impl::LocalParticipant* isLocalParticipant() noexcept;

protected:
  /// Called when objects are been added to a source.
  virtual void onObjectsAdded(const ObjectAdditionList& additions) = 0;

  /// Called when objects will be removed from a source.
  virtual void onObjectsRemoved(const ObjectRemovalList& removals) = 0;

private:
  friend class ObjectProvider;
  void addProvider(ObjectProvider* provider);
  void removeProvider(ObjectProvider* provider);

private:
  std::vector<ObjectProvider*> providers_;
};

/// Base class for an entity that is able to produce objects.
class ObjectProvider: public std::enable_shared_from_this<ObjectProvider>
{
  SEN_NOCOPY_NOMOVE(ObjectProvider)

public:
  ObjectProvider() = default;
  virtual ~ObjectProvider();

public:
  /// Registers an event listener.
  ///
  /// The listener will be notified of objects being added and removed, and
  /// this provider will pass itself as the source of the objects.
  ///
  /// A listener can be only added once to the same provider. So calling his method with the same
  /// listener will do nothing.
  ///
  /// This provider will *not* take ownership of the added listener. But when a listener
  /// gets destroyed, it will ensure that it removes itself from the providers it was added to.
  /// Note that in this case your listener will *not* be notified about the removal of existing
  /// objects.
  ///
  /// @param listener the entity that will be notified about objects.
  /// @param notifyAboutExistingObjects if true, the listener will get an immediate 'onObjectAdded'
  ///                                   call for each existing object in the provider.
  virtual void addListener(ObjectProviderListener* listener, bool notifyAboutExistingObjects);

  /// Un-registers an event listener.
  ///
  /// Does nothing if the provided event listener was not added.
  ///
  /// @param listener the listener that was previously added.
  /// @param notifyAboutExistingObjects if true, the listener will get a 'onObjectRemoved' call
  ///                                   for each existing object in the provider.
  virtual void removeListener(ObjectProviderListener* listener, bool notifyAboutExistingObjects);

  /// Returns true if the listener has been added.
  [[nodiscard]] virtual bool hasListener(ObjectProviderListener* listener) const noexcept;

  /// Returns true if there are registered listeners.
  [[nodiscard]] virtual bool hasListeners() const noexcept;

protected:
  friend class ObjectFilter;

  /// Calls onObjectAdded on all the registered listeners.
  virtual void notifyObjectsAdded(const ObjectAdditionList& additions);

  /// Calls onObjectRemoved on all the registered listeners.
  virtual void notifyObjectsRemoved(const ObjectRemovalList& removals);

  /// Does nothing by default.
  virtual void listenerAdded(ObjectProviderListener* listener, bool notifyAboutExistingObjects);

  /// Does nothing by default.
  virtual void listenerRemoved(ObjectProviderListener* listener, bool notifyAboutExistingObjects);

protected:
  /// Subclasses must call onObjectAdded on the listener for all the existing objects.
  virtual void notifyAddedOnExistingObjects(ObjectProviderListener* listener) = 0;

  /// Subclasses must call onObjectRemoved on the listener for all the existing objects.
  virtual void notifyRemovedOnExistingObjects(ObjectProviderListener* listener) = 0;

  /// Same as notifyRemovedOnExistingObjects but for all registered listeners.
  void notifyRemovedOnExistingObjectsForAllListeners();

protected:  // helpers for subclasses
  void callOnObjectsAdded(ObjectProviderListener* listener, const ObjectAdditionList& additions) const;
  void callOnObjectsRemoved(ObjectProviderListener* listener, const ObjectRemovalList& removals) const;
  [[nodiscard]] const std::vector<ObjectProviderListener*>& getListeners() const noexcept;

private:
  friend ObjectProviderListener;
  void listenerDeleted(ObjectProviderListener* listener);

private:
  std::vector<ObjectProviderListener*> listeners_;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline ObjectRemoval makeRemoval(const ObjectAddition& discovery)
{
  if (std::holds_alternative<ObjectInstanceDiscovery>(discovery))
  {
    const auto& instanceDiscovery = std::get<ObjectInstanceDiscovery>(discovery);
    return {instanceDiscovery.interestId, instanceDiscovery.id, instanceDiscovery.ownerId};
  }

  const auto& remoteObjectDiscovery = std::get<RemoteObjectDiscovery>(discovery);
  return {remoteObjectDiscovery.interestId, remoteObjectDiscovery.id, remoteObjectDiscovery.ownerId};
}

}  // namespace sen

SEN_STRONG_TYPE_HASHABLE(::sen::ObjectOwnerId)

#endif  // SEN_CORE_OBJ_OBJECT_PROVIDER_H
