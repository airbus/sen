// === object_provider.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/obj/object_provider.h"

// sen
#include "sen/core/obj/detail/proxy_object.h"

// xenium
#include <xenium/harris_michael_list_based_set.hpp>
#include <xenium/policy.hpp>
#include <xenium/reclamation/generic_epoch_based.hpp>

// std
#include <algorithm>
#include <functional>
#include <memory>
#include <tuple>
#include <vector>

namespace sen
{

//--------------------------------------------------------------------------------------------------------------
// ObjectProviderListener
//--------------------------------------------------------------------------------------------------------------

/// Lock-free Harris-Michael list using epoch-based reclamation.
/// Inherits directly from xenium to eliminate wrapper boilerplate within the Pimpl idiom.
struct ObjectProviderListener::ConcurrentProviderList
  : xenium::harris_michael_list_based_set<ObjectProvider*,
                                          xenium::policy::reclaimer<xenium::reclamation::epoch_based<>>,
                                          xenium::policy::compare<std::less<ObjectProvider*>>>
{
};

ObjectProviderListener::ObjectProviderListener(): providers_(std::make_unique<ConcurrentProviderList>()) {}

ObjectProviderListener::ObjectProviderListener(ObjectProviderListener&& other) noexcept = default;

ObjectProviderListener& ObjectProviderListener::operator=(ObjectProviderListener&& other) noexcept = default;

ObjectProviderListener::~ObjectProviderListener()
{
  // guard against moved-from state where unique_ptr ownership was transferred
  if (!providers_)
  {
    return;
  }

  for (auto* provider: *providers_)
  {
    if (provider != nullptr)
    {
      provider->listenerDeleted(this);
    }
  }
}

void ObjectProviderListener::addProvider(ObjectProvider* provider) { providers_->emplace(provider); }

void ObjectProviderListener::removeProvider(ObjectProvider* provider) { providers_->erase(provider); }

kernel::impl::RemoteParticipant* ObjectProviderListener::isRemoteParticipant() noexcept { return nullptr; }

kernel::impl::LocalParticipant* ObjectProviderListener::isLocalParticipant() noexcept { return nullptr; }

//--------------------------------------------------------------------------------------------------------------
// ObjectProvider
//--------------------------------------------------------------------------------------------------------------

/// Lock-free Harris-Michael list using epoch-based reclamation.
/// Inherits directly from xenium to eliminate wrapper boilerplate within the Pimpl idiom.
struct ObjectProvider::ConcurrentListenerList
  : xenium::harris_michael_list_based_set<ObjectProviderListener*,
                                          xenium::policy::reclaimer<xenium::reclamation::epoch_based<>>,
                                          xenium::policy::compare<std::less<ObjectProviderListener*>>>
{
};

ObjectProvider::ObjectProvider(): listeners_(std::make_unique<ConcurrentListenerList>()) {}

ObjectProvider::~ObjectProvider()
{
  for (auto* listener: *listeners_)
  {
    if (listener != nullptr)
    {
      listener->removeProvider(this);
    }
  }
}

void ObjectProvider::notifyRemovedOnExistingObjectsForAllListeners()
{
  for (auto* listener: *listeners_)
  {
    notifyRemovedOnExistingObjects(listener);
  }
}

void ObjectProvider::replaceListener(ObjectProviderListener* oldListener, ObjectProviderListener* newListener)
{
  listeners_->erase(oldListener);
  listeners_->emplace(newListener);
  // NOTE: the links to the providers in the listeners are NOT updated in this method (the old listener cannot be
  // modified after moving it in the Subscription move constructor)
}

bool ObjectProvider::hasListener(ObjectProviderListener* listener) const noexcept
{
  return listeners_->contains(listener);
}

bool ObjectProvider::hasListeners() const noexcept { return listeners_->begin() != listeners_->end(); }

void ObjectProvider::addListener(ObjectProviderListener* listener, bool notifyAboutExistingObjects)
{
  if (listeners_->emplace(listener))
  {
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
  if (listeners_->erase(listener))
  {
    listener->removeProvider(this);

    if (notifyAboutExistingObjects)
    {
      notifyRemovedOnExistingObjects(listener);
    }

    listenerRemoved(listener, notifyAboutExistingObjects);
  }
}

void ObjectProvider::listenerDeleted(ObjectProviderListener* listener) { listeners_->erase(listener); }

void ObjectProvider::notifyObjectsAdded(const ObjectAdditionList& additions)
{
  for (auto* listener: *listeners_)
  {
    callOnObjectsAdded(listener, additions);
  }
}

void ObjectProvider::notifyObjectsRemoved(const ObjectRemovalList& removals)
{
  for (auto* listener: *listeners_)
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

}  // namespace sen
