// === ether_transport.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "ether_transport.h"

// component
#include "bus_handler.h"
#include "discovery.h"
#include "process_handler.h"
#include "stats.h"
#include "util.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/hash32.h"
#include "sen/core/obj/object_provider.h"
#include "sen/kernel/tracer.h"
#include "sen/kernel/transport.h"
#include "sen/kernel/util.h"

// generated code
#include "stl/configuration.stl.h"
#include "stl/discovery.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// asio
#include <asio/error.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/basic_endpoint.hpp>
#include <asio/ip/tcp.hpp>

// std
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace sen::components::ether
{

std::atomic<std::size_t> udpSentBytes = 0;
std::atomic<std::size_t> udpReceivedBytes = 0;
std::atomic<std::size_t> tcpSentBytes = 0;
std::atomic<std::size_t> tcpReceivedBytes = 0;

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

void populateLocalEndpoints(EnpointList& endpoints, uint16_t port, const Configuration& config)
{
  const auto interfaces = getLocalInterfaces(config);
  for (const auto& elem: interfaces)
  {
    if (endpoints.full())
    {
      return;
    }
    endpoints.push_back({elem.address.to_v4().to_uint(), port});
  }
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Acceptor
//--------------------------------------------------------------------------------------------------------------

class Acceptor final
{
  SEN_NOCOPY_NOMOVE(Acceptor)

public:
  Acceptor(EtherTransport* transport, asio::io_context& io, uint16_t port): acceptor_(io), transport_(transport)
  {
    asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);

    getLogger()->debug("EtherTransport: accepting connections on {}", acceptor_.local_endpoint().port());
    acceptor_.listen();
    doAccept();
  }

  ~Acceptor() = default;

public:
  [[nodiscard]] asio::ip::tcp::endpoint getEndpoint() const { return acceptor_.local_endpoint(); }

private:
  void doAccept()
  {
    acceptor_.async_accept(
      [this](std::error_code ec, asio::ip::tcp::socket socket)
      {
        if (!ec)
        {
          transport_->processConnected(std::move(socket));
        }
        else
        {
          getLogger()->debug("EtherTransport: error accepting connection: {}", ec.message());
        }
        doAccept();
      });
  }

private:
  asio::ip::tcp::acceptor acceptor_;
  EtherTransport* transport_;
};

//--------------------------------------------------------------------------------------------------------------
// EtherTransport
//--------------------------------------------------------------------------------------------------------------

EtherTransport::EtherTransport(const Configuration& config,
                               std::string_view sessionName,
                               std::string_view appName,
                               std::shared_ptr<DiscoverySystem> discovery,
                               std::unique_ptr<sen::kernel::Tracer> tracer)
  : kernel::Transport(sessionName)
  , config_(config)
  , sessionId_(crc32(sessionName))
  , ownInfo_(sen::kernel::getOwnProcessInfo(sessionName))
  , discovery_(std::move(discovery))
  , logger_(getLogger())
  , tracer_(std::move(tracer))
{
  ownInfo_.appName = appName;
}

EtherTransport::~EtherTransport() { logger_->debug("transport: deleted"); }

void EtherTransport::stop() noexcept
{
  {
    Lock procLock(procMutex_);

    logger_->debug("transport: stop started");
    discovery_->removeListener(this, false);
    discovery_->stopBeaming(ownInfo_);

    // signal the i/o to stop accepting new requests
    if (io_)
    {
      logger_->debug("transport: stopping i/o");
      {
        io_->stop();  // stop all requests
      }
    }

    // stop bus activity
    logger_->debug("transport: stopping bus handlers");
    for (const auto& [first, second]: busMap_)
    {
      second->stop();
    }

    // stop process activity
    logger_->debug("transport: stopping process handlers");
    for (const auto& elem: processes_)
    {
      elem->stop();
    }

    // do not accept incoming connections
    acceptor_.reset();

    // clear our state
    {
      logger_->debug("transport: clearing state");
      readyProcesses_.clear();
      processes_.clear();
      busMap_.clear();
    }
  }

  if (std::this_thread::get_id() != executor_.get_id() && executor_.joinable())
  {
    logger_->debug("transport: waiting for thread");
    executor_.join();
  }

  {
    std::lock_guard lock(ioMutex_);
    io_ = {};  // free all i/o resources
  }

  logger_->debug("transport: stop finished");
}

kernel::TransportStats EtherTransport::fetchStats() const
{
  kernel::TransportStats result;
  result.udpSentBytes = udpSentBytes;
  result.udpReceivedBytes = udpReceivedBytes;
  result.tcpSentBytes = tcpSentBytes;
  result.tcpReceivedBytes = tcpReceivedBytes;
  return result;
}

void EtherTransport::start(kernel::TransportListener* listener)
{
  {
    std::lock_guard lock(ioMutex_);
    io_ = std::make_unique<asio::io_context>();
  }
  acceptor_ = std::make_unique<Acceptor>(this, *io_, 0);
  listener_ = listener;
  discovery_->addListener(this);

  EnpointList endpoints;
  populateLocalEndpoints(endpoints, acceptor_->getEndpoint().port(), config_);

  if (endpoints.empty())
  {
    const std::string err =
      "No available interfaces. If working in a container, mind that virtual interfaces (NO_CARRIER) are discarded "
      "by default; set the allowVirtualInterfaces to true to change it.";
    getLogger()->error(err);
    throw std::runtime_error(err);
  }

  discovery_->startBeaming(ownInfo_.sessionName, endpoints);
  executor_ = std::thread(
    [this]()
    {
      try
      {
        io_->run();
      }
      catch (const asio::error_code& ec)
      {
        logger_->error("transport: i/o thread error: {}", ec.message());
      }
      catch (...)
      {
        logger_->error("transport: unknown i/o thread error");
      }
    });
}

const Configuration& EtherTransport::getConfig() const noexcept { return config_; }

const kernel::ProcessInfo& EtherTransport::getOwnInfo() const noexcept { return ownInfo_; }

TimerId EtherTransport::startTimer(std::chrono::steady_clock::duration timeout, std::function<void()>&& timeoutCallback)
{
  std::scoped_lock timersLock(timersMutex_);

  const auto id = nextTimerId_++;

  const auto [itr, done] = timers_.emplace(id, TimerData {*io_, std::move(timeoutCallback)});

  auto& data = itr->second;
  data.timer.expires_after(timeout);

  // handler to be executed when the timeout is reached
  data.timer.async_wait(
    [this, id, &data](const asio::error_code& ec) mutable
    {
      std::function<void()> timeoutCb;
      {
        std::scoped_lock lock(timersMutex_);

        // timer was canceled, timer data erasing handled in
        // the cancelTimer method
        if (ec == asio::error::operation_aborted)
        {
          return;
        }

        timeoutCb = std::move(data.timeoutCallback);
        timers_.erase(id);
      }

      if (ec)
      {
        logger_->error("Error in asynchronous timer: {}", ec.message());
        return;
      }

      // timeout was reached, execute the handler
      timeoutCb();
    });

  return id;
}

bool EtherTransport::cancelTimer(TimerId id)
{
  std::scoped_lock timersLock(timersMutex_);
  if (const auto it = timers_.find(id.get()); it != timers_.end())
  {
    // If cancel() > 0, the handler will be called with operation_aborted.
    // If cancel() == 0, the handler is already executing or queued with success.
    if (it->second.timer.cancel() > 0)
    {
      timers_.erase(it);
      return true;  // We successfully stopped it before it ran
    }
  }
  return false;  // It was too late or didn't exist
}

void EtherTransport::beamFound(const BeamInfo& info)
{
  if (info.process.hostId == ownInfo_.hostId && info.process.processId == ownInfo_.processId)
  {
    // detected ourselves
  }
  else if (info.process.sessionId == ownInfo_.sessionId)
  {
    detectedNewProcess(info);
  }
  else
  {
    getLogger()->debug("detected session '{}' from host {} (process {})",
                       info.process.sessionName,
                       info.process.hostId,
                       info.process.processId);
  }
}

void EtherTransport::beamLost(const kernel::ProcessInfo& processInfo)
{
  if (processInfo != ownInfo_ && processInfo.sessionId == ownInfo_.sessionId)
  {
    lostExistingProcess(processInfo);
  }
}

void EtherTransport::detectedNewProcess(const BeamInfo& beam)
{
  // NOTE: in case the process ID matches, the host ID determines who starts the connection
  if (ownInfo_.processId > beam.process.processId ||
      (ownInfo_.processId == beam.process.processId && ownInfo_.hostId > beam.process.hostId))
  {
    Lock procLock(procMutex_);

    // collect end points
    std::vector<asio::ip::basic_endpoint<asio::ip::tcp>> otherEndpoints;
    otherEndpoints.reserve(beam.endpoints.size());
    for (const auto& elem: beam.endpoints)
    {
      otherEndpoints.push_back(getAsioTcpEndpoint(elem));
    }

    getLogger()->debug("EtherTransport: detectedNewProcess in session {}  host {} pid {}. Will start connection",
                       beam.process.sessionName,
                       beam.process.hostId,
                       beam.process.processId);

    // create the process handler and make it start the connection
    processes_.push_back(ProcessHandler::make(this, listener_, *io_, *tracer_));
    processes_.back()->startConnection(std::move(otherEndpoints));
  }
  else
  {
    getLogger()->debug("EtherTransport: detectedNewProcess in session {}  host {} pid {}. Will wait for connection",
                       beam.process.sessionName,
                       beam.process.hostId,
                       beam.process.processId);
  }
}

void EtherTransport::processConnected(asio::ip::tcp::socket socket)
{
  Lock procLock(procMutex_);

  getLogger()->debug("EtherTransport: process connected");

  // create the process handler, and make it wait until ready before doing anything else
  processes_.push_back(ProcessHandler::make(this, listener_, std::move(socket), *io_, *tracer_));
  processes_.back()->startHandlingIncomingConnection();
}

void EtherTransport::processDisconnected(ProcessHandler* process)
{
  const auto& id = process->getId();

  {
    Lock procLock(procMutex_);
    readyProcesses_.erase(id);
  }

  listener_->remoteProcessLost(id);  // needs to be called without holding procMutex_

  {
    Lock procLock(procMutex_);
    auto isSame = [process](const auto& other) { return other.get() == process; };
    processes_.erase(std::remove_if(processes_.begin(), processes_.end(), std::move(isSame)), processes_.end());
  }
}

kernel::ProcessId EtherTransport::processReady(ProcessHandler* process)
{
  Lock procLock(procMutex_);
  auto id = process->getInfo().processId;
  readyProcesses_.insert({id, process});

  // notify the process about the existing local participants
  // in all the open buses
  for (const auto& [busId, bus]: busMap_)
  {
    for (const auto& local: bus->getLocalParticipants())
    {
      process->localParticipantJoinedBus(local, busId, bus->getName());
    }
  }
  return id;
}

void EtherTransport::lostExistingProcess(const kernel::ProcessInfo& info)
{
  std::vector<std::shared_ptr<ProcessHandler>> list;

  {
    Lock procLock(procMutex_);
    list = processes_;
  }

  auto isSame = [&info](const auto& other) { return other->getInfo() == info; };
  auto itr = std::find_if(list.begin(), list.end(), std::move(isSame));
  if (itr != list.end())
  {
    itr->get()->lostBeam();
  }
}

void EtherTransport::localParticipantJoinedBus(ObjectOwnerId participant, kernel::BusId bus, const std::string& busName)
{
  Lock procLock(procMutex_);

  // create the bus handler, if not present
  auto busItr = busMap_.find(bus);
  if (busItr == busMap_.end())
  {
    const auto procId = ownInfo_.processId;

    uint16_t port = 60543;
    if (std::holds_alternative<MulticastDiscovery>(config_.discovery))
    {
      port = std::get<MulticastDiscovery>(config_.discovery).port;
    }

    auto handler = BusHandler::make(sessionId_, bus, busName, procId, listener_, port, *io_, config_, *tracer_);
    handler->startReading();

    auto [itr, done] = busMap_.try_emplace(bus, std::move(handler));
    busItr = itr;
  }

  // save the id of the local Participant
  busItr->second->saveLocalParticipantId(participant);

  // notify other processes about this local localParticipantId
  for (const auto& [first, second]: readyProcesses_)
  {
    second->localParticipantJoinedBus(participant, bus, busName);
  }
}

void EtherTransport::localParticipantLeftBus(ObjectOwnerId participant, kernel::BusId bus, const std::string& busName)
{
  Lock procLock(procMutex_);

  for (const auto& [first, second]: readyProcesses_)
  {
    second->localParticipantLeftBus(participant, bus, busName);
  }

  // remove the participant id from our bus handler
  auto busItr = busMap_.find(bus);
  if (busItr != busMap_.end())
  {
    busItr->second->removeLocalParticipantId(participant);

    // if no local participants, we don't need the handler
    if (!busItr->second->hasLocalParticipants())
    {
      busMap_.erase(busItr);
    }
  }
}

}  // namespace sen::components::ether
