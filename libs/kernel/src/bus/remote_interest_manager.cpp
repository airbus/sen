// === remote_interest_manager.cpp =====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "remote_interest_manager.h"

// implementation
#include "bus/containers.h"
#include "remote_participant.h"
#include "runner.h"
#include "util.h"

// sen
#include "sen/core/base/memory_block.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/type.h"
#include "sen/core/obj/detail/event_buffer.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_filter.h"
#include "sen/core/obj/object_provider.h"
#include "sen/kernel/tracer.h"
#include "sen/kernel/transport.h"

// spdlog
#include <spdlog/logger.h>

// std
#include <list>
#include <memory>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace sen::kernel::impl
{

//---------------------------------------------------------------------------------------------//
// Helpers
//---------------------------------------------------------------------------------------------//

namespace
{

template <typename Pool>
inline void writeEventToBestEffortStream(Pool& pool,
                                         const ::sen::impl::SerializableEvent& serializableEvent,
                                         BestEffortBlockPtr& currentBuffer,
                                         BestEffortBufferList& bufferList)
{
  const auto maxSize = getMaxEventSerializedSize(serializableEvent);
  auto writer = getBestEffortBuffer(pool, currentBuffer, bufferList, maxSize);

  OutputStream out(writer);
  writeEventToStream(out, serializableEvent);
}

}  // namespace

//---------------------------------------------------------------------------------------------//
// RemoteInterestsManager
//---------------------------------------------------------------------------------------------//

RemoteInterestsManager::RemoteInterestsManager(ObjectOwnerId ownerIdRef, LocalParticipant* owner)
  : ObjectFilter(ownerIdRef), owner_(owner), bestEffortPool_(BestEffortPool::make())
{
  interestsHandler_.onInterestRemoved(
    [this](InterestId interestId)
    {
      owner_->getLogger().debug("Interest {} removed", interestId.get());

      auto& interestsUpdatesBMMap = remoteInterestsHandler_.interestsUpdatesBMMap;
      const auto& orphanUpdates = interestsUpdatesBMMap.remove(interestId).second;
      for (auto* update: orphanUpdates)
      {
        update->applyToNativeObject([&](auto* localObject) { removeObjectById(localObject->getId()); });
      }
    });
}

RemoteInterestsManager::~RemoteInterestsManager()
{
  objectUpdates_.clear();  // clear allocated blocks before destroying the pool
  objectIdToUpdateItr_.clear();
  updatesObjectIdsBiMap_.clear();
}

void RemoteInterestsManager::sendToRelevantRemoteParticipants(const std::list<::sen::impl::SerializableEvent>& events)
{
  sendObjectUpdates();
  sendEvents(events);
}

void RemoteInterestsManager::markObjectAsRejected(ObjectId objectId, ObjectProviderListener* listener)
{
  if (auto* remote = listener->isRemoteParticipant(); remote != nullptr)
  {
    remoteInterestsHandler_.remotesUpdatesBMMap.remove(remote, &*objectIdToUpdateItr_[objectId]);
  }
}

inline void RemoteInterestsManager::sendEvents(const std::list<::sen::impl::SerializableEvent>& events)
{
  SEN_TRACE_ZONE(owner_->getOwner()->getTracer());

  // early exit
  if (events.empty() || aRemoteParticipant_ == nullptr)
  {
    return;
  }

  struct RemoteData
  {
    std::shared_ptr<ResizableHeapBlock> confirmedBuffer = std::make_shared<ResizableHeapBlock>();
    BestEffortBufferList unicastBuffers;
    BestEffortBlockPtr unicastBuffer;
  };

  BestEffortBufferList multicastBuffers;
  BestEffortBlockPtr multicastBuffer;

  std::unordered_map<RemoteParticipant*, RemoteData> remoteDataMap;

  // collect all the data to be sent
  for (const auto& element: events)
  {
    const auto* const updatePtr = updatesObjectIdsBiMap_.tryGet(element.producerId);  // NOSONAR

    // discard the event if we don't have an update for it
    if (updatePtr == nullptr)
    {
      continue;
    }

    // get the update
    auto* update = *updatePtr;

    // tackle direct communication
    if (element.transportMode == TransportMode::confirmed || element.transportMode == TransportMode::unicast)
    {
      remoteInterestsHandler_.remotesUpdatesBMMap.forEach(
        update,
        [this, &remoteDataMap, &element](RemoteParticipant* remote)
        {
          auto& remoteData = remoteDataMap[remote];

          if (element.transportMode == TransportMode::confirmed)
          {
            // compute the size the event will take
            auto maxEventSize = getMaxEventSerializedSize(element);

            // reserve space for potential resizes
            remoteData.confirmedBuffer->reserve(remoteData.confirmedBuffer->size() + maxEventSize);

            // write the event - ResizableBufferWriter as we already reserved enough space
            ResizableBufferWriter confirmedWriter(*remoteData.confirmedBuffer);
            OutputStream reliableStream(confirmedWriter);
            writeEventToStream(reliableStream, element);
          }
          else
          {
            // unicast
            writeEventToBestEffortStream(
              *bestEffortPool_, element, remoteData.unicastBuffer, remoteData.unicastBuffers);
          }
        });
    }
    else
    {
      // multicast
      writeEventToBestEffortStream(*bestEffortPool_, element, multicastBuffer, multicastBuffers);
    }
  }

  const auto& ownerId = getOwnerId();

  // use the first remote to send the multicast events to all
  if (!multicastBuffers.empty())
  {
    aRemoteParticipant_->sendEventsMulticast(std::move(multicastBuffers), ownerId);
  }

  for (auto& [participant, data]: remoteDataMap)
  {
    if (!data.unicastBuffers.empty())
    {
      participant->sendEventsUnicast(std::move(data.unicastBuffers), ownerId);
    }

    if (!data.confirmedBuffer->empty())
    {
      participant->sendEventsConfirmed(data.confirmedBuffer, ownerId);
    }
  }
}

void RemoteInterestsManager::sendObjectUpdates()
{
  SEN_TRACE_ZONE(owner_->getOwner()->getTracer());

  // only collect updates if there's at least one remote
  if (aRemoteParticipant_ == nullptr)
  {
    return;
  }

  {
    std::lock_guard lock(objectsMutex_);
    // serialize the objects that require updates
    for (auto& update: objectUpdates_)
    {
      update.collect(*bestEffortPool_);
    }
  }

  {
    const std::lock_guard lock(remoteInterestsHandler_.remotesUpdatesBMMapMutex_);
    remoteInterestsHandler_.remotesUpdatesBMMap.forEach(
      [ownerId = getOwnerId()](ObjectUpdate* update, const List<RemoteParticipant*>& remotes)
      {
        std::unordered_set<ProcessId> used;
        used.reserve(remotes.size());

        for (auto* remote: remotes)
        {
          if (auto proc = remote->getAddress().proc; used.find(proc) == used.end())
          {
            remote->sendObjectUpdate(ownerId, update);
            used.insert(proc);
          }
        }
      });
  }
}

void RemoteInterestsManager::subscriberAdded(std::shared_ptr<Interest> interest,
                                             ObjectProviderListener* listener,
                                             bool notifyAboutExisting)
{
  std::ignore = notifyAboutExisting;

  const auto interestId = interest->getId();
  bool isANewInterest = !interestsHandler_.isRegistered(interestId);
  interestsHandler_.addInterest(interestId);

  auto* remote = listener->isRemoteParticipant();
  if (remote != nullptr)
  {
    owner_->getLogger().debug("LP {}: remote participant {} subscribed to interest {}",
                              owner_->getDebugName(),
                              remote->getId().get(),
                              interest->getId().get());

    // add to the interest list of participants
    {
      aRemoteParticipant_ = remote;
    }
    remoteInterestsHandler_.addSubscriber(interestId, remote, isANewInterest);
  }
}

void RemoteInterestsManager::subscriberRemoved(std::shared_ptr<Interest> interest,
                                               ObjectProviderListener* listener,
                                               bool notifyAboutExisting)
{
  std::ignore = notifyAboutExisting;

  const auto interestId = interest->getId();
  interestsHandler_.removeInterest(interestId);

  auto* remote = listener->isRemoteParticipant();
  if (remote == nullptr)
  {
    return;
  }

  owner_->getLogger().debug("LP {}: remote participant {} unsubscribed from interest {}",
                            owner_->getDebugName(),
                            remote->getId().get(),
                            interestId.get());

  std::ignore = remoteInterestsHandler_.removeSubscriber(interestId, remote);

  {
    // update the reference
    if (aRemoteParticipant_ == remote)
    {
      aRemoteParticipant_ = nullptr;
      updateRemoteReference();
    }
  }
}

void RemoteInterestsManager::subscriberRemoved(ObjectProviderListener* listener, bool notifyAboutExisting)
{
  std::ignore = notifyAboutExisting;
  auto* remote = listener->isRemoteParticipant();
  if (remote == nullptr)
  {
    return;
  }

  owner_->getLogger().debug("LP {}: remote participant {} unsubscribed", owner_->getDebugName(), remote->getId().get());

  const auto& orphanInterests = remoteInterestsHandler_.removeSubscriber(remote);
  for (auto interestId: orphanInterests)
  {
    interestsHandler_.removeInterest(interestId);
  }

  // update the reference
  if (aRemoteParticipant_ == remote)
  {
    aRemoteParticipant_ = nullptr;
    updateRemoteReference();
  }
}

void RemoteInterestsManager::updateRemoteReference()
{
  const std::lock_guard lock(remoteInterestsHandler_.remotesUpdatesBMMapMutex_);
  // try to find another
  const auto& remotesInterestsBMMap = remoteInterestsHandler_.remotesInterestsBMMap;
  for (auto itr = remotesInterestsBMMap.begin<InterestId>();  // NOLINT(modernize-loop-convert) NOSONAR
       itr != remotesInterestsBMMap.end<InterestId>();
       ++itr)
  {
    if (!itr->second.empty())
    {
      aRemoteParticipant_ = itr->second.front();
      break;
    }
  }
}

void RemoteInterestsManager::objectsAdded(std::shared_ptr<Interest> interest, ObjectAdditionList additions)
{
  const auto interestId = interest->getId();

  for (const auto& addition: additions)
  {
    auto* instance = getObjectInstance(addition);
    if (instance == nullptr)
    {
      continue;
    }

    auto* local = instance->asNativeObject();
    if (local == nullptr)
    {
      continue;
    }

    owner_->getLogger().debug("Object added {}: with interest {} ", local->getName(), interest->getId().get());

    {
      std::lock_guard lock(objectsMutex_);

      // create new object update if a new object is added
      ObjectUpdate* updatePtr;
      if (auto itr = objectIdToUpdateItr_.find(local->getId()); itr == objectIdToUpdateItr_.end())
      {
        auto updateItr = objectUpdates_.insert(objectUpdates_.end(), ObjectUpdate {local});
        updatePtr = &(*updateItr);
        updatesObjectIdsBiMap_.add(updatePtr, local->getId());
        objectIdToUpdateItr_.try_emplace(local->getId(), updateItr);
      }
      else
      {
        updatePtr = &(*itr->second);
      }

      // add update to interested remote participants
      remoteInterestsHandler_.addObject(interestId, updatePtr);
    }
  }
}

void RemoteInterestsManager::objectsRemoved(std::shared_ptr<Interest> interest, ObjectRemovalList removals)
{
  std::ignore = interest;
  for (const auto& removal: removals)
  {
    removeObjectById(removal.objectid);
  }
}

void RemoteInterestsManager::removeObjectById(const ObjectId& objectId)
{
  auto updateItr = objectIdToUpdateItr_.find(objectId);
  if (updateItr == objectIdToUpdateItr_.end())
  {
    return;
  }

  auto* updatePtr = &(*updateItr->second);

  remoteInterestsHandler_.removeObject(updatePtr);

  objectUpdates_.erase(updateItr->second);
  updatesObjectIdsBiMap_.remove(objectId);
  objectIdToUpdateItr_.erase(updateItr);
}

}  // namespace sen::kernel::impl
