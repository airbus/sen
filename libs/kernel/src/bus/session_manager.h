// === session_manager.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_BUS_SESSION_MANAGER_H
#define SEN_LIBS_KERNEL_SRC_BUS_SESSION_MANAGER_H

#include "session.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/kernel/transport.h"

// spdglog
#include <spdlog/fwd.h>

// std
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace sen::kernel::impl
{

/// Knows all open sessions and can be used to open/get handlers to them.
class SessionManager
{
public:
  SEN_NOCOPY_NOMOVE(SessionManager)

public:
  using SessionCallback = std::function<void(const std::string&)>;

public:
  SessionManager(KernelImpl& kernel, SessionCallback onSessionAvailable, SessionCallback onSessionUnavailable);
  ~SessionManager();

public:
  /// Gets or creates a session that we can connect to.
  [[nodiscard]] std::shared_ptr<Session> getOrOpenSession(const std::string& name);

  /// Sets the function to create session transports
  void setTransportFactory(TransportFactory&& factory, uint32_t transportVersion);

  /// Informs the session manager about a remote process connected to some session
  void remoteProcessDetected(const ProcessInfo& processInfo);

  /// Informs the session manager about a remote process that is no longer connected to a session
  void remoteProcessLost(const ProcessInfo& processInfo);

  /// Collects transport statistics
  TransportStats fetchTransportStats() const;

  void startMessageProcessing() { messageDispatcher_.start(); }

private:  // interface towards Session
  friend class Session;

  class DeletionGuard
  {
    SEN_NOCOPY_NOMOVE(DeletionGuard)

  public:
    DeletionGuard(SessionManager* manager, Session* session);
    ~DeletionGuard();

  private:
    Session* session_;
    SessionManager* manager_;
    std::scoped_lock<std::recursive_mutex> lock_;
  };

  DeletionGuard makeDeletionGuard(Session* session);

private:
  friend class DeletionGuard;
  void sessionDeleted(Session* session);
  [[nodiscard]] bool isSessionLocallyOpen(std::string_view name) const;

private:
  using Lock = std::scoped_lock<std::recursive_mutex>;

private:
  mutable std::recursive_mutex usageMutex_;
  mutable std::recursive_mutex localSessionsMutex_;
  mutable std::recursive_mutex remoteSessionsMutex_;
  TransportFactory factory_;
  std::vector<Session*> locallyOpenedSessions_;
  std::unordered_map<std::string, std::vector<ProcessInfo>> remotelyDetectedProcessesPerSession_;
  KernelImpl& kernel_;
  SessionCallback onSessionAvailable_;
  SessionCallback onSessionUnavailable_;
  std::shared_ptr<spdlog::logger> logger_;
  TransportListener::ByteBufferManager bufferManager_;
  MessageDispatcher messageDispatcher_;
};

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_BUS_SESSION_MANAGER_H
