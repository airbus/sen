// === multicast_beam_tracker.cpp ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "multicast_beam_tracker.h"

// component
#include "beam_tracker.h"
#include "beamer.h"
#include "util.h"

// sen
#include "sen/kernel/component_api.h"

// generated code
#include "stl/configuration.stl.h"
#include "stl/discovery.stl.h"

// asio
#include <asio/buffer.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/udp.hpp>
#include <asio/socket_base.hpp>

// std
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <system_error>
#include <tuple>
#include <utility>
#include <vector>

namespace sen::components::ether
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

namespace
{

constexpr auto rcvTimeout = std::chrono::milliseconds(1000);

}  // namespace

struct ReadOp
{
  std::vector<uint8_t> buffer;
  asio::ip::udp::endpoint senderEndpoint;
};

//--------------------------------------------------------------------------------------------------------------
// MulticastBeamTracker
//--------------------------------------------------------------------------------------------------------------

MulticastBeamTracker::MulticastBeamTracker(asio::io_context& io,
                                           const Configuration& config,
                                           FoundCallback&& onFound,
                                           LostCallback&& onLost)
  : BeamTrackerBase(io,
                    std::move(onFound),
                    std::move(onLost),
                    std::get<MulticastDiscovery>(config.discovery).beamExpirationTime)
  , config_(config)
  , socket_(io)
  , rcvTimeoutTimer_(io)
{
  const auto& mcastConfig = std::get<MulticastDiscovery>(config.discovery);
  auto listenEndpoint = asio::ip::udp::endpoint(asio::ip::make_address(mcastConfig.mcastGroup), mcastConfig.port);
  configureMulticastSocket(
    socket_, listenEndpoint, std::get<MulticastDiscovery>(config.discovery).device, config, getLogger().get());
}

MulticastBeamTracker::~MulticastBeamTracker()
{
  try
  {
    asio::error_code ec;
    // Currently, we don't handle the returned asio::error_code
    socket_.shutdown(asio::socket_base::shutdown_both, ec);  // NOLINT(bugprone-unused-return-value)
    socket_.close(ec);                                       // NOLINT(bugprone-unused-return-value)
  }
  catch (...)
  {
    // swallow error
  }
}

void MulticastBeamTracker::start(kernel::RunApi& api)
{
  if (api.stopRequested())
  {
    return;
  }

  startRcvTimer(api);
  receiveLoop(api);
}

void MulticastBeamTracker::receiveLoop(kernel::RunApi& api)
{
  auto opPtr = std::make_shared<ReadOp>();
  opPtr->buffer.resize(BeamerBase::maxBeamSize);

  auto receive = [this, &api, ptr = opPtr](std::error_code ec, std::size_t length)
  {
    std::ignore = length;
    if (!ec)
    {
      beamReceived(api, readFromBuffer<SessionPresenceBeam>(ptr->buffer));
      receiveLoop(api);
    }
  };
  socket_.async_receive_from(asio::buffer(opPtr->buffer), opPtr->senderEndpoint, std::move(receive));
}

void MulticastBeamTracker::startRcvTimer(kernel::RunApi& api)
{
  rcvTimeoutTimer_.cancel();
  rcvTimeoutTimer_.expires_after(rcvTimeout);
  rcvTimeoutTimer_.async_wait(
    [this, &api](std::error_code ec)
    {
      if (!ec)
      {
        socket_.cancel();
        start(api);
      }
    });
}

}  // namespace sen::components::ether
