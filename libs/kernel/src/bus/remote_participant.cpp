// === remote_participant.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "./remote_participant.h"

// implementation
#include "kernel_impl.h"

// bus
#include "bus/bus.h"
#include "bus/containers.h"
#include "bus/generic_remote_object.h"
#include "bus/local_participant.h"
#include "bus/remote_interest_manager.h"
#include "bus/session.h"
#include "bus/util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/memory_block.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/obj/detail/event_buffer.h"
#include "sen/core/obj/detail/remote_object.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_provider.h"
#include "sen/kernel/transport.h"
#include "sen/kernel/type_specs_utils.h"

// generated
#include "bus_protocol.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/type_specs.stl.h"

// spdlog
#include <spdlog/logger.h>  // NOLINT (misc-include-cleaner): indirectly used by log function

// std
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

namespace sen::kernel::impl
{

namespace
{
//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

template <typename T>
[[nodiscard]] inline MemBlockPtr makeControlMessage(T&& payload)
{
  ControlMessage msg(std::forward<T>(payload));

  auto block = std::make_shared<ResizableHeapBlock>();
  block->resize(1U + SerializationTraits<ControlMessage>::serializedSize(msg));

  BufferWriter writer(*block);
  OutputStream out(writer);

  out.writeUInt8(static_cast<uint8_t>(MessageCategory::controlMessage));
  SerializationTraits<ControlMessage>::write(out, msg);

  return block;
}

// review required in case of multiple inheritance implementation NOLINTNEXTLINE
[[nodiscard]] inline MaybeConstTypeHandle<ClassType> findClosestParent(ConstTypeHandle<ClassType> meta,
                                                                       const CustomTypeRegistry& localRegistry)
{
  if (localRegistry.get(meta->getQualifiedName().data()))
  {
    return meta;
  }

  for (auto& parent: meta->getParents())
  {
    if (auto parentMeta = findClosestParent(parent, localRegistry))
    {
      return parentMeta;
    }
  }

  return std::nullopt;
}

/// Timeout for an object to be completely resolved (type and state) after this participant is notified that the object
/// has been published remotely. Used to prevent the buffering of updates in the consumer from getting out of control.
constexpr std::chrono::steady_clock::duration remoteObjectResolutionTimeout = std::chrono::seconds(5);

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// RemoteProvider
//--------------------------------------------------------------------------------------------------------------

void RemoteProvider::notifyObjectsAdded(const ObjectAdditionList& additions)
{
  std::vector<ObjectAddition> nonRepeatedAdditions;
  nonRepeatedAdditions.reserve(additions.size());

  for (const auto& addition: additions)
  {
    const auto id = getObjectId(addition);

    if (currentAdditions_.find(id) == currentAdditions_.end())
    {
      currentAdditions_.try_emplace(id, addition);
      nonRepeatedAdditions.push_back(addition);
    }
  }

  ObjectProvider::notifyObjectsAdded(nonRepeatedAdditions);
}

void RemoteProvider::notifyObjectsRemoved(const ObjectRemovalList& removals)
{
  for (const auto& removal: removals)
  {
    currentAdditions_.erase(removal.objectid);
  }

  ObjectProvider::notifyObjectsRemoved(removals);
}

void RemoteProvider::notifyAddedOnExistingObjects(ObjectProviderListener* listener)
{
  if (!currentAdditions_.empty())
  {
    std::vector<ObjectAddition> additions;
    additions.reserve(currentAdditions_.size());

    for (const auto& [id, addition]: currentAdditions_)
    {
      additions.push_back(addition);
    }

    callOnObjectsAdded(listener, additions);
  }
}

void RemoteProvider::notifyRemovedOnExistingObjects(ObjectProviderListener* listener)
{
  std::vector<ObjectRemoval> removals;
  removals.reserve(currentAdditions_.size());

  for (const auto& [id, addition]: currentAdditions_)
  {
    removals.push_back(makeRemoval(addition));
  }

  callOnObjectsRemoved(listener, removals);
}

//--------------------------------------------------------------------------------------------------------------
// RemoteObjectFilter
//--------------------------------------------------------------------------------------------------------------

RemoteObjectFilter::RemoteObjectFilter(RemoteParticipant* owner): owner_(owner) {}

RemoteObjectFilter::~RemoteObjectFilter() = default;

void RemoteObjectFilter::addSubscriber(std::shared_ptr<Interest> interest,
                                       ObjectProviderListener* listener,
                                       bool notifyAboutExisting)
{
  // try to find an existing provider, and use it if available
  for (const auto& [first, second]: providers_)
  {
    if (second->getInterest() == *interest)
    {
      second->addListener(listener, notifyAboutExisting);

      // notify the other process
      if (const auto* localParticipant = listener->isLocalParticipant(); localParticipant != nullptr)
      {
        owner_->localInterestStarted(*localParticipant, interest);
      }
      return;
    }
  }

  // if here, it is the first time we have seen interest in this one,
  // so we need to notify the other process about it in order for it to
  // start sending us objects that satisfy it.

  // create and save the provider
  auto provider = std::make_unique<RemoteProvider>(interest);
  auto [itr, done] = providers_.insert({interest->getId(), std::move(provider)});

  // add the listener
  itr->second->addListener(listener, notifyAboutExisting);

  // notify the other process
  if (const auto* localParticipant = listener->isLocalParticipant(); localParticipant != nullptr)
  {
    owner_->localInterestStarted(*localParticipant, interest);
  }
}

void RemoteObjectFilter::removeSubscriber(std::shared_ptr<Interest> interest,
                                          ObjectProviderListener* listener,
                                          bool notifyAboutExisting)
{
  // try to find an existing provider, and use it if available
  for (const auto& [id, provider]: providers_)
  {
    if (provider->getInterest() == *interest)
    {
      provider->removeListener(listener, notifyAboutExisting);

      if (!provider->hasListeners())
      {
        // it was the last listener for this provider, we don't need it anymore
        if (const auto* localParticipant = listener->isLocalParticipant(); localParticipant != nullptr)
        {
          owner_->localInterestStopped(*localParticipant, id);
        }

        providers_.erase(id);
      }
      return;
    }
  }
}

void RemoteObjectFilter::removeSubscriber(ObjectProviderListener* listener, bool notifyAboutExisting)
{
  std::vector<decltype(providers_)::key_type> toRemove;
  toRemove.reserve(providers_.size());

  // remove from all providers
  for (const auto& [id, provider]: providers_)
  {
    provider->removeListener(listener, notifyAboutExisting);

    // mark it for removal if it has no listeners
    if (!provider->hasListeners())
    {
      // it was the last listener for this provider, we don't need it anymore
      if (const auto* localParticipant = listener->isLocalParticipant(); localParticipant != nullptr)
      {
        owner_->localInterestStopped(*localParticipant, id);
      }

      toRemove.push_back(id);
    }
  }

  // remove the marked ones
  for (const auto& elem: toRemove)
  {
    providers_.erase(elem);
  }
}

void RemoteObjectFilter::remoteObjectsAdded(InterestId interestId, const ObjectAdditionList& additions)
{
  if (const auto itr = providers_.find(interestId); itr != providers_.end())
  {
    itr->second->notifyObjectsAdded(additions);
  }
}

void RemoteObjectFilter::remoteObjectsRemoved(InterestId interestId, const ObjectRemovalList& removals)
{
  auto itr = providers_.find(interestId);
  if (itr != providers_.end())
  {
    itr->second->notifyObjectsRemoved(removals);
  }
}

//--------------------------------------------------------------------------------------------------------------
// UpdatesBufferStorage
//--------------------------------------------------------------------------------------------------------------

void UpdatesBufferStorage::addUpdatesIfNeeded(const ObjectUpdateMessage& message)
{
  if (const auto itr = data_.find(message.objectId); itr != data_.end())
  {
    BufferedObjectUpdate update;
    {
      update.buffer.resize(message.buffer.size());
      memcpy(update.buffer.data(), message.buffer.data(), message.buffer.size());
      update.time = message.time;
    }

    itr->second.updates.push_back(std::move(update));
  }
}

void UpdatesBufferStorage::startMonitoring(const ObjectAdded& addition,
                                           Transport& transport,
                                           std::function<void()>&& timeoutCallback)
{
  // only start one timer per object (we can receive the same addition for different interests in the
  // remoteObjectsPublished message)
  if (data_.find(addition.id) == data_.end())
  {
    const auto timerId = transport.startTimer(remoteObjectResolutionTimeout, std::move(timeoutCallback));
    data_.emplace(addition.id, BufferedObjectData {timerId, addition, {}, {}});
  }
}

void UpdatesBufferStorage::setDynamicProperties(const ObjectState& objState)
{
  if (const auto itr = data_.find(objState.id); itr != data_.end())
  {
    itr->second.dynamicState = objState;
  }
}

void UpdatesBufferStorage::stopMonitoring(ObjectId objectId, Transport& transport)
{
  if (const auto it = data_.find(objectId); it != data_.end())
  {
    std::ignore = transport.cancelTimer(it->second.timerId);
    data_.erase(objectId);
  }
}

void UpdatesBufferStorage::eraseMonitoredData(ObjectId objectId) { data_.erase(objectId); }

void UpdatesBufferStorage::applyStateAndStopMonitoring(sen::impl::RemoteObject& proxy, Transport& transport)
{
  if (const auto itr = data_.find(proxy.getId()); itr != data_.end())
  {
    // initialize with addition
    proxy.initializeState(itr->second.staticState.state.asVector(), itr->second.staticState.time);

    // initialize with the requested state
    const auto& dynamicStateTime = itr->second.dynamicState.timestamp;
    proxy.initializeState(itr->second.dynamicState.state.asVector(), dynamicStateTime);

    // apply updates newer than the requested state
    for (const auto& update: itr->second.updates)
    {
      if (update.time > dynamicStateTime)
      {
        proxy.initializeState(update.buffer, update.time);
      }
    }

    stopMonitoring(proxy.getId(), transport);
  }
}

const ObjectAdded* UpdatesBufferStorage::getStaticState(const ObjectId& id) const noexcept
{
  if (const auto it = data_.find(id); it != data_.end())
  {
    return &it->second.staticState;
  }

  return nullptr;
}

//--------------------------------------------------------------------------------------------------------------
// RemoteParticipant
//--------------------------------------------------------------------------------------------------------------

RemoteParticipant::RemoteParticipant(ProcessInfo processInfo, ParticipantAddr address, Bus* bus, Session* session)
  : processInfo_(std::move(processInfo))
  , ownerAddress_(address)
  , busId_(bus->getId())
  , bus_(bus)
  , session_(session)
  , incomingInterestsManager_(this)
  , smallBufferPool_(SmallPool::make())
  , logger_(KernelImpl::getKernelLogger())
{
  debugName_ = std::to_string(ownerAddress_.id.get());
  debugName_.append(".");
  debugName_.append(bus_->getAddress().sessionName);
  debugName_.append(".");
  debugName_.append(bus_->getAddress().busName);

  logger_->debug("RP {}: created", getDebugName());
}

RemoteParticipant::~RemoteParticipant()
{
  bus_->remoteSubscriberRemoved(this, false);
  bus_->remoteParticipantRemoved(this);

  while (!trackedProxies_.empty())
  {
    stopTrackingProxy(trackedProxies_.begin()->first);
  }
}

void RemoteParticipant::localInterestStarted(const LocalParticipant& source, std::shared_ptr<Interest> interest)
{
  Lock usageLock(usageMutex_);

  InterestStarted msg {interest->getQueryString(), interest->getId().get()};

  const auto sourceId = source.getId();

  if (participantReady_)
  {
    logger_->debug("{}  → interest → {}", sourceId.get(), getId().get());
    sendControlMessage(getRemoteAddress(sourceId), std::move(msg));
  }
  else
  {
    // buffer the local participant (source id) and the interest message to be sent
    pendingInterestStartedMessages_[sourceId].emplace_back(std::move(msg));
  }
}

::sen::impl::SendCallFunc RemoteParticipant::getProxyCallFunc(const ClassType* meta)
{
  return [usWeak = weak_from_this(), meta](TransportMode mode,
                                           ObjectOwnerId ownerId,
                                           ObjectId objectId,
                                           MemberHash methodId,
                                           auto&& argsBuffer) -> Result<sen::impl::CallId, std::string>
  {
    // send a method call request
    auto argsSpan = argsBuffer->getConstSpan();

    if (auto us = usWeak.lock(); us)
    {
      ++us->nextCallId_;

      auto remoteAddress = us->getRemoteAddress(ownerId);

      us->sendToBus(remoteAddress,
                    mode,
                    us->makeMethodCallHeader(mode,
                                             us->ownerAddress_.id,
                                             objectId,
                                             methodId,
                                             us->nextCallId_,
                                             static_cast<uint32_t>(argsSpan.size()),
                                             meta),
                    std::forward<decltype(argsBuffer)>(argsBuffer));

      return Ok(us->nextCallId_);
    }

    return Err(std::string("object deleted"));
  };
}

void RemoteParticipant::remoteParticipantReady(const RemoteParticipantReady& msg)
{
  Lock usageLock(usageMutex_);

  logger_->debug("{} <- rp ready <- {}", msg.id, getId().get());
  if (!participantReady_)
  {
    participantReady_ = true;

    logger_->debug("{} -> rp ready -> {}", msg.id, getId().get());
    sendControlMessage(getRemoteAddress(msg.id), RemoteParticipantReady {getId().get()});
    sendPendingInterestStartedMessages();
  }
}

ParticipantAddr RemoteParticipant::getRemoteAddress(ObjectOwnerId ownerId) const noexcept
{
  auto result = ownerAddress_;
  result.id = ownerId;
  return result;
}

void RemoteParticipant::tryReadControlMsg(InputStream& in, ControlMessage& msg) const
{
  try
  {
    SerializationTraits<ControlMessage>::read(in, msg);
  }
  catch (...)
  {
    logger_->error("RP {}: received an unknown or invalid ControlMessage.", getDebugName());
    throw std::runtime_error("Unknown or invalid ControlMessage");
  }
}

void RemoteParticipant::sendPendingInterestStartedMessages()
{
  for (auto& [sourceId, messages]: pendingInterestStartedMessages_)
  {
    for (auto& msg: messages)
    {
      logger_->debug("{}  → interest → {}", sourceId.get(), getId().get());
      sendControlMessage(getRemoteAddress(sourceId), std::move(msg));
    }
  }

  pendingInterestStartedMessages_.clear();
}

const RemoteParticipant::PropertyCount& RemoteParticipant::getOrComputePropertyCount(const ClassType& meta) noexcept
{
  const auto& classId = meta.getId();

  if (const auto it = classIdToPropertyCountMap_.find(classId); it != classIdToPropertyCountMap_.end())
  {
    return it->second;
  }

  // cache the new property count
  PropertyCount propertyCount {};
  {
    for (const auto& prop: meta.getProperties(ClassType::SearchMode::includeParents))
    {
      if (const auto& category = prop->getCategory();
          category == PropertyCategory::staticRO || category == PropertyCategory::staticRW)
      {
        propertyCount.staticProps++;
      }
      else
      {
        propertyCount.dynamicProps++;
      }
    }
  }
  classIdToPropertyCountMap_[classId] = propertyCount;

  return classIdToPropertyCountMap_[classId];
}

void RemoteParticipant::localInterestStopped(const LocalParticipant& source, InterestId interestId)
{
  Lock usageLock(usageMutex_);
  const auto interestIdVal = interestId.get();

  logger_->debug(
    "RP {} : local participant {} stopped interest {}", getDebugName(), source.getDebugName(), interestIdVal);

  // remove the pending interest started if there is one
  if (const auto it = pendingInterestStartedMessages_.find(source.getId()); it != pendingInterestStartedMessages_.end())
  {
    auto& interestMsgVec = it->second;

    interestMsgVec.erase(std::remove_if(interestMsgVec.begin(),
                                        interestMsgVec.end(),
                                        [interestIdVal](const auto& msg) { return msg.id == interestIdVal; }),
                         interestMsgVec.end());

    if (interestMsgVec.empty())
    {
      pendingInterestStartedMessages_.erase(it);
    }
  }

  sendControlMessage(getRemoteAddress(source.getId()), InterestStopped {interestId.get()});
}

void RemoteParticipant::onObjectsAdded(const ObjectAdditionList& additions)
{
  if (additions.empty())
  {
    return;
  }

  // to collect all the objects to be sent
  ObjectsPublished objectsPublished {};
  objectsPublished.ownerId = getId().get();

  auto ownerId = getObjectOwnerId(additions[0]);

  for (const auto& addition: additions)  // NOLINT
  {
    if (auto* localDiscovery = std::get_if<ObjectInstanceDiscovery>(&addition))
    {
      auto* localObject = localDiscovery->instance->asNativeObject();
      auto meta = localObject->getClass();

      logger_->debug(
        "RP {} : detected local object ({}, {})", getDebugName(), localObject->getName(), localObject->getId().get());

      // save the reference to the object we now know
      {
        Lock usageLock(usageMutex_);
        knownLocalObjects_[localObject->getId()] = localObject;
      }

      // find an existing entry for the interest id
      InterestDiscovery* interestDiscovery = nullptr;
      for (auto& elem: objectsPublished.discoveries)
      {
        if (elem.interestId == localDiscovery->interestId.get())
        {
          interestDiscovery = &elem;
          break;
        }
      }

      // if not found, create one
      if (interestDiscovery == nullptr)
      {
        objectsPublished.discoveries.emplace_back();
        interestDiscovery = &objectsPublished.discoveries.back();
        interestDiscovery->interestId = localDiscovery->interestId.get();
      }

      interestDiscovery->objects.emplace_back();
      auto& obj = interestDiscovery->objects.back();

      // reserve some space for each property
      constexpr std::size_t estSizeOfASerializedProperty = 10U;
      obj.state.reserve(getOrComputePropertyCount(*meta).staticProps *
                        estSizeOfASerializedProperty);  // NOLINT(readability-magic-numbers)

      obj.name = localObject->getName();
      obj.typeHash = meta->getHash().get();
      obj.id = localObject->getId().get();
      obj.className = meta->getQualifiedName();
      obj.time = localObject->getLastCommitTime();

      ResizableBufferWriter writer(obj.state);
      OutputStream stream(writer);
      auto objectReadLock = localObject->createReaderLock();
      localObject->senImplWriteStaticPropertiesToStream(stream);
    }
  }

  logger_->debug("RP: {} → additions → RP: {}", ownerId.get(), getId().get());

  sendControlMessage(getRemoteAddress(ownerId), std::move(objectsPublished));
}

void RemoteParticipant::onObjectsRemoved(const ObjectRemovalList& removals)
{
  if (removals.empty())
  {
    return;
  }

  ObjectsRemoved objectsRemoved;

  auto ownerId = removals[0].ownerId;

  for (const auto& removal: removals)
  {
    // find an existing entry for the interest id
    InterestObjectRemoval* interestRemoval = nullptr;
    for (auto& elem: objectsRemoved.removals)
    {
      if (elem.interestId == removal.interestId.get())
      {
        interestRemoval = &elem;
        break;
      }
    }

    // if not found, create one
    if (interestRemoval == nullptr)
    {
      objectsRemoved.removals.emplace_back();
      interestRemoval = &objectsRemoved.removals.back();
      interestRemoval->interestId = removal.interestId.get();
    }

    interestRemoval->ids.push_back(removal.objectid.get());

    logger_->debug(
      "RP {} : removing local object {} and informing {}", getDebugName(), interestRemoval->ids.back(), ownerId.get());
  }

  // remove the reference to the objects
  {
    Lock usageLock(usageMutex_);
    for (const auto& removal: removals)
    {
      knownLocalObjects_.erase(removal.objectid);
    }
  }

  sendControlMessage(getRemoteAddress(ownerId), std::move(objectsRemoved));
}

void RemoteParticipant::sendObjectUpdate(ObjectOwnerId ownerId, ObjectUpdate* update)
{
  update->applyToNativeObject(
    [&](const auto* obj)
    {
      if (auto reliableProps = update->getReliableProperties(); !reliableProps->empty())
      {
        logger_->debug("RP: {} → updates of {} → RP: {}", ownerId.get(), obj->getName(), getId().get());
        sendUpdates(ownerId, obj, std::move(reliableProps), TransportMode::confirmed);
      }

      const auto& unicastProps = update->getUnicastProperties();
      for (const auto& message: unicastProps)
      {
        sendUpdates(ownerId, obj, message, TransportMode::unicast);
      }

      if (!update->getMulticastPropsSent())
      {
        const auto& multicastProps = update->getMulticastProperties();
        for (const auto& message: multicastProps)
        {
          sendUpdates(ownerId, obj, message, TransportMode::multicast);
        }

        update->setMulticastPropsSent(true);
      }
    });
}

void RemoteParticipant::sendUpdates(ObjectOwnerId ownerId,
                                    const NativeObject* object,
                                    MemBlockPtr&& propertiesBuffer,
                                    TransportMode mode)
{
  auto span = propertiesBuffer->getConstSpan();

  if (!span.empty())
  {
    auto to = getRemoteAddress(ownerId);
    sendToBus(to, mode, makeObjectUpdateHdr(object, static_cast<uint32_t>(span.size())), std::move(propertiesBuffer));
  }
}

void RemoteParticipant::sendEventsBuffer(MemBlockPtr&& events, ParticipantAddr& owner, TransportMode mode)
{
  sendToBus(owner, mode, makeEventsHeader(), std::move(events));
}

void RemoteParticipant::sendEventsUnicast(BestEffortBufferList&& events, ObjectOwnerId owner)
{
  if (events.empty())
  {
    return;
  }

  auto address = getRemoteAddress(owner);
  for (auto& msg: events)
  {
    sendEventsBuffer(std::move(msg), address, TransportMode::unicast);
  }
}

void RemoteParticipant::sendEventsMulticast(BestEffortBufferList&& events, ObjectOwnerId owner)
{
  if (events.empty())
  {
    return;
  }

  auto address = getRemoteAddress(owner);
  for (auto& msg: events)
  {
    sendEventsBuffer(std::move(msg), address, TransportMode::multicast);
  }
}

void RemoteParticipant::sendEventsConfirmed(MemBlockPtr events, ObjectOwnerId owner)
{
  auto address = getRemoteAddress(owner);
  sendEventsBuffer(std::move(events), address, TransportMode::confirmed);
}

std::pair<MaybeConstTypeHandle<>, bool> RemoteParticipant::searchForRemoteType(uint32_t typeHash) const
{
  return {remoteTypesRegistry_.get(typeHash),
          incompatibleClassHashes_.find(typeHash) != incompatibleClassHashes_.end()};
}

void RemoteParticipant::sendRemoteParticipantReady(ObjectOwnerId ownerId)
{
  logger_->debug("{} -> connect -> {}", ownerId.get(), getId().get());
  sendControlMessage(getRemoteAddress(ownerId), RemoteParticipantReady {getId().get()});
}

void RemoteParticipant::rcvControlMessage(InputStream& in)
{
  ControlMessage decodedMessage {};
  tryReadControlMsg(in, decodedMessage);

  return std::visit(Overloaded {[&](RemoteParticipantReady& arg) { remoteParticipantReady(arg); },
                                [&](InterestStarted& arg) { remoteInterestStarted(arg); },
                                [&](InterestStopped& arg) { remoteInterestStopped(arg); },
                                [&](ObjectsPublished& arg) { remoteObjectsPublished(arg); },
                                [&](ObjectsRemoved& arg) { remoteObjectsRemoved(arg); },
                                [&](PublicationRejection& arg) { publicationRejected(arg); },
                                [&](ObjectsStateRequest& arg) { objectsStateRequest(arg); },
                                [&](ObjectsStateResponse& arg) { objectsStateResponse(arg); },
                                [&](TypesInfoRequest& arg) { typesInfoRequest(arg); },
                                [&](TypesInfoResponse& arg) { typesInfoResponse(arg); },
                                [&](TypesInfoRejection& arg) { typesInfoRejection(arg); }},
                    decodedMessage);
}

void RemoteParticipant::rcvObjectUpdate(const ObjectUpdateMessage& message)
{
  Lock lock(usageMutex_);

  monitoredObjects_.addUpdatesIfNeeded(message);

  if (auto mapItr = trackedProxies_.find(message.objectId); mapItr != trackedProxies_.end())
  {
    for (auto& elem: *mapItr->second)
    {
      elem->propertyUpdatesReceived(message.buffer, message.time);
    }
  }
}

void RemoteParticipant::rcvMethodCall(TransportMode mode, InputStream& in)
{
  uint32_t ownerId = 0U;
  uint32_t objectId = 0U;
  uint32_t methodId = 0U;
  uint32_t ticketId = 0U;
  uint32_t argsSize = 0U;

  in.readUInt32(ownerId);
  in.readUInt32(objectId);
  in.readUInt32(methodId);
  in.readUInt32(ticketId);
  in.readUInt32(argsSize);

  auto arguments = makeConstSpan(in.advance(argsSize), argsSize);
  auto responseAddress = getRemoteAddress(ownerId);

  {
    Lock usageLock(usageMutex_);

    auto itr = knownLocalObjects_.find(objectId);
    if (itr == knownLocalObjects_.end())
    {
      sendToBus(responseAddress, mode, makeObjectNotFoundResponseHeader(objectId, ticketId));
    }
    else
    {
      auto* localObject = itr->second;

      // we need to copy the args out of the buffer, as this call will be executed by a runner

      localObject->addWorkToQueue(
        [us = shared_from_this(),
         responseAddress,
         localObject,
         objectId,
         ticketId,
         methodId,
         mode,
         argsBuffer = std::make_shared<Buffer>(arguments.begin(), arguments.end())]() mutable
        {
          auto callForwarder =
            [us = std::move(us), mode, responseAddress, objectId, ticketId](StreamCall&& call) mutable
          {
            // if the remote participant is marked as removed, this queued call is invalid
            if (us->removed_)
            {
              return;
            }

            // we use a resizable buffer writer because we expect this buffer to
            // contain 0 or 1 value, therefore only one allocation is expected.
            auto returnBuffer = std::make_shared<ResizableHeapBlock>();

            // anyway, reserve some space to avoid re-allocations
            returnBuffer->reserve(maxBestEffortMessageSize);

            ResizableBufferWriter retValWriter(*returnBuffer);
            OutputStream retValOut(retValWriter);

            // make the call (this might throw)
            try
            {
              call(retValOut);
              const auto returnBufferSize = static_cast<uint32_t>(returnBuffer->size());

              // send back the result
              us->sendToBus(responseAddress,
                            mode,
                            us->makeMethodResponseHeader(objectId, ticketId, returnBufferSize),
                            std::move(returnBuffer));
            }
            catch (const std::logic_error& err)
            {
              us->sendToBus(responseAddress, mode, makeLogicErrorResponseHeader(objectId, ticketId, err.what()));
            }
            catch (const std::exception& err)
            {
              us->sendToBus(responseAddress, mode, makeRuntimeErrorResponseHeader(objectId, ticketId, err.what()));
            }
            catch (...)
            {
              us->sendToBus(responseAddress, mode, us->makeUnknownErrorResponseHeader(objectId, ticketId));
            }
          };

          InputStream argsIn(argsBuffer->asVector());

          // ensure that the stream call can modify the local object
          localObject->senImplStreamCall(methodId, argsIn, std::move(callForwarder));
        },
        ::sen::impl::cannotBeDropped(mode));
    }
  }
}

void RemoteParticipant::rcvMethodResponse(InputStream& in)
{
  uint8_t responseResultValue = 0U;
  uint32_t objectId = 0U;
  uint32_t ticketId = 0U;

  in.readUInt8(responseResultValue);
  in.readUInt32(objectId);
  in.readUInt32(ticketId);

  // to hold what the proxy will receive
  ::sen::impl::MethodResponseData responseData {};
  responseData.callId = ticketId;
  responseData.result = static_cast<::sen::impl::RemoteCallResult>(responseResultValue);

  switch (responseData.result)
  {
    case ::sen::impl::RemoteCallResult::success:
    {
      // read the size and get the buffer containing the return value
      uint32_t retValSize = 0U;
      in.readUInt32(retValSize);

      if (retValSize != 0U)
      {
        auto retValBuffer = makeConstSpan(in.advance(retValSize), retValSize);
        responseData.returnValBuffer = std::make_shared<std::vector<uint8_t>>(retValBuffer.begin(), retValBuffer.end());
      }
      else
      {
        responseData.returnValBuffer = std::make_shared<std::vector<uint8_t>>();
      }
    }
    break;

    case ::sen::impl::RemoteCallResult::runtimeError:
    case ::sen::impl::RemoteCallResult::logicError:
      in.readString(responseData.error);
      break;

    case ::sen::impl::RemoteCallResult::objectNotFound:
      responseData.error = "object not found";
      break;

    default:
      // no need to specify a string for these errors
      break;
  }

  // send the response to the proxy that issued the call
  RemoteObjectListPtr remotes;
  {
    Lock lock(usageMutex_);
    auto mapItr = trackedProxies_.find(objectId);
    if (mapItr == trackedProxies_.end())
    {
      return;
    }
    remotes = mapItr->second;
  }

  for (auto& elem: *remotes)
  {
    if (elem->responseReceived(responseData))
    {
      break;
    }
  }
}

void RemoteParticipant::rcvEvents(InputStream& in)
{
  EventFromStream eventData {};

  while (!in.atEnd())
  {
    readEventFromStream(in, eventData);

    // find the associated proxies
    RemoteObjectListPtr remotes;
    {
      Lock lock(usageMutex_);
      auto mapItr = trackedProxies_.find(eventData.producerId);
      if (mapItr == trackedProxies_.end())
      {
        continue;
      }
      remotes = mapItr->second;
    }

    // make all the proxies emit the event out of the received buffer
    for (auto& proxyPtr: *remotes)
    {
      proxyPtr->eventReceived(eventData.eventId, eventData.creationTime, eventData.argumentsBuffer);
    }
  }
}

void RemoteParticipant::remoteInterestStarted(InterestStarted& msg)
{
  logger_->debug("RP: {} ← interest ← RP: {}", msg.id, getId().get());

  try
  {
    auto interest = Interest::make(msg.query, session_->getKernel().getTypes());

    // store the id and associated interest
    remoteInterests_.try_emplace(msg.id, interest);

    // make local participants aware of our interest
    bus_->remoteSubscriberAdded(interest, this, true);
  }
  catch (const std::exception& err)
  {
    logger_->error(err.what());
  }
}

void RemoteParticipant::remoteInterestStopped(InterestStopped& msg)
{
  logger_->debug("RP {}: received remote interest stopped message (id: {})", getDebugName(), msg.id);

  // make local participants aware that we are no longer interested
  InterestId id(msg.id);
  auto itr = remoteInterests_.find(id);

  if (itr != remoteInterests_.end())
  {
    bus_->remoteSubscriberRemoved(itr->second, this, true);
    remoteInterests_.erase(itr);
  }
}

void RemoteParticipant::remoteObjectsPublished(ObjectsPublished& msg)
{
  Lock usageLock(usageMutex_);

  logger_->debug("RP {}: received remote objects published message", getDebugName());

  constexpr std::size_t rejectionFactor = 5U;  // assume 5 objects per interest
  PublicationRejection rejectionsResponse;
  rejectionsResponse.rejections.reserve(msg.discoveries.size() * rejectionFactor);

  std::unordered_set<u32> typesToRequest;

  ObjectIdsByInterestList additionsByInterest;
  additionsByInterest.reserve(msg.discoveries.size());

  for (const auto& discovery: msg.discoveries)
  {
    ObjectIdsByInterest objectIdList {discovery.interestId, {}};
    objectIdList.objectIds.reserve(discovery.objects.size());

    for (const auto& msgObject: discovery.objects)
    {
      // first possibility: we have the type -> ask for the dynamic state
      if (session_->getKernel().getTypes().get(msgObject.typeHash) || remoteTypesRegistry_.get(msgObject.typeHash))
      {
        processObjectAddition(objectIdList.objectIds, rejectionsResponse, msgObject, discovery.interestId, msg.ownerId);
      }
      // second possibility: we can find the type in the bus -> ask for the dynamic state
      else if (const auto& [typeInBus, isIncompatible] =
                 bus_->searchForTypeInRemoteParticipants(this, msgObject.typeHash);
               typeInBus)
      {
        SEN_ASSERT(typeInBus.value()->isCustomType());
        registerRemoteTypeFoundInBus(dynamicTypeHandleCast<const CustomType>(typeInBus.value()).value(),
                                     isIncompatible);
        processObjectAddition(objectIdList.objectIds, rejectionsResponse, msgObject, discovery.interestId, msg.ownerId);
      }
      // third possibility: we don't know the type -> we ask for the type.
      // Once we have it, we ask for the dynamic state.
      else
      {
        // add to pending additions
        typesToRequest.insert(msgObject.typeHash);
        pendingAdditions_[msgObject.typeHash][msg.ownerId][discovery.interestId].push_back(std::move(msgObject));
      }
    }

    if (!objectIdList.objectIds.empty())
    {
      additionsByInterest.emplace_back(std::move(objectIdList));
    }
  }

  if (!additionsByInterest.empty())
  {
    // ask the dynamic state for the objects that we have accepted.
    sendControlMessage(getRemoteAddress(msg.ownerId),
                       ObjectsStateRequest {getId().get(), std::move(additionsByInterest)});
  }

  if (!rejectionsResponse.rejections.empty())
  {
    logger_->debug("RP {}: rejecting ObjectsPublished", getId().get());
    sendControlMessage(getRemoteAddress(msg.ownerId), std::move(rejectionsResponse));
  }

  // request missing types
  if (!typesToRequest.empty())
  {
    SpecRequestList requests;
    requests.reserve(typesToRequest.size());
    requests.insert(requests.end(), typesToRequest.begin(), typesToRequest.end());

    logger_->debug("RP {}: requesting missing remote object types", getDebugName());
    sendControlMessage(getRemoteAddress(msg.ownerId), TypesInfoRequest {getId().get(), requests});
  }
}

void RemoteParticipant::remoteObjectsRemoved(const ObjectsRemoved& msg)
{
  Lock usageLock(usageMutex_);
  logger_->debug("RP {}: received remote objects removed message", getDebugName());

  for (const auto& interestObjectsRemoval: msg.removals)
  {
    const InterestId interestId(interestObjectsRemoval.interestId);

    std::vector<ObjectRemoval> removals;
    removals.reserve(interestObjectsRemoval.ids.size());

    for (const auto elem: interestObjectsRemoval.ids)
    {
      auto objectId = ObjectId(elem);

      stopTrackingProxy(objectId);

      ObjectRemoval removal;
      removal.interestId = interestId;
      removal.objectid = objectId;
      removals.push_back(removal);
    }

    logger_->debug("RP {}: processing remotely removed objects", getDebugName());
    incomingInterestsManager_.remoteObjectsRemoved(interestId, removals);
  }

  // clean the objects from pending additions if we still have them stored
  // 1. Build a lookup table:  interest -> set of ids that must be removed
  std::unordered_map<InterestId, std::unordered_set<ObjectId>> toRemove;
  toRemove.reserve(msg.removals.size());

  for (const auto& [interestId, ids]: msg.removals)  // r has: interestId, ids
  {
    toRemove[interestId].insert(ids.begin(), ids.end());
  }

  // 2. Walk the pendingAdditions_ tree
  for (auto& [type, owners]: pendingAdditions_)
  {
    for (auto& [owner, interests]: owners)
    {
      for (auto& [interest, objAdditionList]: interests)
      {
        auto it = toRemove.find(interest);
        if (it == toRemove.end())
        {
          continue;  // nothing to remove
        }

        const auto& idsToRemove = it->second;

        // 3. Erase all pending object additions whose id is in idsToRemove
        objAdditionList.erase(std::remove_if(objAdditionList.begin(),
                                             objAdditionList.end(),
                                             [&idsToRemove](const auto& elem) { return idsToRemove.count(elem.id); }),
                              objAdditionList.end());

        if (objAdditionList.empty())
        {
          interests.erase(interest);
        }
      }

      if (interests.empty())
      {
        owners.erase(owner);
      }
    }

    if (owners.empty())
    {
      pendingAdditions_.erase(type);
    }
  }
}

void RemoteParticipant::publicationRejected(const PublicationRejection& msg) { bus_->publicationsRejected(msg, this); }

void RemoteParticipant::typesInfoRequest(const TypesInfoRequest& msg)
{
  TypeSpecResponseList response;
  const auto& types = session_->getKernel().getTypes();

  StringList rejections;

  for (const auto hash: msg.requests)
  {
    if (auto type = types.get(hash); type)
    {
      if (!type.value()->isCustomType())
      {
        // just ignore native types (we shouldn't be receiving any)
        continue;
      }

      const auto* customType = type.value()->asCustomType();
      if (customType->isClassType())
      {
        TypeHashList dependentTypeHashes;
        std::function cb = [&dependentTypeHashes](ConstTypeHandle<> dependent)
        {
          if (dependent->isCustomType())
          {
            dependentTypeHashes.push_back(dependent->getHash().get());
          }
        };

        iterateOverDependentTypes(*type.value(), cb);

        response.emplace_back(ClassSpecResponse {
          type.value()->getHash().get(), makeCustomTypeSpec(customType), std::move(dependentTypeHashes)});
      }
      else
      {
        response.emplace_back(NonClassSpecResponse {makeCustomTypeSpec(customType)});
      }

      logger_->debug("RP {}: sending type {} to remote participant with id: {}",
                     getDebugName(),
                     customType->getQualifiedName(),
                     msg.ownerId);
    }
    else
    {
      std::string rejection;
      {
        rejection.append("Type with hash ");
        rejection.append(std::to_string(hash));
        rejection.append(
          " not found in the local registry. This is an unexpected logic error and could result in remote class "
          "resolution issues.");
      }

      rejections.push_back(rejection);
    }
  }

  // respond to requested types
  sendControlMessage(getRemoteAddress(msg.ownerId), TypesInfoResponse {getId().get(), std::move(response)});

  // send type info request rejections
  if (!rejections.empty())
  {
    sendControlMessage(getRemoteAddress(msg.ownerId), TypesInfoRejection {getId().get(), std::move(rejections)});
  }
}

void RemoteParticipant::typesInfoResponse(const TypesInfoResponse& msg)
{
  std::vector<u32> missingTypeHashes;

  auto insertUnique = [&missingTypeHashes](u32 value)
  {
    if (std::find(missingTypeHashes.begin(), missingTypeHashes.end(), value) == missingTypeHashes.end())
    {
      missingTypeHashes.push_back(value);
    }
  };

  auto& localTypesRegistry = session_->getKernel().getTypes();

  for (const auto& type: msg.types)
  {
    std::visit(
      sen::Overloaded {
        [this, &insertUnique, &localTypesRegistry](const ClassSpecResponse& response)
        {
          logger_->debug("RP {}: received class {}", getDebugName(), response.spec.qualifiedName);

          // check if we are missing any type
          for (auto typeHash: response.dependentTypes)
          {
            if (!localTypesRegistry.get(typeHash) && !remoteTypesRegistry_.get(typeHash) &&
                pendingClassSpecsMap_.find(typeHash) == pendingClassSpecsMap_.end())
            {
              // check if we have the type in any other participant
              if (const auto& [typeInBus, isIncompatible] = bus_->searchForTypeInRemoteParticipants(this, typeHash);
                  typeInBus)
              {
                SEN_ASSERT(typeInBus.value()->isCustomType());
                registerRemoteTypeFoundInBus(dynamicTypeHandleCast<const CustomType>(typeInBus.value()).value(),
                                             isIncompatible);
              }
              else
              {
                insertUnique(typeHash);
              }
            }
          }

          if (!localTypesRegistry.get(response.classHash) && !remoteTypesRegistry_.get(response.classHash) &&
              pendingClassSpecsMap_.find(response.classHash) == pendingClassSpecsMap_.end())
          {
            // mark the class as pending
            pendingClassSpecsList_.emplace_back(response);
            pendingClassSpecsMap_[response.classHash] = std::prev(pendingClassSpecsList_.end());
          }
        },
        [this](const NonClassSpecResponse& response)
        {
          logger_->debug("RP {}: received type {}", getDebugName(), response.spec.qualifiedName);
          registerRemoteType(response.spec);
        }},
      type);
  }

  // request missing types
  if (!missingTypeHashes.empty())
  {
    logger_->debug("RP {}: requesting missing dependent types", getDebugName());

    sendControlMessage(
      getRemoteAddress(msg.ownerId),
      TypesInfoRequest {getId().get(), SpecRequestList {missingTypeHashes.begin(), missingTypeHashes.end()}});
  }

  while (tryResolvePendingClasses())
  {
    // no code needed
  }
}

void RemoteParticipant::typesInfoRejection(const TypesInfoRejection& msg)
{
  logger_->error("RP {}: type info request rejected by remote participant {}:", getDebugName(), msg.ownerId);

  for (const auto& elem: msg.rejections)
  {
    logger_->error("  - {}", elem);
  }
}

void RemoteParticipant::processObjectAddition(ObjectIdList& additions,
                                              PublicationRejection& rejectionResponse,
                                              const ObjectAdded& object,
                                              uint32_t interestId,
                                              uint32_t ownerId)
{
  bool repeated = false;

  // see if we know any object with the same name
  for (const auto& [id, obj]: knownLocalObjects_)
  {
    if (obj->getName() == object.name)
    {
      repeated = true;
      rejectionResponse.rejections.push_back({object.id, interestId, RepeatedName {obj->getRegistrationTime()}});
      break;
    }
  }

  if (!repeated)
  {
    // we also ask the bus, in case there's any object with the same name
    if (const auto registrationTime = bus_->isObjectNameUsedLocally(object.name); registrationTime.has_value())
    {
      repeated = true;
      rejectionResponse.rejections.push_back({object.id, interestId, RepeatedName {registrationTime.value()}});
    }
  }

  // we reject the object if the types are incompatible.
  bool incompatible = false;
  if (std::find(incompatibleClassHashes_.begin(), incompatibleClassHashes_.end(), object.typeHash) !=
      incompatibleClassHashes_.end())
  {
    incompatible = true;
    rejectionResponse.rejections.push_back({object.id, interestId, RuntimeIncompatibleType {}});
  }

  if (!repeated && !incompatible)
  {
    // we start buffering property changes for this object right now because we know
    // that the object is valid and that we will eventually get it.

    auto timeoutCallback = [this, id = object.id, interestId, ownerId]()
    {
      // protect this access to the monitored objects via timer callback
      Lock usageLock(usageMutex_);

      // erase stored data for the object
      monitoredObjects_.eraseMonitoredData(id);

      // send object rejection (reason: timeout)
      sendControlMessage(getRemoteAddress(ownerId), PublicationRejection {{{id, interestId, Timeout {}}}});
    };

    monitoredObjects_.startMonitoring(object, session_->getTransport(), std::move(timeoutCallback));

    additions.emplace_back(object.id);
  }
}

void RemoteParticipant::objectsStateRequest(const ObjectsStateRequest& msg)
{
  Lock usageLock(usageMutex_);

  ObjectsStateResponse responseMsg {getId().get(), {}};
  responseMsg.responses.reserve(msg.requests.size());

  for (const auto& [interestId, objectIds]: msg.requests)
  {
    ObjectStatesByInterest objectStatesByInterest {interestId, {}};
    objectStatesByInterest.objectStates.reserve(objectIds.size());

    for (const auto id: objectIds)
    {
      if (const auto it = knownLocalObjects_.find(id); it != knownLocalObjects_.end())
      {
        const auto& localObj = it->second;
        auto& objectState = objectStatesByInterest.objectStates.emplace_back();

        constexpr std::size_t estSizeOfASerializedProperty = 10U;

        objectState.id = id;
        objectState.timestamp = localObj->getLastCommitTime();
        objectState.state.reserve(getOrComputePropertyCount(*localObj->getClass()).dynamicProps *
                                  estSizeOfASerializedProperty);

        ResizableBufferWriter writer(objectState.state);
        OutputStream stream(writer);
        auto objectReadLock = it->second->createReaderLock();
        localObj->senImplWriteDynamicPropertiesToStream(stream);
      }
    }

    if (!objectStatesByInterest.objectStates.empty())
    {
      responseMsg.responses.emplace_back(std::move(objectStatesByInterest));
    }
  }

  // send the response with the object states requested
  sendControlMessage(getRemoteAddress(msg.ownerId), std::move(responseMsg));
}

void RemoteParticipant::objectsStateResponse(const ObjectsStateResponse& msg)
{
  Lock lock(usageMutex_);

  for (const auto& [interestId, objectStates]: msg.responses)
  {
    ObjectAdditionList additions {};
    additions.reserve(objectStates.size());

    for (const auto& objectState: objectStates)
    {
      monitoredObjects_.setDynamicProperties(objectState);
      if (const auto* additionData = monitoredObjects_.getStaticState(objectState.id); additionData != nullptr)
      {
        additions.emplace_back(makeRemoteObjectDiscovery(*additionData, interestId));
      }
      else
      {
        additions.emplace_back(makeRemoteObjectDiscoveryFromProxy(objectState.id, interestId));
      }
    }

    incomingInterestsManager_.remoteObjectsAdded(interestId, std::move(additions));
  }
}

bool RemoteParticipant::tryResolvePendingClasses()
{
  Lock usageLock(usageMutex_);

  bool resolvedAnything = false;

  std::vector<u32> resolvedClassHashes;
  resolvedClassHashes.reserve(pendingClassSpecsList_.size());

  const auto& localTypes = session_->getKernel().getTypes();

  for (const auto& [classHash, spec, dependentTypes]: pendingClassSpecsList_)
  {
    bool anyDependencyMissing = false;

    for (const auto typeHash: dependentTypes)
    {
      if (!localTypes.get(typeHash) && !remoteTypesRegistry_.get(typeHash))
      {
        anyDependencyMissing = true;
        break;
      }
    }

    // we can register the class, notify the addition and remove from all pending lists
    if (!anyDependencyMissing)
    {
      resolvedAnything = true;
      logger_->debug("RP {}: remote class {} resolved", getDebugName(), spec.qualifiedName);

      registerRemoteType(spec);
      resolvedClassHashes.push_back(classHash);
      addOrRejectObjectsOfClass(classHash);
    }
  }

  for (const auto typeHash: resolvedClassHashes)
  {
    pendingAdditions_.erase(typeHash);
    pendingClassSpecsList_.erase(pendingClassSpecsMap_[typeHash]);
    pendingClassSpecsMap_.erase(typeHash);
  }

  return resolvedAnything;
}

void RemoteParticipant::addOrRejectObjectsOfClass(u32 typeHash)
{
  Lock usageLock(usageMutex_);

  // request the dynamic state of the pending additions once the missing types were received
  if (const auto& itr = pendingAdditions_.find(typeHash); itr != pendingAdditions_.end())
  {
    for (const auto& [ownerId, byInterest]: itr->second)
    {
      PublicationRejection rejectionsResponse;

      ObjectsStateRequest requestMsg {getId().get(), {}};
      requestMsg.requests.reserve(byInterest.size());

      for (const auto& [interestId, objectAddedList]: byInterest)
      {
        ObjectIdsByInterest objectsByInterest {interestId.get(), {}};
        objectsByInterest.objectIds.reserve(objectAddedList.size());

        for (const auto& object: objectAddedList)
        {
          processObjectAddition(
            objectsByInterest.objectIds, rejectionsResponse, object, interestId.get(), ownerId.get());
        }

        if (!objectsByInterest.objectIds.empty())
        {
          requestMsg.requests.emplace_back(std::move(objectsByInterest));
        }
      }

      if (!requestMsg.requests.empty())
      {
        // ask the dynamic state for the objects that we have accepted.
        sendControlMessage(getRemoteAddress(ownerId), std::move(requestMsg));
      }

      if (!rejectionsResponse.rejections.empty())
      {
        logger_->debug("RP {}: informing about rejections", getDebugName());
        sendControlMessage(getRemoteAddress(ownerId), std::move(rejectionsResponse));
      }
    }
  }
}

void RemoteParticipant::registerRemoteType(const CustomTypeSpec& remoteTypeSpec)
{
  const auto& nativeTypesRegistry = session_->getKernel().getTypes();

  // add the type to the non-native registry
  auto remoteType = buildNonNativeType(remoteTypeSpec, nativeTypesRegistry, remoteTypesRegistry_);
  remoteTypesStorage_.push_back(remoteType);
  remoteTypesRegistry_.add(remoteType);

  if (auto localType = nativeTypesRegistry.get(remoteTypeSpec.qualifiedName))
  {
    const auto runtimeCompatProblems = runtimeCompatible(localType.value().type(), remoteType.type());

    if (runtimeCompatProblems.empty())
    {
      logger_->debug("RP {} Detected different version of type {}. Will adapt their data models.",
                     getDebugName(),
                     remoteTypeSpec.qualifiedName);

      for (const auto& diff: getRuntimeDifferences(localType.value().type(), remoteType.type()))
      {
        logger_->debug(" - {}", diff);
      }

      return;
    }

    // store the incompatible classes to reject their objects upon reception
    logger_->warn(
      "RP {}: Found incompatibility problems with types used by a remote participant with process ID {}. The "
      "problems are the following:",
      getDebugName(),
      ownerAddress_.proc.get());

    for (const auto& problem: runtimeCompatProblems)
    {
      logger_->warn(" - {}", problem);
    }

    // store the incompatible type if it is a class
    if (remoteType->isClassType())
    {
      incompatibleClassHashes_.insert(remoteType->getHash().get());
    }
  }
}

void RemoteParticipant::registerRemoteTypeFoundInBus(ConstTypeHandle<CustomType> type, bool isIncompatible)
{
  // store the type in the remote registry
  remoteTypesStorage_.push_back(type);
  remoteTypesRegistry_.add(type);

  if (isIncompatible)
  {
    incompatibleClassHashes_.insert(type->getHash().get());
  }
}

std::function<std::shared_ptr<sen::impl::ProxyObject>(sen::impl::WorkQueue*, const std::string&, ObjectOwnerId)>
RemoteParticipant::createProxyMaker(const std::string& name,
                                    sen::ObjectId id,
                                    MaybeConstTypeHandle<ClassType> proxyClass,
                                    MaybeConstTypeHandle<ClassType> writerSchema)
{
  return [this, name, id, proxyClassType = proxyClass.value(), writerSchema](
           ::sen::impl::WorkQueue* workQueue, const std::string& localPrefix, ObjectOwnerId ownerId)
  {
    std::string localName;
    if (localPrefix.empty())
    {
      localName = name;
    }
    else
    {
      localName = localPrefix;
      localName.append(".");
      localName.append(name);
    }

    auto deletionFunc = [this](auto instance) { proxyAboutToBeDeleted(instance); };

    ::sen::impl::RemoteObjectInfo info {
      proxyClassType,
      name,
      id,
      workQueue,
      getProxyCallFunc(writerSchema.has_value() ? writerSchema->type() : proxyClassType.type()),
      localName,
      deletionFunc,
      ownerId,
      weak_from_this(),
      writerSchema};

    return startTrackingProxy(*proxyClassType, std::move(info));
  };
}

RemoteObjectDiscovery RemoteParticipant::makeRemoteObjectDiscovery(const ObjectAdded& addition, InterestId interestId)
{
  // try to get the local and remote classes from their respective registries
  const auto& localRegistry = session_->getKernel().getTypes();

  const auto localCustom = localRegistry.get(addition.className);
  const auto remoteCustom = remoteTypesRegistry_.get(addition.className);

  MaybeConstTypeHandle<> localClass = localCustom ? localCustom : std::nullopt;
  MaybeConstTypeHandle<> maybeRemoteClass = remoteCustom ? remoteCustom : std::nullopt;

  MaybeConstTypeHandle<ClassType> proxyClass;
  MaybeConstTypeHandle<ClassType> writerSchema;

  // throw error if the object's type is not a class in the remote registry
  if (!maybeRemoteClass && !localClass)
  {
    std::string err = "RP ";
    err.append(getDebugName());
    err.append(": could not create proxy for class ");
    err.append(addition.className);
    err.append(". Class could not be found in our types registries.");
    logger_->error(err);
    throwRuntimeError(err);
  }

  // the remote class is not known locally
  if (!localClass)
  {
    SEN_ASSERT(maybeRemoteClass.value()->isClassType() && "Remote needs to be a class type.");
    auto remoteClass = dynamicTypeHandleCast<const ClassType>(maybeRemoteClass.value()).value();

    // known parent object found in the remote hierarchy
    if (auto remoteParent = findClosestParent(remoteClass, localRegistry))
    {
      auto localParent = localRegistry.get(remoteParent.value()->getQualifiedName().data());
      SEN_ASSERT(localParent.has_value() && "Local parent was not found in the registry.");
      SEN_ASSERT(localParent.value()->isClassType() && "Local parent was not a class type.");

      proxyClass = dynamicTypeHandleCast<const ClassType>(localParent.value()).value();

      // the parents are equivalent
      if (equivalent(localParent.value().type(), remoteParent.value().type()))
      {
        writerSchema = std::nullopt;
      }
      // the parents are runtime compatible
      else
      {
        writerSchema = remoteParent;
      }
    }

    //  known remote not found in the remote hierarchy
    else
    {
      proxyClass = remoteClass;
      writerSchema = std::nullopt;
    }
  }

  // the remote class is known locally
  else
  {
    // the local class should have proxy makers
    if (!localClass.value()->asClassType()->hasProxyMakers())
    {
      std::string err = "RP ";
      err.append(getDebugName());
      err.append(": local class ");
      err.append(addition.className);
      err.append(". Does not have the expected proxy makers.");
      logger_->error(err);
      throw std::logic_error(err);
    }

    proxyClass = dynamicTypeHandleCast<const ClassType>(localClass.value()).value();

    // the local and remote types are equivalent
    if (localClass && maybeRemoteClass && equivalent(localClass->type(), maybeRemoteClass->type()))
    {
      writerSchema = std::nullopt;
    }
    // the local and remote types are runtime-compatible
    else
    {
      if (maybeRemoteClass)
      {
        SEN_ASSERT(maybeRemoteClass.value()->isClassType() && "Remote needs to be a class type.");
        writerSchema = dynamicTypeHandleCast<const ClassType>(maybeRemoteClass.value()).value();
      }
      else
      {
        writerSchema = std::nullopt;
      }
    }
  }

  SEN_ASSERT(proxyClass.has_value() && "We should have initiated/found a proxy class by now.");

  auto proxyMaker = createProxyMaker(addition.name, addition.id, proxyClass, writerSchema);

  return {
    addition.id, addition.name, std::move(proxyClass).value(), std::move(proxyMaker), interestId, ownerAddress_.id};
}

RemoteObjectDiscovery RemoteParticipant::makeRemoteObjectDiscoveryFromProxy(ObjectId objectId, InterestId interestId)
{
  if (const auto it = trackedProxies_.find(objectId); it != trackedProxies_.end())
  {
    auto proxy = it->second->front();
    auto proxyMaker = createProxyMaker(proxy->getName(), proxy->getId(), proxy->getClass(), proxy->getWriterSchema());

    return {objectId, proxy->getName(), proxy->getClass(), std::move(proxyMaker), interestId, ownerAddress_.id};
  }

  std::string err = "Could not make remote object discovery (interest ID: ";
  err.append(std::to_string(interestId.get()));
  err.append(" | objectId: ");
  err.append(std::to_string(objectId.get()));
  err.append(") from existing proxy");

  throwRuntimeError(std::move(err));
}

std::shared_ptr<::sen::impl::RemoteObject> RemoteParticipant::startTrackingProxy(const ClassType& proxyClass,
                                                                                 ::sen::impl::RemoteObjectInfo&& info)
{
  Lock lock(usageMutex_);

  std::shared_ptr<::sen::impl::RemoteObject> proxy = proxyClass.hasProxyMakers()
                                                       ? proxyClass.createRemoteProxy(std::move(info))
                                                       : std::make_shared<GenericRemoteObject>(std::move(info));

  RemoteObjectListPtr remotes;
  if (auto mapItr = trackedProxies_.find(proxy->getId()); mapItr == trackedProxies_.end())
  {
    auto [itr, done] = trackedProxies_.try_emplace(
      proxy->getId(), std::make_shared<std::vector<std::shared_ptr<::sen::impl::RemoteObject>>>());

    remotes = itr->second;
  }
  else
  {
    remotes = mapItr->second;
  }

  // if we already have a proxy for this object, we take the initial state out of it.
  // otherwise, we take it from what we have received so far
  if (!remotes->empty())
  {
    proxy->copyStateFrom(*remotes->front());
    monitoredObjects_.stopMonitoring(proxy->getId(), session_->getTransport());
  }
  else
  {
    // take the state that we were monitoring and stop the monitoring.
    monitoredObjects_.applyStateAndStopMonitoring(*proxy, session_->getTransport());
  }

  remotes->push_back(proxy);

  return proxy;
}

void RemoteParticipant::stopTrackingProxy(ObjectId objectId)
{
  RemoteObjectListPtr remotes;
  {
    Lock lock(usageMutex_);
    auto mapItr = trackedProxies_.find(objectId);
    if (mapItr == trackedProxies_.end())
    {
      // stop monitoring the object (the proxy might not have been created yet)
      monitoredObjects_.stopMonitoring(objectId, session_->getTransport());
      return;
    }
    remotes = mapItr->second;
  }

  // stop the proxy from calling us, since we will no longer exist
  for (auto& proxy: *remotes)
  {
    proxy->clearDestructionCallback();
  }

  {
    Lock lock(usageMutex_);
    trackedProxies_.erase(objectId);
    monitoredObjects_.stopMonitoring(objectId, session_->getTransport());
  }
}

void RemoteParticipant::proxyAboutToBeDeleted(::sen::impl::RemoteObject* proxy)
{
  const auto id = proxy->getId();

  RemoteObjectListPtr remotes;
  {
    Lock lock(usageMutex_);
    auto mapItr = trackedProxies_.find(id);
    if (mapItr == trackedProxies_.end())
    {
      // stop monitoring the object in case the proxy has not been tracked yet
      monitoredObjects_.stopMonitoring(id, session_->getTransport());
      return;
    }
    remotes = mapItr->second;
  }

  if (auto listItr =
        std::find_if(remotes->begin(), remotes->end(), [proxy](auto& elem) { return elem.get() == proxy; });
      listItr != remotes->end())
  {
    remotes->erase(listItr);
  }

  if (remotes->empty())
  {
    Lock lock(usageMutex_);
    trackedProxies_.erase(id);
  }

  {
    Lock lock(usageMutex_);
    monitoredObjects_.stopMonitoring(id, session_->getTransport());
  }
}

//--------------------------------------------------------------------------------------------------------------
// Sending
//--------------------------------------------------------------------------------------------------------------

void RemoteParticipant::sendToBus(ParticipantAddr& to, TransportMode mode, MemBlockPtr&& hdr, MemBlockPtr&& data)
{
  session_->getTransport().sendTo(to, busId_, mode, std::move(hdr), std::move(data));
}

void RemoteParticipant::sendToBus(ParticipantAddr& to, TransportMode mode, MemBlockPtr&& data)
{
  session_->getTransport().sendTo(to, busId_, mode, std::move(data));
}

template <typename T>
void RemoteParticipant::sendControlMessage(ParticipantAddr to, T&& message)
{
  if (!removed_)
  {
    sendToBus(to, TransportMode::confirmed, makeControlMessage(std::forward<T>(message)));
  }
}

//--------------------------------------------------------------------------------------------------------------
// RemoteParticipant header creation
//--------------------------------------------------------------------------------------------------------------

MemBlockPtr RemoteParticipant::makeObjectUpdateHdr(const NativeObject* object, uint32_t propertiesBufferSize) const
{
  auto hdr = smallBufferPool_->getBlockPtr();

  // pooled buffer - no cost of resizing
  ResizableBufferWriter writer(*hdr);
  OutputStream out(writer);

  out.writeUInt8(static_cast<uint8_t>(MessageCategory::runtimeObjectUpdate));
  out.writeUInt32(object->getId().get());
  out.writeTimestamp(object->getLastCommitTime());
  out.writeUInt32(propertiesBufferSize);

  return hdr;
}

MemBlockPtr RemoteParticipant::makeEventsHeader() const
{
  auto hdr = smallBufferPool_->getBlockPtr();

  ResizableBufferWriter writer(*hdr);
  OutputStream out(writer);
  out.writeUInt8(static_cast<uint8_t>(MessageCategory::runtimeEvents));

  return hdr;
}

MemBlockPtr RemoteParticipant::makeObjectNotFoundResponseHeader(ObjectId objectId, uint32_t ticketId) const
{
  auto hdr = smallBufferPool_->getBlockPtr();

  // pooled buffer - no cost of resizing
  ResizableBufferWriter writer(*hdr);
  OutputStream out(writer);

  out.writeUInt8(static_cast<uint8_t>(MessageCategory::runtimeMethodResponse));
  out.writeUInt8(static_cast<uint8_t>(::sen::impl::RemoteCallResult::objectNotFound));
  out.writeUInt32(objectId.get());
  out.writeUInt32(ticketId);

  return hdr;
}

MemBlockPtr RemoteParticipant::makeMethodResponseHeader(ObjectId objectId, uint32_t ticketId, uint32_t bufferSize) const
{
  // if we get here all went fine, write the header
  auto hdr = smallBufferPool_->getBlockPtr();

  // pooled buffer - no cost of resizing
  ResizableBufferWriter writer(*hdr);
  OutputStream out(writer);

  out.writeUInt8(static_cast<uint8_t>(MessageCategory::runtimeMethodResponse));
  out.writeUInt8(static_cast<uint8_t>(::sen::impl::RemoteCallResult::success));
  out.writeUInt32(objectId.get());
  out.writeUInt32(ticketId);
  out.writeUInt32(bufferSize);

  return hdr;
}

MemBlockPtr RemoteParticipant::makeRuntimeErrorResponseHeader(ObjectId objectId,
                                                              uint32_t ticketId,
                                                              const std::string& errorMsg)
{
  auto hdr = std::make_shared<ResizableHeapBlock>();

  // non-pooled buffer - higher cost of resizing, but exceptional case
  ResizableBufferWriter writer(*hdr);
  OutputStream out(writer);

  out.writeUInt8(static_cast<uint8_t>(MessageCategory::runtimeMethodResponse));
  out.writeUInt8(static_cast<uint8_t>(::sen::impl::RemoteCallResult::runtimeError));
  out.writeUInt32(objectId.get());
  out.writeUInt32(ticketId);
  out.writeString(errorMsg);

  return hdr;
}

MemBlockPtr RemoteParticipant::makeLogicErrorResponseHeader(ObjectId objectId,
                                                            uint32_t ticketId,
                                                            const std::string& errorMsg)
{
  auto hdr = std::make_shared<ResizableHeapBlock>();

  // non-pooled buffer - higher cost of resizing, but exceptional case
  ResizableBufferWriter writer(*hdr);
  OutputStream out(writer);

  out.writeUInt8(static_cast<uint8_t>(MessageCategory::runtimeMethodResponse));
  out.writeUInt8(static_cast<uint8_t>(::sen::impl::RemoteCallResult::logicError));
  out.writeUInt32(objectId.get());
  out.writeUInt32(ticketId);
  out.writeString(errorMsg);

  return hdr;
}

MemBlockPtr RemoteParticipant::makeUnknownErrorResponseHeader(ObjectId objectId, uint32_t ticketId) const
{
  auto hdr = smallBufferPool_->getBlockPtr();

  // pooled buffer - no cost of resizing
  ResizableBufferWriter writer(*hdr);
  OutputStream out(writer);

  out.writeUInt8(static_cast<uint8_t>(MessageCategory::runtimeMethodResponse));
  out.writeUInt8(static_cast<uint8_t>(::sen::impl::RemoteCallResult::unknownException));
  out.writeUInt32(objectId.get());
  out.writeUInt32(ticketId);

  return hdr;
}

MemBlockPtr RemoteParticipant::makeMethodCallHeader(TransportMode& mode,
                                                    ObjectOwnerId ownerId,
                                                    ObjectId objectId,
                                                    MemberHash methodId,
                                                    ::sen::impl::CallId callId,
                                                    uint32_t argsBufferSize,
                                                    const ClassType* classType) const
{
  const MessageCategory category = (mode == TransportMode::confirmed ? MessageCategory::runtimeMethodCallConfirmed
                                                                     : MessageCategory::runtimeMethodCallBestEffort);

  auto hdr = smallBufferPool_->getBlockPtr();

  // pooled buffer - no cost of resizing
  ResizableBufferWriter writer(*hdr);
  OutputStream out(writer);

  out.writeUInt8(static_cast<uint8_t>(category));
  out.writeUInt32(ownerId.get());
  out.writeUInt32(objectId.get());
  out.writeUInt32(methodId.get());
  out.writeUInt32(callId);
  out.writeUInt32(argsBufferSize);

  // if too large for best effort, change to confirmed mode
  if (const auto totalSize = hdr->size() + argsBufferSize;
      category == MessageCategory::runtimeMethodCallBestEffort && totalSize > maxBestEffortMessageSize)
  {
    logger_->warn(
      "Cannot send method call {} on object {} (class {}) over best effort - arguments are too long ({} "
      "bytes). "
      "Defaulting to confirmed transport",
      classType->searchMethodById(methodId)->getName(),
      objectId.get(),
      classType->getQualifiedName(),
      totalSize);

    mode = TransportMode::confirmed;
  }

  return hdr;
}

}  // namespace sen::kernel::impl
