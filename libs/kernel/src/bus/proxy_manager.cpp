// === proxy_manager.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "proxy_manager.h"

#include "local_participant.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/obj/detail/proxy_object.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object_provider.h"

// std
#include <cstddef>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace sen::kernel::impl
{

namespace
{

using ProxyPtr = std::shared_ptr<::sen::impl::ProxyObject>;

class ProxyMaker final
{
  SEN_NOCOPY_NOMOVE(ProxyMaker)

public:
  explicit ProxyMaker(::sen::impl::WorkQueue* queue, const std::string& localPrefix, ObjectOwnerId ownerId)
    : queue_(queue), localPrefix_(localPrefix), ownerId_(ownerId)
  {
  }

  ~ProxyMaker() = default;

public:
  [[nodiscard]] ProxyPtr operator()(const ObjectInstanceDiscovery& instanceDiscovery) const
  {
    if (auto* local = instanceDiscovery.instance->asNativeObject())
    {
      auto proxy = instanceDiscovery.instance->getClass()->createLocalProxy(local, localPrefix_);
      proxy->drainInputs();

      return {proxy};
    }
    throw std::logic_error("remote object instance received by a proxy manager");
  }

  [[nodiscard]] ProxyPtr operator()(const RemoteObjectDiscovery& remoteDiscovery) const
  {
    return remoteDiscovery.proxyMaker(queue_, localPrefix_, ownerId_);
  }

private:
  ::sen::impl::WorkQueue* queue_;
  const std::string& localPrefix_;
  ObjectOwnerId ownerId_;
};

}  // namespace

ProxyManager::ProxyManager(LocalParticipant& owner,
                           std::shared_ptr<Interest> interest,
                           ::sen::impl::WorkQueue& workQueue,
                           std::string proxiesPrefix)
  : owner_(owner), interest_(std::move(interest)), workQueue_(workQueue), proxiesPrefix_(std::move(proxiesPrefix))
{
}

ProxyManager::~ProxyManager()
{
  Lock listenersLock(listenersMutex_);

  recentlyDeletedObjects_.clear();
  recentlyAddedObjects_.clear();
  presentObjects_.clear();
  presentProxiesList_.clear();
  additionsToDelete_.clear();
}

std::shared_ptr<::sen::impl::ProxyObject> ProxyManager::createProxy(const ObjectAddition& discovery)
{
  ProxyMaker maker(&workQueue_, proxiesPrefix_, owner_.getId());
  return std::visit(maker, discovery);
}

void ProxyManager::notifyChangesToLocalListeners()
{
  Lock listenersLock(listenersMutex_);

  // drainInputs the buffers of the remote objects owned by this view
  for (const auto& [first, second]: presentProxiesList_)
  {
    const std::lock_guard lock(*second);
    second->drainInputs();
  }

  processPendingActions();
}

void ProxyManager::processPendingActions()
{
  std::vector<Action> actions;
  actions.resize(pendingActions_.size_approx());

  if (actions.empty())
  {
    return;
  }

  recentlyAddedObjects_.clear();
  recentlyAddedObjects_.reserve(actions.size());

  recentlyDeletedObjects_.clear();
  recentlyDeletedObjects_.reserve(actions.size());

  auto count = pendingActions_.try_dequeue_bulk(actions.begin(), actions.size());

  for (std::size_t i = 0U; i < count; ++i)
  {
    std::visit(
      Overloaded {
        [this](const ObjectAddition& addition)
        {
          const auto id = getObjectId(addition);

          // do not add repeated objects
          if (presentProxiesList_.find(id) == presentProxiesList_.end())
          {
            // create and store the proxy
            if (const auto proxy = createProxy(addition); proxy != nullptr)
            {
              auto [proxyItr, done] = presentProxiesList_.try_emplace(id, std::move(proxy));

              ObjectAddition discovery {ObjectInstanceDiscovery {
                proxyItr->second->shared_from_this(), id, interest_->getId(), getObjectOwnerId(addition)}};

              // add it to presentObjects_
              presentObjects_.try_emplace(id, discovery);

              // mark it as recently added for later notification
              recentlyAddedObjects_.push_back(discovery);

              // remove it from the proxies to delete, if present
              additionsToDelete_.erase(id);
            }
          }
        },
        [this](const ObjectRemoval& removal)
        {
          // try to find a proxy, and remove it if present
          if (auto listItr = presentProxiesList_.find(removal.objectid); listItr != presentProxiesList_.end())
          {
            if (auto* remoteProxy = listItr->second->asRemoteObject(); remoteProxy != nullptr)
            {
              remoteProxy->invalidateTransport();
            }

            presentProxiesList_.erase(listItr);
          }

          // remove it from presentObjects_
          auto presentObjectsItr = presentObjects_.find(removal.objectid);
          if (presentObjectsItr != presentObjects_.end())
          {
            additionsToDelete_.insert(*presentObjectsItr);
            presentObjects_.erase(presentObjectsItr);

            // add it to recentlyDeleted for later notification
            recentlyDeletedObjects_.push_back(removal);
          }
        },
      },
      actions[i]);
  }

  // notify additions
  if (!recentlyAddedObjects_.empty())
  {
    notifyObjectsAdded(recentlyAddedObjects_);
  }

  // notify removals
  if (!recentlyDeletedObjects_.empty())
  {
    notifyObjectsRemoved(recentlyDeletedObjects_);
  }

  // we can delete unused proxies now
  additionsToDelete_.clear();
}

void ProxyManager::onObjectsAdded(const ObjectAdditionList& additions)
{
  for (const auto& addition: additions)
  {
    pendingActions_.enqueue(addition);
  }
}

void ProxyManager::onObjectsRemoved(const ObjectRemovalList& removals)
{
  for (const auto& removal: removals)
  {
    pendingActions_.enqueue(removal);
  }
}

void ProxyManager::notifyAddedOnExistingObjects(ObjectProviderListener* listener)
{
  if (!presentObjects_.empty())
  {
    std::vector<ObjectAddition> additions;
    additions.reserve(presentObjects_.size());

    for (const auto& [first, second]: presentObjects_)
    {
      additions.push_back(second);
    }

    callOnObjectsAdded(listener, additions);
  }
}

void ProxyManager::notifyRemovedOnExistingObjects(ObjectProviderListener* listener)
{
  if (!presentObjects_.empty())
  {
    std::vector<ObjectRemoval> removals;
    removals.reserve(presentObjects_.size());

    for (const auto& [first, second]: presentObjects_)
    {
      removals.push_back(makeRemoval(second));
    }

    callOnObjectsRemoved(listener, removals);
  }
}

}  // namespace sen::kernel::impl
