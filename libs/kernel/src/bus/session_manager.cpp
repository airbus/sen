// === session_manager.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "bus/session_manager.h"

// implementation
#include "crash_reporter.h"
#include "kernel_impl.h"

// bus
#include "bus/session.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/memory_block.h"
#include "sen/core/meta/type.h"
#include "sen/core/obj/object_provider.h"
#include "sen/kernel/transport.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// spdlog
#include <spdlog/logger.h>  // NOLINT (misc-include-cleaner): indirectly used by log function

// std
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace sen::kernel::impl
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

/// Default implementation of a transport that does nothing
class EmptyTransport final: public Transport
{
  SEN_NOCOPY_NOMOVE(EmptyTransport)

public:
  explicit EmptyTransport(const std::string& sessionName): Transport(sessionName) {}

  ~EmptyTransport() override = default;

public:
  void start(TransportListener* listener) override { std::ignore = listener; }

  void sendTo(ParticipantAddr& to, BusId& busId, TransportMode mode, MemBlockPtr data) override
  {
    std::ignore = to;
    std::ignore = busId;
    std::ignore = mode;
    std::ignore = data;
  }

  void sendTo(ParticipantAddr& to, BusId& busId, TransportMode mode, MemBlockPtr data1, MemBlockPtr data2) override
  {
    std::ignore = to;
    std::ignore = busId;
    std::ignore = mode;
    std::ignore = data1;
    std::ignore = data2;
  }

  void localParticipantJoinedBus(ObjectOwnerId participant,            // NOSONAR
                                 BusId bus,                            // NOSONAR
                                 const std::string& busName) override  // NOSONAR
  {
    std::ignore = participant;
    std::ignore = bus;
    std::ignore = busName;
  }

  void localParticipantLeftBus(ObjectOwnerId participant, BusId bus, const std::string& busName) override
  {
    std::ignore = participant;
    std::ignore = bus;
    std::ignore = busName;
  }

  [[nodiscard]] const ProcessInfo& getOwnInfo() const noexcept override { return ownInfo_; }

  void stop() noexcept override
  {
    // no code needed
  }

  [[nodiscard]] TransportStats fetchStats() const override { return {}; }

  TimerId startTimer(std::chrono::steady_clock::duration timeout, std::function<void()>&& timeoutCallback) override
  {
    std::ignore = timeout;
    std::ignore = timeoutCallback;
    return 0ULL;
  }

  bool cancelTimer(TimerId id) override
  {
    std::ignore = id;
    return false;
  }

private:
  ProcessInfo ownInfo_ {};
};

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// SessionManager::DeletionGuard
//--------------------------------------------------------------------------------------------------------------

SessionManager::DeletionGuard::DeletionGuard(SessionManager* manager, Session* session)
  : session_(session), manager_(manager), lock_(manager->usageMutex_)
{
}

SessionManager::DeletionGuard::~DeletionGuard()
{
  try
  {
    manager_->sessionDeleted(session_);
  }
  catch (...)
  {
    // swallow exceptions
  }
}

//--------------------------------------------------------------------------------------------------------------
// SessionManager
//--------------------------------------------------------------------------------------------------------------

SessionManager::SessionManager(KernelImpl& kernel,
                               SessionCallback onSessionAvailable,
                               SessionCallback onSessionUnavailable)
  : kernel_(kernel)
  , onSessionAvailable_(std::move(onSessionAvailable))
  , onSessionUnavailable_(std::move(onSessionUnavailable))
  , logger_(KernelImpl::getKernelLogger())
  , messageDispatcher_([&kernel](std::string_view name) { return kernel.makeTracer(name); })
{
}

SessionManager::~SessionManager()
{
  Lock lock(localSessionsMutex_);
  // Shutdown all running sessions
  for (auto* elem: locallyOpenedSessions_)
  {
    elem->shutdownTransport();
  }
}

std::shared_ptr<Session> SessionManager::getOrOpenSession(const std::string& name)
{
  std::shared_ptr<Session> sessionPtr;

  {
    Lock lock(localSessionsMutex_);

    // check if it already exists
    for (auto* elem: locallyOpenedSessions_)
    {
      if (elem->getName() == name)
      {
        return elem->shared_from_this();
      }
    }

    // create the transport
    auto transport = name == "local" || factory_ == nullptr ? std::make_unique<EmptyTransport>(name)
                                                            : factory_(name, kernel_.makeTracer(name));

    // create the session
    sessionPtr = std::make_shared<Session>(this, name, std::move(transport), kernel_, messageDispatcher_);
    locallyOpenedSessions_.push_back(sessionPtr.get());
  }

  {
    Lock lock(remoteSessionsMutex_);

    // do not inform about its existence if we have already done so
    if (remotelyDetectedProcessesPerSession_.find(name) == remotelyDetectedProcessesPerSession_.end())
    {
      onSessionAvailable_(name);
    }
  }

  return sessionPtr;
}

void SessionManager::setTransportFactory(TransportFactory&& factory, uint32_t transportVersion)
{
  Lock usageLock(usageMutex_);
  factory_ = std::move(factory);
  CrashReporter::get().setTransportVersion(transportVersion);
}

SessionManager::DeletionGuard SessionManager::makeDeletionGuard(Session* session) { return {this, session}; }

void SessionManager::sessionDeleted(Session* session)
{
  logger_->debug("SessionManager: session {} deleted started", session->getName());

  {
    Lock lock(localSessionsMutex_);

    // remove it from the tracked sessions
    locallyOpenedSessions_.erase(std::remove(locallyOpenedSessions_.begin(), locallyOpenedSessions_.end(), session),
                                 locallyOpenedSessions_.end());
  }

  {
    Lock lock(remoteSessionsMutex_);

    // if no other process is in the session, notify that it has become unavailable
    if (const auto& name = session->getName();
        remotelyDetectedProcessesPerSession_.find(name) == remotelyDetectedProcessesPerSession_.end())
    {
      onSessionUnavailable_(name);
    }
  }

  logger_->debug("SessionManager: session {} deleted done", session->getName());
}

void SessionManager::remoteProcessDetected(const ProcessInfo& processInfo)
{
  logger_->debug("SessionManager: remote process detected (host: {} pid: {}) in session {}",
                 processInfo.hostName,
                 processInfo.processId,
                 processInfo.sessionName);

  Lock usageLock(remoteSessionsMutex_);

  // check if this session was not already known
  auto itr = remotelyDetectedProcessesPerSession_.find(processInfo.sessionName);
  if (itr == remotelyDetectedProcessesPerSession_.end())
  {
    // if not known before, add the process as the first one in the list
    remotelyDetectedProcessesPerSession_.insert({processInfo.sessionName, {processInfo}});

    // so we don't need to inform anyone about its existence if we already knew
    if (!isSessionLocallyOpen(processInfo.sessionName))
    {
      onSessionAvailable_(processInfo.sessionName);
    }
  }
  else
  {
    // it was already known, so we just need to add the process info to our list
    auto& processList = itr->second;
    auto procItr = std::find(processList.begin(), processList.end(), processInfo);
    if (procItr == processList.end())
    {
      processList.push_back(processInfo);
    }
  }
}

void SessionManager::remoteProcessLost(const ProcessInfo& processInfo)
{
  logger_->debug("SessionManager: remote process lost (host: {} pid: {}) in session {}",
                 processInfo.hostName,
                 processInfo.processId,
                 processInfo.sessionName);

  Lock usageLock(remoteSessionsMutex_);

  auto itr = remotelyDetectedProcessesPerSession_.find(processInfo.sessionName);
  if (itr != remotelyDetectedProcessesPerSession_.end())
  {
    auto& procList = itr->second;
    procList.erase(std::remove(procList.begin(), procList.end(), processInfo), procList.end());

    if (procList.empty())
    {
      remotelyDetectedProcessesPerSession_.erase(itr);

      if (!isSessionLocallyOpen(processInfo.sessionName))
      {
        onSessionUnavailable_(processInfo.sessionName);
      }
    }
  }
}

TransportStats SessionManager::fetchTransportStats() const
{
  Lock lock(localSessionsMutex_);

  TransportStats result {};
  for (const auto& session: locallyOpenedSessions_)
  {
    auto transportStats = session->getTransport().fetchStats();
    result.udpReceivedBytes += transportStats.udpReceivedBytes;
    result.tcpReceivedBytes += transportStats.tcpReceivedBytes;
    result.udpSentBytes += transportStats.udpSentBytes;
    result.tcpSentBytes += transportStats.tcpSentBytes;
  }

  return result;
}

bool SessionManager::isSessionLocallyOpen(std::string_view name) const
{
  Lock lock(localSessionsMutex_);
  return std::any_of(locallyOpenedSessions_.begin(),
                     locallyOpenedSessions_.end(),
                     [&name](const auto& elem) { return elem->getName() == name; });
}

}  // namespace sen::kernel::impl
