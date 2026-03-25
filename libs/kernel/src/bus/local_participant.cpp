// === local_participant.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "local_participant.h"

// implementation
#include "bus_protocol.stl.h"
#include "kernel_impl.h"
#include "runner.h"

// bus
#include "bus/participant.h"
#include "bus/proxy_manager.h"
#include "bus/remote_participant.h"
#include "bus/util.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/iterator_adapters.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/obj/detail/event_buffer.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_provider.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/tracer.h"
#include "sen/kernel/transport.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// spdlog
#include <spdlog/logger.h>

// std
#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

using sen::util::makeLockedRange;

namespace sen::kernel::impl
{

LocalParticipant::LocalParticipant(ObjectOwnerId id,
                                   const BusAddress& busAddress,
                                   std::shared_ptr<impl::Session> session,
                                   impl::Runner* owner,
                                   ::sen::impl::WorkQueue& workQueue)
  : ObjectSource(id)
  , owner_(owner)
  , session_(std::move(session))
  , bus_(session_->getOrOpenBus(busAddress.busName).get())
  , id_(id)
  , remoteInterestsManager_(id_, this)
  , workQueue_(workQueue)
  , logger_(KernelImpl::getKernelLogger())
  , targetNewObjects_ {&localObjects_.newObjects}
{
  debugName_ = owner_->getComponentContext().info.name;
  debugName_.append(".");
  debugName_.append(getBusAddress().sessionName);
  debugName_.append(".");
  debugName_.append(getBusAddress().busName);

  logger_->debug("LP {}: created with id {}", debugName_, getId().get());
}

LocalParticipant::~LocalParticipant()
{
  bus_->disconnect(this);
  owner_->localParticipantDeleted(this);
}

BusId LocalParticipant::getBusId() const noexcept { return bus_->getId(); }

BusAddress LocalParticipant::getBusAddress() const noexcept { return bus_->getAddress(); }

bool LocalParticipant::add(const Span<std::shared_ptr<NativeObject>>& instances)
{
  Session::PublicLock sessionLock(*session_);

  bool result = true;

  targetNewObjects_->reserve(targetNewObjects_->size() + instances.size());
  for (const auto& instance: instances)
  {
    const auto id = instance->getId();

    instance->setLocalPrefix(objectsNamePrefix_);
    const auto& localName = instance->getLocalName();
    if (localObjectNames_.count(localName) != 0)
    {
      result = false;

      getLogger().error("Repeated object name '{}'. Sen doesn't allow name repetitions on the same bus.",
                        instance->getLocalName());

      // undo the changes to the object
      instance->setLocalPrefix({});
    }
    else
    {
      // store the object in the toAdd list, remove it from the toRemove list (if present)
      localObjectNames_.insert(localName);
      targetNewObjects_->try_emplace(id, instance);
      localObjects_.deletedObjects.erase(id);
    }
  }

  // this will call commit() on all the instances
  owner_->registerObjects(instances);

  return result;
}

[[nodiscard]] std::optional<TimeStamp> LocalParticipant::objectNameUsed(std::string_view name) const
{
  std::string localName = objectsNamePrefix_;
  localName.append(".").append(name);

  if (localObjectNames_.count(localName) != 0)
  {
    for (const auto& [id, obj]: localObjects_.currentObjects)
    {
      if (obj->getName() == name)
      {
        return obj->asNativeObject()->getRegistrationTime();
      }
    }
  }
  return std::nullopt;
}

void LocalParticipant::remove(const Span<std::shared_ptr<NativeObject>>& instances)
{
  Session::PublicLock sessionLock(*session_);

  for (const auto& instance: instances)
  {
    removeSingleObject(instance.get());
  }

  owner_->unregisterObjects(instances);
}

void LocalParticipant::removeSingleObject(Object* instance)
{

  // store the object in the toRemove list, remove it from the toAdd list (if present)
  const auto id = instance->getId();

  localObjects_.newObjects.erase(id);

  if (localObjects_.currentObjects.find(id) != localObjects_.currentObjects.end())
  {
    localObjects_.deletedObjects.insert(id);
  }

  localObjectNames_.erase(instance->getLocalName());
}

bool LocalParticipant::handleRejectedPublications(const PublicationRejection& rejections, RemoteParticipant* whom)
{

  std::vector<Object*> objectsToRemove;
  std::vector<Object*> objectsToIgnore;
  objectsToRemove.reserve(rejections.rejections.size());
  objectsToIgnore.reserve(rejections.rejections.size());

  for (const auto& rejection: rejections.rejections)
  {

    if (auto itr = localObjects_.currentObjects.find(rejection.objectId); itr != localObjects_.currentObjects.end())
    {
      std::visit(
        sen::Overloaded {
          [&](const RepeatedName& reason)
          {
            if (itr->second->asNativeObject()->getRegistrationTime() >= reason.myObjectRegistrationTime)
            {
              objectsToRemove.push_back(itr->second.get());
              logger_->error(
                "Remote participant {} rejected the publication of object {} due to name repetitions on the bus. Will "
                "unpublish the object. \033[1m\033[31m\033[5mObjects with this name are now invalid for this "
                "process!\033[m",
                whom->getAddress().id.get(),
                itr->second->getLocalName());
            }
            else
            {
              logger_->debug(
                "Remote participant {} rejected the publication of object {} due to name repetitions on the bus. Will "
                "not comply (local object is older)",
                whom->getAddress().id.get(),
                itr->second->getLocalName());
            }
          },
          [&](const RuntimeIncompatibleType& reason)
          {
            std::ignore = reason;
            objectsToIgnore.push_back(itr->second.get());
            logger_->error(
              "Remote participant {} rejected the publication of object {}, of type {}, because the local and remote "
              "definitions of this type are runtime-incompatible",
              whom->getAddress().id.get(),
              itr->second->getLocalName(),
              itr->second->getClass()->getName());
          },
          [&](const Timeout& reason)
          {
            std::ignore = reason;
            objectsToIgnore.push_back(itr->second.get());
            // TODO: there are false positives here when having two participants for the same interest
            // logger_->error(
            //   "Remote participant {} rejected the publication of object {}, of type {}, because the timeout for
            //   object " "resolution was reached.", whom->getAddress().id.get(), itr->second->getLocalName(),
            //   itr->second->getClass()->getName());
          }},
        rejection.reason);
    }
  }

  // return false in case there are not local objects affected by the rejection
  if (objectsToRemove.empty() && objectsToIgnore.empty())
  {
    return false;
  }

  for (auto* instance: objectsToRemove)
  {
    removeSingleObject(instance);
  }

  for (const auto* instance: objectsToIgnore)
  {
    remoteInterestsManager_.markObjectAsRejected(instance->getId(), whom);
  }

  return true;
}

void LocalParticipant::flushOutputs()
{
  SEN_TRACE_ZONE(owner_->getTracer());

  flushLocalObjectsState(owner_->getSerializableEvents().getContents());
  flushLocalAdditionsAndRemovals();
}

void LocalParticipant::drainInputs()
{
  // explicit copy to prevent modifications
  auto interestsOnOthers = interestsOnOthers_;
  for (const auto& proxyManager: interestsOnOthers)
  {
    if (proxyManager == nullptr)
    {
      continue;
    }

    proxyManager->notifyChangesToLocalListeners();
  }
}

void LocalParticipant::flushLocalAdditionsAndRemovals()
{

  // let local listeners know
  evaluateLocalObjects(localObjects_);

  // let other participants know
  remoteInterestsManager_.evaluate(localObjects_);

  // add new objects to the list of current objects
  for (auto& newObject: localObjects_.newObjects)
  {
    localObjects_.currentObjects.insert(std::move(newObject));
  }

  // we recover the objects that were created during the evaluation
  localObjects_.newObjects = tempNewObjects_;
  tempNewObjects_.clear();

  // remove deleted objects from the list of current objects
  for (auto& deletedObject: localObjects_.deletedObjects)
  {
    localObjects_.currentObjects.erase(deletedObject);
  }
  localObjects_.deletedObjects.clear();
}

void LocalParticipant::flushLocalObjectsState(const std::list<::sen::impl::SerializableEvent>& events)
{
  // send our state and events to the interested *remote* participants
  // note: for local participants we already use
  //       the local object queues
  remoteInterestsManager_.sendToRelevantRemoteParticipants(events);
}

void LocalParticipant::connect()
{
  Session::PublicLock sessionLock(*session_);

  const auto busAddress = getBusAddress();

  objectsNamePrefix_ = getOwner()->getComponentContext().info.name;
  objectsNamePrefix_.append(".");
  objectsNamePrefix_.append(busAddress.sessionName);
  objectsNamePrefix_.append(".");
  objectsNamePrefix_.append(busAddress.busName);

  bus_->connect(this);
}

void LocalParticipant::localSubscriberAdded(const std::shared_ptr<Interest>& interest,
                                            ObjectProviderListener* listener,
                                            bool notifyAboutExisting)
{
  if (const auto* localParticipant = listener->isLocalParticipant();
      localParticipant == nullptr && listener->isRemoteParticipant() == nullptr)
  {
    logger_->debug("LP {}: local subscriber (mux) added for interest (id {})", debugName_, interest->getId().get());
  }
  else if (localParticipant != nullptr)
  {
    logger_->debug("LP {}: local subscriber {} added for interest (id {})",
                   debugName_,
                   localParticipant->getDebugName(),
                   interest->getId().get());
  }

  // look if we already have a manager for this interest
  std::lock_guard lock(
    interestsOnOthersMutex_);  // needs to be a write lock to ensure we don't do double insertion afterward
  for (const auto& proxyManager: interestsOnOthers_)
  {
    if (proxyManager->getLocalInterest() == *interest)
    {
      proxyManager->addListener(listener, notifyAboutExisting);
      return;
    }
  }

  // we need to create a manager for this interest
  interestsOnOthers_.push_back(std::make_shared<ProxyManager>(*this, interest, workQueue_, objectsNamePrefix_));

  interestsOnOthers_.back()->addListener(listener, notifyAboutExisting);

  std::lock_guard participantLock(participantsMutex_);
  // make other participants notify us when it is the case
  for (auto* participant: participants_)
  {
    interestsOnOthers_.back()->startListeningToParticipant(participant, notifyAboutExisting);
  }
}

void LocalParticipant::localSubscriberRemoved(const std::shared_ptr<Interest>& interest,
                                              ObjectProviderListener* listener,
                                              bool notifyAboutExisting)
{
  if (const auto* localParticipant = listener->isLocalParticipant();
      localParticipant == nullptr && listener->isRemoteParticipant() == nullptr)
  {
    logger_->debug("LP {}: local subscriber (mux) removed from interest (id {})", debugName_, interest->getId().get());
  }
  else if (localParticipant != nullptr)
  {
    logger_->debug("LP {}: local subscriber {} removed from interest (id {})",
                   debugName_,
                   localParticipant->getDebugName(),
                   interest->getId().get());
  }

  for (auto itr = interestsOnOthers_.begin(); itr != interestsOnOthers_.end(); ++itr)
  {
    if (auto* proxyManager = (*itr).get(); proxyManager->getLocalInterest() == *interest)
    {
      proxyManager->removeListener(listener, notifyAboutExisting);

      // delete the manager if there are no listeners
      if (!proxyManager->hasListeners())
      {
        interestsOnOthers_.erase(itr);
      }
      return;
    }
  }

  throw std::logic_error("proxy manager not found in local participant");
}

void LocalParticipant::localSubscriberRemoved(ObjectProviderListener* listener, bool notifyAboutExisting)
{
  if (auto* localParticipant = listener->isLocalParticipant();
      localParticipant == nullptr && listener->isRemoteParticipant() == nullptr)
  {
    logger_->debug("LP {}: local subscriber (mux) removed", debugName_);
  }
  else if (localParticipant != nullptr)
  {
    logger_->debug("LP {}: local subscriber ({}, {}) removed",
                   debugName_,
                   localParticipant->debugName_,
                   localParticipant->getId().get());
  }

  std::vector<std::shared_ptr<ProxyManager>> remaining;

  for (auto& proxyManager: interestsOnOthers_)
  {
    proxyManager->removeListener(listener, notifyAboutExisting);

    if (proxyManager->hasListeners())
    {
      remaining.push_back(std::move(proxyManager));
    }
  }

  interestsOnOthers_ = std::move(remaining);
}

void LocalParticipant::remoteSubscriberAdded(const std::shared_ptr<Interest>& interest,
                                             RemoteParticipant* listener,
                                             bool notifyAboutExisting)
{
  logger_->debug("LP {}: remote subscriber {} added for interest {} begin '{}')",
                 debugName_,
                 listener->getId().get(),
                 interest->getId().get(),
                 interest->getQueryString().c_str());

  remoteInterestsManager_.addSubscriber(interest, listener, notifyAboutExisting);

  logger_->debug("LP {}: remote subscriber {} added for interest {} end",
                 debugName_,
                 listener->getId().get(),
                 interest->getId().get());
}

void LocalParticipant::remoteSubscriberRemoved(const std::shared_ptr<Interest>& interest,
                                               RemoteParticipant* listener,
                                               bool notifyAboutExisting)
{
  logger_->debug("LP {}: remote subscriber {} removed from interest {} begin",
                 debugName_,
                 listener->getId().get(),
                 interest->getId().get());

  remoteInterestsManager_.removeSubscriber(interest, listener, notifyAboutExisting);

  logger_->debug("LP {}: remote subscriber {} removed from interest {} end",
                 debugName_,
                 listener->getId().get(),
                 interest->getId().get());
}

void LocalParticipant::remoteSubscriberRemoved(RemoteParticipant* listener, bool notifyAboutExisting)
{
  logger_->debug("LP {}: remote subscriber {} removed begin", debugName_, listener->getId().get());

  remoteInterestsManager_.removeSubscriber(listener, notifyAboutExisting);

  logger_->debug("LP {}: remote subscriber {} removed end", debugName_, listener->getId().get());
}

void LocalParticipant::localParticipantAdded(LocalParticipant* other, bool notifyAboutExisting)
{
  logger_->debug("LP {}: local participant {} added", debugName_, other->debugName_);

  for (const auto& proxyManager: makeLockedRange<std::shared_lock>(interestsOnOthers_, interestsOnOthersMutex_))
  {
    proxyManager->startListeningToParticipant(other, notifyAboutExisting);
  }

  std::lock_guard participantLock(participantsMutex_);
  participants_.push_back(other);
}

void LocalParticipant::localParticipantRemoved(LocalParticipant* other, bool notifyAboutExisting)
{
  logger_->debug("LP {}: local participant {} removed", debugName_, other->debugName_);

  for (const auto& proxyManager: interestsOnOthers_)
  {
    proxyManager->stopListeningToParticipant(other, notifyAboutExisting);
  }

  std::lock_guard participantLock(participantsMutex_);
  removeIfPresent<Participant*>(other, participants_);
}

void LocalParticipant::remoteParticipantAdded(RemoteParticipant* other, bool notifyAboutExisting)
{
  for (const auto& proxyManager: interestsOnOthers_)
  {
    proxyManager->startListeningToParticipant(other, notifyAboutExisting);
  }

  std::lock_guard participantLock(participantsMutex_);
  participants_.push_back(other);

  // try to connect to the remote process (ensure that the messages from the remote participant will reach their
  // recipient)
  logger_->debug("LP: {} → remoteParticipantReady → RP {}", id_.get(), other->getId().get());
  other->sendRemoteParticipantReady(getId());
}

void LocalParticipant::remoteParticipantRemoved(RemoteParticipant* other, bool notifyAboutExisting)
{
  logger_->debug("LP {}: remote participant {} removed", debugName_, other->getId().get());

  for (const auto& proxyManager: interestsOnOthers_)
  {
    proxyManager->stopListeningToParticipant(other, notifyAboutExisting);
  }

  std::lock_guard participantLock(participantsMutex_);
  removeIfPresent<Participant*>(other, participants_);

  remoteInterestsManager_.removeSubscriber(other, notifyAboutExisting);
}

void LocalParticipant::subscriberAdded(std::shared_ptr<Interest> interest,
                                       ObjectProviderListener* listener,
                                       bool notifyAboutExisting)
{
  if (const auto& typeCondition = interest->getTypeCondition();
      std::holds_alternative<ConstTypeHandle<ClassType>>(typeCondition))
  {
    owner_->registerType(std::get<ConstTypeHandle<ClassType>>(typeCondition));
  }
  localSubscriberAdded(interest, listener, notifyAboutExisting);
}

void LocalParticipant::subscriberRemoved(std::shared_ptr<Interest> interest,
                                         ObjectProviderListener* listener,
                                         bool notifyAboutExisting)
{
  localSubscriberRemoved(interest, listener, notifyAboutExisting);
}

void LocalParticipant::subscriberRemoved(ObjectProviderListener* listener, bool notifyAboutExisting)
{
  localSubscriberRemoved(listener, notifyAboutExisting);
}

}  // namespace sen::kernel::impl
