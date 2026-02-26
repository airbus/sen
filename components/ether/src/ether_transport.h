// === ether_transport.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_ETHER_TRANSPORT_H
#define SEN_COMPONENTS_ETHER_SRC_ETHER_TRANSPORT_H

// component
#include "bus_handler.h"
#include "discovery.h"
#include "process_handler.h"
#include "util.h"

// sen
#include "sen/kernel/tracer.h"
#include "sen/kernel/transport.h"

// generated code
#include "stl/configuration.stl.h"

// std
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace sen::components::ether
{

class EtherTransport final: public kernel::Transport, public DiscoveryEventListener
{
  SEN_NOCOPY_NOMOVE(EtherTransport)

public:
  EtherTransport(const Configuration& config,
                 std::string_view sessionName,
                 std::string_view appName,
                 std::shared_ptr<DiscoverySystem> discovery,
                 std::unique_ptr<sen::kernel::Tracer> tracer);
  ~EtherTransport() override;

public:
  [[nodiscard]] const Configuration& getConfig() const noexcept;

public:  // implements Transport
  [[nodiscard]] const kernel::ProcessInfo& getOwnInfo() const noexcept override;
  void start(kernel::TransportListener* listener) override;
  void localParticipantJoinedBus(ObjectOwnerId participant, kernel::BusId bus, const std::string& busName) override;
  void localParticipantLeftBus(ObjectOwnerId participant, kernel::BusId bus, const std::string& busName) override;
  void sendTo(kernel::ParticipantAddr& to, kernel::BusId& busId, TransportMode mode, MemBlockPtr data) override;
  void sendTo(kernel::ParticipantAddr& to,
              kernel::BusId& busId,
              TransportMode mode,
              MemBlockPtr data1,
              MemBlockPtr data2) override;
  void stop() noexcept override;
  kernel::TransportStats fetchStats() const override;

  [[nodiscard]] TimerId startTimer(std::chrono::steady_clock::duration timeout,
                                   std::function<void()>&& timeoutCallback) override;
  [[nodiscard]] bool cancelTimer(TimerId id) override;

public:  // implements DiscoveryEventListener
  void beamFound(const BeamInfo& info) override;
  void beamLost(const kernel::ProcessInfo& processInfo) override;

private:
  friend class ProcessHandler;
  friend class Acceptor;
  void processConnected(asio::ip::tcp::socket socket);
  void processDisconnected(ProcessHandler* process);
  void detectedNewProcess(const BeamInfo& beam);
  void lostExistingProcess(const kernel::ProcessInfo& info);
  kernel::ProcessId processReady(ProcessHandler* process);
  [[nodiscard]] BusHandler* getHandler(kernel::BusId& busId);
  [[nodiscard]] ProcessHandler* getHandler(kernel::ParticipantAddr& participantAddr);

private:
  using Lock = std::scoped_lock<std::recursive_mutex>;

  void stopIO() noexcept override
  {
    std::lock_guard lock(ioMutex_);
    io_->stop();
  }

private:  // timers
  /// Stores an asynchronous steady timer and its expiration callback
  struct TimerData
  {
    // NOLINTBEGIN(readability-identifier-naming)
    explicit TimerData(asio::io_context& _ioContext, std::function<void()>&& _timeoutCallback)
      : timer(_ioContext), timeoutCallback(std::move(_timeoutCallback))
    {
    }
    // NOLINTEND(readability-identifier-naming)

    asio::steady_timer timer;               // NOLINT(misc-non-private-member-variables-in-classes)
    std::function<void()> timeoutCallback;  // NOLINT(misc-non-private-member-variables-in-classes)
  };

private:
  const Configuration& config_;
  uint32_t sessionId_;
  kernel::ProcessInfo ownInfo_ {};
  std::shared_ptr<DiscoverySystem> discovery_;
  kernel::TransportListener* listener_ = nullptr;
  std::vector<std::shared_ptr<ProcessHandler>> processes_;
  std::unordered_map<kernel::ProcessId, ProcessHandler*> readyProcesses_;
  std::unordered_map<kernel::BusId, std::shared_ptr<BusHandler>> busMap_;
  std::recursive_mutex procMutex_;
  std::unique_ptr<asio::io_context> io_;
  std::thread executor_;
  std::unique_ptr<Acceptor> acceptor_;
  std::shared_ptr<spdlog::logger> logger_;
  std::unique_ptr<sen::kernel::Tracer> tracer_;
  std::mutex ioMutex_;

  // timers
  std::unordered_map<TimerId, TimerData> timers_;
  TimerId nextTimerId_ = 1ULL;
  std::mutex timersMutex_;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline BusHandler* EtherTransport::getHandler(kernel::BusId& busId)
{
  BusHandler* handler;
  if (SEN_UNLIKELY(busId.userData == nullptr))
  {
    handler = busMap_.find(busId)->second.get();
    busId.userData = handler;
  }
  else
  {
    handler = static_cast<BusHandler*>(busId.userData);
  }

  return handler;
}

inline ProcessHandler* EtherTransport::getHandler(kernel::ParticipantAddr& participantAddr)
{
  ProcessHandler* handler = nullptr;
  if (SEN_UNLIKELY(participantAddr.userData == nullptr))
  {
    if (auto itr = readyProcesses_.find(participantAddr.proc); itr != readyProcesses_.end())
    {
      handler = itr->second;
      participantAddr.userData = handler;
    }
  }
  else
  {
    handler = static_cast<ProcessHandler*>(participantAddr.userData);
  }

  return handler;
}

inline void EtherTransport::sendTo(kernel::ParticipantAddr& to,
                                   kernel::BusId& busId,
                                   TransportMode mode,
                                   MemBlockPtr data)
{
  Lock procLock(procMutex_);

  switch (mode)
  {
    case TransportMode::multicast:
    {
      if (config_.busConfig.multicastDisabled)
      {
        for (auto& [id, procHandler]: readyProcesses_)
        {
          procHandler->sendBusMessageConfirmed(to, busId, std::move(data));
        }
      }
      else
      {
        getHandler(busId)->broadcast(std::move(data));
      }
    }
    break;

    case TransportMode::unicast:
    {
      if (auto* handler = getHandler(to); handler != nullptr)
      {
        handler->sendBusMessageUnicast(to, busId, std::move(data));
      }
    }
    break;

    case TransportMode::confirmed:
    {
      if (auto* handler = getHandler(to); handler != nullptr)
      {
        handler->sendBusMessageConfirmed(to, busId, std::move(data));
      }
    }
    break;

    default:
      SEN_UNREACHABLE();
      throw std::logic_error("invalid transport mode");
  }
}

inline void EtherTransport::sendTo(kernel::ParticipantAddr& to,
                                   kernel::BusId& busId,
                                   TransportMode mode,
                                   MemBlockPtr data1,
                                   MemBlockPtr data2)
{
  Lock procLock(procMutex_);

  switch (mode)
  {
    case TransportMode::multicast:
    {
      if (config_.busConfig.multicastDisabled)
      {
        for (auto& [id, procHandler]: readyProcesses_)
        {
          procHandler->sendBusMessageConfirmed(to, busId, std::move(data1), std::move(data2));
        }
      }
      else
      {
        getHandler(busId)->broadcast(std::move(data1), std::move(data2));
      }
    }
    break;

    case TransportMode::unicast:
    {
      if (auto* handler = getHandler(to); handler != nullptr)
      {
        handler->sendBusMessageUnicast(to, busId, std::move(data1), std::move(data2));
      }
    }
    break;

    case TransportMode::confirmed:
    {
      if (auto* handler = getHandler(to); handler != nullptr)
      {
        handler->sendBusMessageConfirmed(to, busId, std::move(data1), std::move(data2));
      }
    }
    break;

    default:
      SEN_UNREACHABLE();
      throw std::logic_error("invalid transport mode");
  }
}

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_ETHER_TRANSPORT_H
