// === session.h =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_BUS_SESSION_H
#define SEN_LIBS_KERNEL_SRC_BUS_SESSION_H

#include "./bus.h"
#include "bus/local_participant.h"
#include "message_dispatcher.h"
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/obj/object_provider.h"
#include "sen/kernel/source_info.h"
#include "sen/kernel/transport.h"
#include "stl/sen/kernel/basic_types.stl.h"

// spdlog
#include <spdlog/fwd.h>

// std
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace sen::kernel::impl
{

class SessionManager;
class RemoteParticipant;
class Bus;
class KernelImpl;

/// Internal representation of a session.
class Session final: public TransportListener, public std::enable_shared_from_this<Session>
{
public:
  SEN_NOCOPY_NOMOVE(Session)

public:
  Session(SessionManager* owner,
          std::string name,
          std::unique_ptr<Transport> transport,
          KernelImpl& kernel,
          MessageDispatcher& messageDispatcher);
  ~Session() override;

public:
  ByteBufferManager& getBufferManager() override { return messageDispatcher_.getByteBufferManager(); }

public:
  using PublicLock = std::scoped_lock<Session>;

  void lock();
  void unlock() noexcept;

public:
  /// The name of the session
  [[nodiscard]] const std::string& getName() const noexcept { return name_; }

  /// Gets or creates a bus that we can connect to.
  [[nodiscard]] std::shared_ptr<Bus> getOrOpenBus(const std::string& name);

  /// Closes a previously-open bus. Does nothing if the bus is already closed.
  void closeBus(const Bus* bus);

  /// The id of this process.
  [[nodiscard]] ProcessInfo getOwnProcessInfo() const { return transport_->getOwnInfo(); }

  /// Creates an info provider
  std::shared_ptr<SessionInfoProvider> makeInfoProvider();
  void infoProviderDeleted(SessionInfoProvider* infoProvider);

  /// Transport implementation
  [[nodiscard]] Transport& getTransport() noexcept { return *transport_; }

public:
  void localParticipantJoinedBus(const LocalParticipant* participant, BusId bus);
  void localParticipantLeftBus(const LocalParticipant* participant, BusId bus);

public:  // implements TransportListener
  void remoteProcessLost(ProcessId who) override;
  void remoteParticipantJoinedBus(ParticipantAddr addr,
                                  const ProcessInfo& processInfo,
                                  BusId bus,
                                  std::string busName) override;
  void remoteParticipantLeftBus(ParticipantAddr addr, BusId bus, std::string busName) override;
  void remoteMessageReceived(ObjectOwnerId to,
                             BusId busId,
                             Span<const uint8_t> msg,
                             ByteBufferHandle buffer,
                             bool ensureNotDropped) override;
  void remoteBroadcastMessageReceived(BusId busId, Span<const uint8_t> msg, ByteBufferHandle buffer) override;

private:
  friend class RemoteParticipant;
  [[nodiscard]] KernelImpl& getKernel() noexcept { return kernel_; }

private:
  void notifySourceAddedToInfoProviders(const std::string& name) const;
  void notifySourceRemovedToInfoProviders(const std::string& name) const;

private:
  using Lock = std::scoped_lock<std::recursive_mutex>;

private:
  struct RemoteParticipantInfo
  {
    ParticipantAddr address;
    ProcessInfo processInfo;
  };

private:
  friend SessionManager;
  void shutdownTransport() { transport_->stopIO(); }

private:
  SessionManager* owner_;
  std::string name_;
  MessageDispatcher& messageDispatcher_;
  std::unique_ptr<Transport> transport_;
  std::unordered_map<BusId, std::shared_ptr<Bus>> buses_;
  std::unordered_map<BusId, std::vector<RemoteParticipantInfo>> detectedPendingRemotes_;
  mutable std::recursive_mutex busesMutex_;
  KernelImpl& kernel_;
  std::vector<SessionInfoProvider*> infoProviders_;
  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_BUS_SESSION_H
