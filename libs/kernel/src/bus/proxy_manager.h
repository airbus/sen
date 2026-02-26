// === proxy_manager.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_BUS_PROXY_MANAGER_H
#define SEN_LIBS_KERNEL_SRC_BUS_PROXY_MANAGER_H

#include "./participant.h"

// sen
#include "sen/core/obj/detail/native_object_proxy.h"
#include "sen/core/obj/object_filter.h"
#include "sen/core/obj/object_provider.h"

// concurrentqueue
#include "moodycamel/concurrentqueue.h"

// std
#include <mutex>

namespace sen::kernel::impl
{

class LocalParticipant;

/// For a given interest, this class holds proxies of objects that live in
/// other participants by registering as an ObjectProviderListener to them.
class ProxyManager final: public ObjectProviderListener, public ObjectProvider
{
public:
  SEN_NOCOPY_NOMOVE(ProxyManager)

public:
  ProxyManager(LocalParticipant& owner,
               std::shared_ptr<Interest> interest,
               ::sen::impl::WorkQueue& workQueue,
               std::string proxiesPrefix);
  ~ProxyManager() override;

public:
  /// The originating interest
  [[nodiscard]] const Interest& getLocalInterest() const noexcept { return *interest_; }

  /// Update the state of current proxies and notify listeners about new and deleted objects
  void notifyChangesToLocalListeners();

public:
  [[nodiscard]] LocalParticipant* isLocalParticipant() noexcept override { return &owner_; }

public:  // actions towards participants
  void startListeningToParticipant(Participant* participant, bool notifyAboutExisting);
  void stopListeningToParticipant(Participant* participant, bool notifyAboutExisting);

public:  // implements ObjectProvider (public methods)
  void addListener(ObjectProviderListener* listener, bool notifyAboutExistingObjects) override;
  void removeListener(ObjectProviderListener* listener, bool notifyAboutExistingObjects) override;
  [[nodiscard]] bool hasListener(ObjectProviderListener* listener) const noexcept override;
  [[nodiscard]] bool hasListeners() const noexcept override;

protected:  // implements ObjectProvider (protected methods)
  void notifyAddedOnExistingObjects(ObjectProviderListener* listener) override;
  void notifyRemovedOnExistingObjects(ObjectProviderListener* listener) override;
  void notifyObjectsAdded(const ObjectAdditionList& additions) override;
  void notifyObjectsRemoved(const ObjectRemovalList& removals) override;

protected:  // implements ObjectProviderListener
  void onObjectsAdded(const ObjectAdditionList& additions) override;
  void onObjectsRemoved(const ObjectRemovalList& removals) override;

private:
  [[nodiscard]] std::shared_ptr<::sen::impl::ProxyObject> createProxy(const ObjectAddition& discovery);
  void processPendingActions();

private:
  using Lock = std::scoped_lock<std::recursive_mutex>;
  using Action = std::variant<ObjectAddition, ObjectRemoval>;

private:
  LocalParticipant& owner_;
  std::shared_ptr<Interest> interest_;
  ::sen::impl::WorkQueue& workQueue_;
  std::string proxiesPrefix_;
  std::vector<ObjectRemoval> recentlyDeletedObjects_;
  std::vector<ObjectAddition> recentlyAddedObjects_;
  std::unordered_map<ObjectId, ObjectAddition> presentObjects_;
  std::unordered_map<ObjectId, std::shared_ptr<::sen::impl::ProxyObject>> presentProxiesList_;
  std::unordered_map<ObjectId, ObjectAddition> additionsToDelete_;
  mutable std::recursive_mutex listenersMutex_;
  moodycamel::ConcurrentQueue<Action> pendingActions_;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline void ProxyManager::addListener(ObjectProviderListener* listener, bool notifyAboutExistingObjects)
{
  // protect the call with a mutex
  Lock listenersLock(listenersMutex_);
  ObjectProvider::addListener(listener, notifyAboutExistingObjects);
}

inline void ProxyManager::removeListener(ObjectProviderListener* listener, bool notifyAboutExistingObjects)
{
  // protect the call with a mutex
  Lock listenersLock(listenersMutex_);
  ObjectProvider::removeListener(listener, notifyAboutExistingObjects);
}

inline void ProxyManager::notifyObjectsAdded(const ObjectAdditionList& additions)
{
  // protect the call with a mutex
  Lock listenersLock(listenersMutex_);
  ObjectProvider::notifyObjectsAdded(additions);
}

inline void ProxyManager::notifyObjectsRemoved(const ObjectRemovalList& removals)
{
  // protect the call with a mutex
  Lock listenersLock(listenersMutex_);
  ObjectProvider::notifyObjectsRemoved(removals);
}

inline bool ProxyManager::hasListener(ObjectProviderListener* listener) const noexcept
{
  Lock listenersLock(listenersMutex_);
  return ObjectProvider::hasListener(listener);
}

inline bool ProxyManager::hasListeners() const noexcept
{
  Lock listenersLock(listenersMutex_);
  return ObjectProvider::hasListeners();
}

inline void ProxyManager::startListeningToParticipant(Participant* participant, bool notifyAboutExisting)
{
  auto& otherFilter = participant->getIncomingInterestManager();

  // start listening to the participant
  otherFilter.addSubscriber(interest_, this, notifyAboutExisting);
}

inline void ProxyManager::stopListeningToParticipant(Participant* participant, bool notifyAboutExisting)
{
  // remove ourselves as subscribers
  auto& otherFilter = participant->getIncomingInterestManager();

  otherFilter.removeSubscriber(interest_, this, notifyAboutExisting);
}

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_BUS_PROXY_MANAGER_H
