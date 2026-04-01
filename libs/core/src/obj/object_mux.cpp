// === object_mux.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/object_mux.h"

// sen
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_provider.h"

// std
#include <algorithm>
#include <tuple>
#include <vector>

namespace sen
{

MuxedProviderListener::~MuxedProviderListener()
{
  auto muxedProviders = muxedProviders_;  // explicit copy to prevent modifications
  for (auto* provider: muxedProviders)
  {
    provider->muxedListenerDeleted(this);
  }
}

void MuxedProviderListener::addMuxedProvider(ObjectMux* provider)
{
  if (std::find(muxedProviders_.begin(), muxedProviders_.end(), provider) == muxedProviders_.end())
  {
    muxedProviders_.push_back(provider);
  }
}

void MuxedProviderListener::removeMuxedProvider(ObjectMux* provider)
{
  if (auto itr = std::find(muxedProviders_.begin(), muxedProviders_.end(), provider); itr != muxedProviders_.end())
  {
    muxedProviders_.erase(itr);
  }
}

void ObjectMux::addListener(ObjectProviderListener* listener, bool notifyAboutExistingObjects)
{
  // protect the call with a mutex
  Lock listenersLock(listenersMutex_);
  ObjectProvider::addListener(listener, notifyAboutExistingObjects);
}

void ObjectMux::removeListener(ObjectProviderListener* listener, bool notifyAboutExistingObjects)
{
  // protect the call with a mutex
  Lock listenersLock(listenersMutex_);
  ObjectProvider::removeListener(listener, notifyAboutExistingObjects);
}

void ObjectMux::muxedListenerDeleted(MuxedProviderListener* listener)
{
  muxedListeners_.erase(std::remove(muxedListeners_.begin(), muxedListeners_.end(), listener), muxedListeners_.end());
}

void ObjectMux::addMuxedListener(MuxedProviderListener* listener, bool notifyAboutExistingObjects)
{
  // protect the call with a mutex
  Lock listenersLock(listenersMutex_);
  if (std::find(muxedListeners_.begin(), muxedListeners_.end(), listener) == muxedListeners_.end())
  {
    muxedListeners_.push_back(listener);
    listener->addMuxedProvider(this);

    if (notifyAboutExistingObjects)
    {
      notifyMuxedAddedOnExistingObjects(listener);
    }
  }
}

void ObjectMux::removeMuxedListener(MuxedProviderListener* listener, bool notifyAboutExistingObjects)
{
  // protect the call with a mutex
  Lock listenersLock(listenersMutex_);
  if (auto itr = std::find(muxedListeners_.begin(), muxedListeners_.end(), listener); itr != muxedListeners_.end())
  {
    if (notifyAboutExistingObjects)
    {
      notifyMuxedRemovedOnExistingObjects(listener);
    }

    (*itr)->removeMuxedProvider(this);
    muxedListeners_.erase(itr);
  }
}

void ObjectMux::notifyAddedOnExistingObjects(ObjectProviderListener* listener)
{
  Lock objectsLock(objectsMutex_);

  if (!presentObjects_.empty())
  {
    std::vector<ObjectAddition> additions;
    additions.reserve(presentObjects_.size());

    for (const auto& [id, addition]: presentObjects_)
    {
      additions.push_back(addition[0]);
    }

    callOnObjectsAdded(listener, additions);
  }
}

void ObjectMux::notifyRemovedOnExistingObjects(ObjectProviderListener* listener)
{
  Lock objectsLock(objectsMutex_);

  if (!presentObjects_.empty())
  {
    std::vector<ObjectRemoval> removals;
    removals.reserve(presentObjects_.size());

    for (const auto& [id, addition]: presentObjects_)
    {
      removals.push_back(makeRemoval(addition[0]));
    }

    callOnObjectsRemoved(listener, removals);
  }
}

void ObjectMux::notifyObjectsAdded(const ObjectAdditionList& additions)
{
  // protect the call with a mutex
  Lock listenersLock(listenersMutex_);
  ObjectProvider::notifyObjectsAdded(additions);
  for (auto* listener: muxedListeners_)
  {
    callOnObjectsAdded(listener, additions);
  }
}

void ObjectMux::notifyObjectsRemoved(const ObjectRemovalList& removals)
{
  // protect the call with a mutex
  Lock listenersLock(listenersMutex_);
  ObjectProvider::notifyObjectsRemoved(removals);
  for (auto* listener: muxedListeners_)
  {
    callOnObjectsRemoved(listener, removals);
  }
}

void ObjectMux::notifyMuxedAddedOnExistingObjects(MuxedProviderListener* listener)
{
  Lock objectsLock(objectsMutex_);

  if (!presentObjects_.empty())
  {
    std::vector<ObjectAddition> additions;
    additions.reserve(presentObjects_.size());

    std::vector<ObjectAddition> readdedObjects;
    readdedObjects.reserve(presentObjects_.size());  // only a rough estimate

    for (const auto& [id, additionsForObject]: presentObjects_)
    {
      additions.push_back(additionsForObject[0]);

      // signal on all "duplicate" additions
      if (additionsForObject.size() == 1)
      {
        continue;
      }
      for (auto itr = additionsForObject.begin() + 1; itr != additionsForObject.end(); ++itr)
      {
        readdedObjects.push_back(*itr);
      }
    }

    callOnObjectsAdded(listener, additions);
    callOnExistingObjectsReadded(listener, readdedObjects);
  }
}

void ObjectMux::notifyMuxedRemovedOnExistingObjects(MuxedProviderListener* listener)
{
  Lock objectsLock(objectsMutex_);

  if (!presentObjects_.empty())
  {
    std::vector<ObjectRemoval> removals;
    removals.reserve(presentObjects_.size());

    std::vector<ObjectRemoval> additionalRemovals;
    additionalRemovals.reserve(presentObjects_.size());

    for (const auto& [id, additionsForObject]: presentObjects_)
    {
      removals.push_back(makeRemoval(additionsForObject[0]));

      // signal on all refcount reductions
      if (additionsForObject.size() == 1)
      {
        continue;
      }
      for (auto revItr = additionsForObject.rbegin(); revItr != additionsForObject.rend() - 1; ++revItr)
      {
        additionalRemovals.push_back(makeRemoval(*revItr));
      }
    }

    callOnObjectsRefCountReduced(listener, additionalRemovals);
    callOnObjectsRemoved(listener, removals);
  }
}

void ObjectMux::notifyExistingObjectsReadded(const ObjectAdditionList& additions)
{
  // protect the call with a mutex
  Lock listenersLock(listenersMutex_);
  for (auto* listener: muxedListeners_)
  {
    callOnExistingObjectsReadded(listener, additions);
  }
}

void ObjectMux::notifyObjectsRefCountReduced(const ObjectRemovalList& removals)
{
  // protect the call with a mutex
  Lock listenersLock(listenersMutex_);
  for (auto* listener: muxedListeners_)
  {
    callOnObjectsRefCountReduced(listener, removals);
  }
}

bool ObjectMux::hasListener(ObjectProviderListener* listener) const noexcept
{
  Lock listenersLock(listenersMutex_);
  return ObjectProvider::hasListener(listener);
}

bool ObjectMux::hasMuxedListener(MuxedProviderListener* listener) const noexcept
{
  Lock listenersLock(listenersMutex_);
  return std::find(muxedListeners_.begin(), muxedListeners_.end(), listener) != muxedListeners_.end();
}

bool ObjectMux::hasListeners() const noexcept
{
  Lock listenersLock(listenersMutex_);
  return ObjectProvider::hasListeners() || !muxedListeners_.empty();
}

void ObjectMux::callOnExistingObjectsReadded(MuxedProviderListener* listener, const ObjectAdditionList& additions) const
{
  listener->onExistingObjectsReadded(additions);
}

void ObjectMux::callOnObjectsRefCountReduced(MuxedProviderListener* listener, const ObjectRemovalList& removals) const
{
  listener->onObjectsRefCountReduced(removals);
}

void ObjectMux::onObjectsAdded(const ObjectAdditionList& additions)
{
  if (additions.empty())
  {
    return;
  }

  ObjectAdditionList newAdditions;
  ObjectAdditionList duplicateAdditions;

  {
    Lock objectsLock(objectsMutex_);

    for (const auto& addition: additions)
    {
      ObjectAdditionList& additionListForObject = presentObjects_[getObjectId(addition)];
      additionListForObject.push_back(addition);
      if (additionListForObject.size() == 1)
      {
        newAdditions.push_back(addition);
        continue;
      }
      // signal for "duplicate" addition
      duplicateAdditions.push_back(addition);
    }
  }

  if (!newAdditions.empty())
  {
    notifyObjectsAdded(newAdditions);
  }
  if (!duplicateAdditions.empty())
  {
    notifyExistingObjectsReadded(duplicateAdditions);
  }
}

void ObjectMux::onObjectsRemoved(const ObjectRemovalList& removals)
{
  if (removals.empty())
  {
    return;
  }

  // removals for objects that are not matched by another interest
  ObjectRemovalList actualRemovals;
  ObjectRemovalList duplicateRemovals;

  {
    Lock objectsLock(objectsMutex_);

    for (const auto& removal: removals)
    {
      ObjectId objectId = removal.objectid;
      ObjectAdditionList& additionListForObject = presentObjects_[objectId];

      auto itr = std::find_if(
        additionListForObject.begin(),
        additionListForObject.end(),
        [&removal](const ObjectAddition& addition)
        { return removal.objectid == getObjectId(addition) && removal.interestId == getInterestId(addition); });

      if (itr != additionListForObject.end())
      {
        additionListForObject.erase(itr);
        if (additionListForObject.empty())
        {
          presentObjects_.erase(removal.objectid);
          actualRemovals.push_back(removal);
          continue;
        }

        // signal for refcount reduced
        duplicateRemovals.push_back(removal);
      }
    }
  }

  if (!duplicateRemovals.empty())
  {
    notifyObjectsRefCountReduced(duplicateRemovals);
  }
  if (!actualRemovals.empty())
  {
    notifyObjectsRemoved(actualRemovals);
  }
}

}  // namespace sen
