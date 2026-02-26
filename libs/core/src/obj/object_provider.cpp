// === object_provider.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/object_provider.h"

// sen
#include "sen/core/obj/detail/proxy_object.h"

// std
#include <algorithm>
#include <tuple>
#include <vector>

namespace sen
{

//--------------------------------------------------------------------------------------------------------------
// ObjectProviderListener
//--------------------------------------------------------------------------------------------------------------

ObjectProviderListener::~ObjectProviderListener()
{
  auto providers = providers_;  // explicit copy to prevent modifications
  for (auto* provider: providers)
  {
    provider->listenerDeleted(this);
  }
}

void ObjectProviderListener::addProvider(ObjectProvider* provider)
{
  auto itr = std::find(providers_.begin(), providers_.end(), provider);
  if (itr == providers_.end())
  {
    providers_.push_back(provider);
  }
}

void ObjectProviderListener::removeProvider(ObjectProvider* provider)
{
  auto itr = std::find(providers_.begin(), providers_.end(), provider);
  if (itr != providers_.end())
  {
    providers_.erase(itr);
  }
}

sen::kernel::impl::RemoteParticipant* ObjectProviderListener::isRemoteParticipant() noexcept { return nullptr; }

sen::kernel::impl::LocalParticipant* ObjectProviderListener::isLocalParticipant() noexcept { return nullptr; }

//--------------------------------------------------------------------------------------------------------------
// ObjectProvider
//--------------------------------------------------------------------------------------------------------------

ObjectProvider::~ObjectProvider()
{
  // explicit copy to prevent modifications
  auto listeners = listeners_;

  for (auto* listener: listeners)
  {
    listener->removeProvider(this);
  }
}

void ObjectProvider::notifyRemovedOnExistingObjectsForAllListeners()
{
  // explicit copy to prevent modifications
  auto listeners = listeners_;

  for (auto* listener: listeners)
  {
    notifyRemovedOnExistingObjects(listener);
  }
}

bool ObjectProvider::hasListener(ObjectProviderListener* listener) const noexcept
{
  return std::find(listeners_.begin(), listeners_.end(), listener) != listeners_.end();
}

bool ObjectProvider::hasListeners() const noexcept { return !listeners_.empty(); }

void ObjectProvider::addListener(ObjectProviderListener* listener, bool notifyAboutExistingObjects)
{
  auto itr = std::find(listeners_.begin(), listeners_.end(), listener);
  if (itr == listeners_.end())
  {
    listeners_.push_back(listener);
    listener->addProvider(this);

    if (notifyAboutExistingObjects)
    {
      notifyAddedOnExistingObjects(listener);
    }

    listenerAdded(listener, notifyAboutExistingObjects);
  }
}

void ObjectProvider::removeListener(ObjectProviderListener* listener, bool notifyAboutExistingObjects)
{
  auto itr = std::find(listeners_.begin(), listeners_.end(), listener);
  if (itr != listeners_.end())
  {
    if (notifyAboutExistingObjects)
    {
      notifyRemovedOnExistingObjects(listener);
    }

    (*itr)->removeProvider(this);
    listeners_.erase(itr);

    listenerRemoved(listener, notifyAboutExistingObjects);
  }
}

void ObjectProvider::listenerDeleted(ObjectProviderListener* listener)
{
  listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), listener), listeners_.end());
}

void ObjectProvider::notifyObjectsAdded(const ObjectAdditionList& additions)
{
  // explicit copy to prevent modifications
  auto listeners = listeners_;
  for (auto* listener: listeners)
  {
    callOnObjectsAdded(listener, additions);
  }
}

void ObjectProvider::notifyObjectsRemoved(const ObjectRemovalList& removals)
{
  // explicit copy to prevent modifications
  auto listeners = listeners_;
  for (auto* listener: listeners)
  {
    callOnObjectsRemoved(listener, removals);
  }
}

void ObjectProvider::listenerAdded(ObjectProviderListener* listener, bool notifyAboutExistingObjects)
{
  std::ignore = listener;
  std::ignore = notifyAboutExistingObjects;
}

void ObjectProvider::listenerRemoved(ObjectProviderListener* listener, bool notifyAboutExistingObjects)  // NOSONAR
{
  std::ignore = listener;
  std::ignore = notifyAboutExistingObjects;
}

void ObjectProvider::callOnObjectsAdded(ObjectProviderListener* listener, const ObjectAdditionList& additions) const
{
  listener->onObjectsAdded(additions);
}

void ObjectProvider::callOnObjectsRemoved(ObjectProviderListener* listener, const ObjectRemovalList& removals) const
{
  listener->onObjectsRemoved(removals);
}

const std::vector<ObjectProviderListener*>& ObjectProvider::getListeners() const noexcept { return listeners_; }

}  // namespace sen
