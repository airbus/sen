// === session.cpp =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "./session.h"

// implementation
#include "kernel_impl.h"
#include "message_dispatcher.h"
#include "session_manager.h"
#include "stl/sen/kernel/basic_types.stl.h"

// bus
#include "bus/bus.h"
#include "bus/local_participant.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/span.h"
#include "sen/core/obj/object_provider.h"
#include "sen/kernel/source_info.h"
#include "sen/kernel/transport.h"

// spdlog
#include <spdlog/logger.h>  // NOLINT (misc-include-cleaner): indirectly used by log function

// std
#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sen::kernel::impl
{

Session::Session(SessionManager* owner,
                 std::string name,
                 std::unique_ptr<Transport> transport,
                 KernelImpl& kernel,
                 MessageDispatcher& messageDispatcher)
  : owner_(owner)
  , name_(std::move(name))
  , messageDispatcher_(messageDispatcher)
  , transport_(std::move(transport))
  , kernel_(kernel)
  , logger_(KernelImpl::getKernelLogger())
{
  transport_->start(this);
}

Session::~Session()
{
  auto guard = owner_->makeDeletionGuard(this);

  transport_->stop();

  // close all open buses
  while (!buses_.empty())
  {
    closeBus(buses_.begin()->second.get());
  }
}

void Session::lock() { busesMutex_.lock(); }

void Session::unlock() noexcept { busesMutex_.unlock(); }

std::shared_ptr<SessionInfoProvider> Session::makeInfoProvider()
{
  // create and register the info provider
  auto result = std::make_shared<SessionInfoProvider>(name_, shared_from_this());
  {
    Lock busesLock(busesMutex_);

    // populate with the current buses
    for (const auto& [id, bus]: buses_)
    {
      result->addSource(bus->getAddress().busName);
    }

    infoProviders_.push_back(result.get());
  }
  return result;
}

void Session::infoProviderDeleted(SessionInfoProvider* infoProvider)
{
  Lock busesLock(busesMutex_);
  infoProviders_.erase(std::remove(infoProviders_.begin(), infoProviders_.end(), infoProvider), infoProviders_.end());
}

std::shared_ptr<Bus> Session::getOrOpenBus(const std::string& name)
{
  Lock busesLock(busesMutex_);

  const BusId busId(crc32(name));

  {
    auto itr = buses_.find(busId);
    if (itr != buses_.end())
    {
      return itr->second;
    }
  }

  auto [itr, done] = buses_.insert({busId, std::make_unique<Bus>(name, busId, this, kernel_.getTypes())});

  // notify info providers
  notifySourceAddedToInfoProviders(name);

  auto busPtr = itr->second;

  // notify the bus about remote participants that were reported to us
  if (auto pendingItr = detectedPendingRemotes_.find(busId); pendingItr != detectedPendingRemotes_.end())
  {
    for (const auto& remoteInfo: pendingItr->second)
    {
      busPtr->remoteParticipantJoined(remoteInfo.address, remoteInfo.processInfo);
    }

    // nothing else to report to this bus
    detectedPendingRemotes_.erase(pendingItr);
  }

  return busPtr;
}

void Session::closeBus(const Bus* bus)
{
  if (bus == nullptr)
  {
    return;
  }

  BusId busId = bus->getId();
  Lock busesLock(busesMutex_);

  // notify info providers
  notifySourceRemovedToInfoProviders(bus->getAddress().busName);

  buses_.erase(busId);
}

void Session::notifySourceAddedToInfoProviders(const std::string& name) const
{
  for (auto* elem: infoProviders_)
  {
    elem->addSource(name);
  }
}

void Session::notifySourceRemovedToInfoProviders(const std::string& name) const
{
  for (auto* elem: infoProviders_)
  {
    elem->removeSource(name);
  }
}

void Session::localParticipantJoinedBus(const LocalParticipant* participant, BusId bus)
{
  Lock busesLock(busesMutex_);

  auto itr = buses_.find(bus);

  // ensure the bus exists
  if (itr == buses_.end())
  {
    throwRuntimeError("trying to add a local participant to a non-existing bus");
  }

  transport_->localParticipantJoinedBus(participant->getId(), bus, itr->second->getAddress().busName);
}

void Session::localParticipantLeftBus(const LocalParticipant* participant, BusId bus)
{
  Lock busesLock(busesMutex_);

  auto busItr = buses_.find(bus);

  // only if the bus exists
  if (busItr != buses_.end())
  {
    transport_->localParticipantLeftBus(participant->getId(), bus, busItr->second->getAddress().busName);
  }
}

void Session::remoteProcessLost(ProcessId who)
{
  Lock busesLock(busesMutex_);

  // notify the buses
  for (const auto& [id, bus]: buses_)
  {
    bus->removeRemotesFromProcess(who);
  }

  // remove from pending remotes
  for (auto& [id, remoteList]: detectedPendingRemotes_)
  {
    remoteList.erase(
      std::remove_if(remoteList.begin(), remoteList.end(), [&](const auto& item) { return item.address.proc == who; }),
      remoteList.end());
  }

  // detect empty slots
  std::vector<BusId> emptySlots;
  emptySlots.reserve(detectedPendingRemotes_.size());
  for (const auto& [id, remoteList]: detectedPendingRemotes_)
  {
    if (remoteList.empty())
    {
      emptySlots.push_back(id);
    }
  }

  // erase empty slots
  while (!emptySlots.empty())
  {
    detectedPendingRemotes_.erase(emptySlots.back());
    emptySlots.pop_back();
  }
}

void Session::remoteParticipantJoinedBus(ParticipantAddr addr,
                                         const ProcessInfo& processInfo,
                                         BusId bus,
                                         std::string busName)
{
  messageDispatcher_.enqueueMessage(MessageDispatcher::Work(
    [this, addr, processInfo, bus, busName = std::move(busName)]() mutable
    {
      Lock busesLock(busesMutex_);

      // find the bus
      auto busItr = buses_.find(bus);
      if (busItr == buses_.end())
      {
        // save for later, when we connect to the bus
        detectedPendingRemotes_[bus].push_back({addr, processInfo});

        // notify the info providers about this bus (even if we don't have any connection to it)
        notifySourceAddedToInfoProviders(busName);
        return;
      }

      // create the object in the corresponding bus
      busItr->second->remoteParticipantJoined(addr, processInfo);
    },
    /*ensureNotDropped=*/true));
}

void Session::remoteParticipantLeftBus(ParticipantAddr addr, BusId bus, std::string busName)
{
  messageDispatcher_.enqueueMessage(MessageDispatcher::Work(
    [this, addr, bus, busName = std::move(busName)]() mutable
    {
      Lock busesLock(busesMutex_);

      // find the bus
      auto busItr = buses_.find(bus);
      if (busItr == buses_.end())
      {
        // if we were saving this for later, remove it from our memory
        if (auto pendingRemotesItr = detectedPendingRemotes_.find(bus);
            pendingRemotesItr != detectedPendingRemotes_.end())
        {
          auto& vec = pendingRemotesItr->second;

          vec.erase(
            std::remove_if(vec.begin(), vec.end(), [&](const auto& elem) -> bool { return elem.address == addr; }),
            vec.end());

          if (vec.empty())
          {
            detectedPendingRemotes_.erase(bus);

            // there is no more bus
            notifySourceRemovedToInfoProviders(busName);
          }
        }
        return;
      }

      // notify the bus
      busItr->second->remoteParticipantLeft(addr);
    },
    /*ensureNotDropped=*/true));
}

void Session::remoteMessageReceived(ObjectOwnerId to,
                                    BusId busId,
                                    Span<const uint8_t> msg,
                                    ByteBufferHandle buffer,
                                    bool ensureNotDropped)
{
  messageDispatcher_.enqueueMessage(MessageDispatcher::Work(
    [this, to, busId, msg, buffer = std::move(buffer)]() mutable
    {
      Lock busesLock(busesMutex_);

      if (auto itr = buses_.find(busId); itr != buses_.end())
      {
        itr->second->remoteMessageReceived(to, std::move(msg));
      }
      else
      {
        logger_->error("session received invalid bus ID {}", busId.get());
      }
    },
    ensureNotDropped));
}

void Session::remoteBroadcastMessageReceived(BusId busId, Span<const uint8_t> msg, ByteBufferHandle buffer)
{
  messageDispatcher_.enqueueMessage(MessageDispatcher::Work(
    [this, busId, msg, buffer = std::move(buffer)]() mutable
    {
      Lock busesLock(busesMutex_);

      auto itr = buses_.find(busId);
      if (SEN_LIKELY(itr != buses_.end()))
      {
        itr->second->remoteBroadcastMessageReceived(std::move(msg));
      }
      else
      {
        logger_->error("session received invalid (broadcasted) bus ID {}", busId.get());
      }
    },
    /*ensureNotDropped=*/false));
}

}  // namespace sen::kernel::impl
