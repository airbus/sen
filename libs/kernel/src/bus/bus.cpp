// === bus.cpp =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "bus.h"

// implementation
#include "session.h"

// bus
#include "bus/local_participant.h"
#include "bus/remote_participant.h"
#include "bus/util.h"
#include "bus_protocol.stl.h"

// kernel
#include "kernel_impl.h"

// sen
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_provider.h"
#include "sen/kernel/transport.h"
#include "sen/kernel/type_specs_utils.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// spdlog
#include <spdlog/logger.h>  // NOLINT (misc-include-cleaner): indirectly used by log function

// std
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sen::kernel::impl
{

Bus::Bus(std::string_view name, BusId id, Session* session, CustomTypeRegistry& localTypes)
  : address_(BusAddress {session->getName(), std::string(name)})
  , id_(id)
  , session_(session)
  , localTypes_(localTypes)
  , logger_(KernelImpl::getKernelLogger())
{
}

Bus::~Bus()
{
  Lock lock(remotesMutex_);
  remotes_.clear();
}

void Bus::connect(LocalParticipant* participant)
{
  logger_->debug(
    "Bus {}.{}: local participant {} connected", address_.sessionName, address_.busName, participant->getDebugName());
  {
    Lock lock(localsMutex_);

    // let the new participant know about the existing ones
    // and the existing locals know about the new one
    for (auto* local: locals_)
    {
      participant->localParticipantAdded(local, true);
      local->localParticipantAdded(participant, true);
    }

    // store it
    locals_.push_back(participant);
  }

  // notify other participants about the fact
  session_->localParticipantJoinedBus(participant, id_);

  {
    Lock participantsLock(remotesMutex_);

    // let the new local know about the existing remotes
    for (const auto& [id, remote]: remotes_)
    {
      participant->remoteParticipantAdded(remote.get(), true);
    }
  }
}

void Bus::disconnect(LocalParticipant* participant)
{
  logger_->debug("Bus {}.{}: local participant {} disconnected",
                 address_.sessionName,
                 address_.busName,
                 participant->getDebugName());

  // notify remote participants about its departure
  session_->localParticipantLeftBus(participant, id_);

  Lock lock(localsMutex_);

  // notify existing local participants about its departure
  for (auto* local: locals_)
  {
    if (local != participant)
    {
      local->localParticipantRemoved(participant, true);
    }
  }

  // remove it from storage
  removeIfPresent<LocalParticipant*>(participant, locals_);
}

void Bus::remoteParticipantJoined(ParticipantAddr address, ProcessInfo processInfo)
{
  logger_->debug("Bus {}.{}: remote participant {} joined", address_.sessionName, address_.busName, address.id.get());

  RemoteParticipant* remote = nullptr;
  {
    Lock lock(remotesMutex_);

    // do not add multiple instances of the same remote
    if (remotes_.find(address.id.get()) != remotes_.end())
    {
      return;
    }

    auto [itr, done] =
      remotes_.insert({address.id, std::make_unique<RemoteParticipant>(processInfo, address, this, session_)});

    remote = itr->second.get();
  }

  {
    Lock lock(localsMutex_);

    // notify the remote about our local types
    for (auto* local: locals_)
    {
      local->remoteParticipantAdded(remote, true);
    }
  }
}

void Bus::remoteParticipantLeft(const ParticipantAddr& addr)
{
  logger_->debug("Bus {}.{}: remote participant {} left", address_.sessionName, address_.busName, addr.id.get());

  Lock remotesLock(remotesMutex_);

  for (const auto& [ownerId, remote]: remotes_)
  {
    if (remote->getAddress() == addr)
    {
      // prevent this remote participant object from sending messages back to the remote process
      remote->markRemoved();
      remotes_.erase(ownerId);
      return;
    }
  }
}

void Bus::remoteParticipantRemoved(RemoteParticipant* remote)
{
  Lock localsLock(localsMutex_);
  for (auto* local: locals_)
  {
    local->remoteParticipantRemoved(remote, true);
  }
}

void Bus::removeRemotesFromProcess(ProcessId processId)
{
  Lock lock(remotesMutex_);

  decltype(remotes_) remaining;

  for (auto& [id, participant]: remotes_)
  {
    if (participant->getAddress().proc != processId)
    {
      remaining.insert({id, std::move(participant)});
    }
  }

  remotes_ = std::move(remaining);
}

void Bus::remoteMessageReceived(ObjectOwnerId to, Span<const uint8_t> msg)
{
  Lock lock(remotesMutex_);
  if (auto remotesItr = remotes_.find(to); remotesItr != remotes_.end())
  {
    InputStream in(msg);

    // read the header
    uint8_t categoryVal = 0;
    in.readUInt8(categoryVal);

    sendMessageToParticipant(static_cast<MessageCategory>(categoryVal), in, *(*remotesItr).second);
  }
  else
  {
    logger_->debug("Bus {}.{}: remote message lost");
  }
}

void Bus::remoteBroadcastMessageReceived(Span<const uint8_t> msg)
{
  InputStream in(msg);

  // read the header
  uint8_t categoryVal = 0;
  in.readUInt8(categoryVal);
  const auto category = static_cast<MessageCategory>(categoryVal);

  // notify all the remote participants that we received a message
  {
    Lock lock(remotesMutex_);
    for (const auto& [ownerId, participant]: remotes_)
    {
      InputStream inCopy(in);
      sendMessageToParticipant(category, inCopy, *participant);
    }
  }
}

ObjectUpdateMessage Bus::readObjectUpdateData(InputStream& in)
{
  uint32_t objectId = 0U;
  TimeStamp mostRecentCommitTime;
  uint32_t propertiesSize = 0U;

  in.readUInt32(objectId);
  in.readTimeStamp(mostRecentCommitTime);
  in.readUInt32(propertiesSize);

  return {objectId, mostRecentCommitTime, makeConstSpan(in.advance(propertiesSize), propertiesSize)};
}

void Bus::sendMessageToParticipant(MessageCategory category, InputStream& in, RemoteParticipant& participant)
{
  // read the payload depending on the category
  switch (category)
  {
    case MessageCategory::controlMessage:
      participant.rcvControlMessage(in);
      break;

    case MessageCategory::runtimeObjectUpdate:
      participant.rcvObjectUpdate(readObjectUpdateData(in));
      break;

    case MessageCategory::runtimeMethodCallBestEffort:
      participant.rcvMethodCall(TransportMode::unicast, in);
      break;

    case MessageCategory::runtimeMethodCallConfirmed:
      participant.rcvMethodCall(TransportMode::confirmed, in);
      break;

    case MessageCategory::runtimeMethodResponse:
      participant.rcvMethodResponse(in);
      break;

    case MessageCategory::runtimeEvents:
      participant.rcvEvents(in);
      break;

    default:
      logger_->error("unknown message received");
      break;
  }
}

void Bus::remoteSubscriberAdded(std::shared_ptr<Interest> interest,
                                RemoteParticipant* listener,
                                bool notifyAboutExisting)
{
  Lock lock(localsMutex_);
  for (auto* participant: locals_)
  {
    participant->remoteSubscriberAdded(interest, listener, notifyAboutExisting);
  }
}

void Bus::remoteSubscriberRemoved(std::shared_ptr<Interest> interest,
                                  RemoteParticipant* listener,
                                  bool notifyAboutExisting)
{
  Lock lock(localsMutex_);
  for (auto* participant: locals_)
  {
    participant->remoteSubscriberRemoved(interest, listener, notifyAboutExisting);
  }
}

void Bus::remoteSubscriberRemoved(RemoteParticipant* listener, bool notifyAboutExisting)
{
  Lock lock(localsMutex_);
  for (auto* participant: locals_)
  {
    participant->remoteSubscriberRemoved(listener, notifyAboutExisting);
  }
}

void Bus::publicationsRejected(const PublicationRejection& rejections, RemoteParticipant* whom)
{
  Lock lock(localsMutex_);
  for (auto* participant: locals_)
  {
    if (participant->handleRejectedPublications(rejections, whom))
    {
      return;
    }
  }
}

std::pair<MaybeConstTypeHandle<>, bool> Bus::searchForTypeInRemoteParticipants(const RemoteParticipant* requestor,
                                                                               uint32_t typeHash)
{
  Lock lock(remotesMutex_);
  for (const auto& [ownerId, participant]: remotes_)
  {
    if (participant.get() != requestor)
    {
      if (const auto& [typePtr, isIncompatible] = participant->searchForRemoteType(typeHash); typePtr)
      {
        return {typePtr, isIncompatible};
      }
    }
  }

  return {std::nullopt, false};
}

std::optional<TimeStamp> Bus::isObjectNameUsedLocally(std::string_view name)
{
  Lock lock(localsMutex_);
  for (auto* participant: locals_)
  {
    if (auto timestamp = participant->objectNameUsed(name); timestamp.has_value())
    {
      return timestamp;
    }
  }
  return std::nullopt;
}

bool Bus::updateLocalTypesSpecs()
{
  auto typesInOrder = localTypes_.getAllInOrder();

  std::vector<const CustomType*> typesToSerialize;
  typesToSerialize.reserve(typesInOrder.size());

  for (const auto& type: typesInOrder)
  {
    // ignore native quantities (these are not really custom types)
    if (type->isTimestampType() || type->isDurationType())
    {
      continue;
    }

    auto alreadySerialized =
      std::find_if(localTypesSpecs_.begin(),
                   localTypesSpecs_.end(),
                   [&](const auto& elem) { return elem.qualifiedName == type->getQualifiedName(); });

    // if not serialized, serialize it
    if (alreadySerialized == localTypesSpecs_.end())
    {
      typesToSerialize.emplace_back(type);
    }
  }

  // return if there's no type to send
  if (typesToSerialize.empty())
  {
    return false;
  }

  for (const auto& type: typesToSerialize)
  {
    localTypesSpecs_.emplace_back(makeCustomTypeSpec(type));
  }

  return true;
}

}  // namespace sen::kernel::impl
