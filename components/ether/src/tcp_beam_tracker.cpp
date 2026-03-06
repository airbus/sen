// === tcp_beam_tracker.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "tcp_beam_tracker.h"

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
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/socket_base.hpp>

// std
#include <chrono>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <tuple>
#include <utility>

namespace sen::components::ether
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

namespace
{

constexpr auto reconnectTimeout = std::chrono::milliseconds(2000);

}

//--------------------------------------------------------------------------------------------------------------
// TcpBeamTracker
//--------------------------------------------------------------------------------------------------------------

TcpBeamTracker::TcpBeamTracker(asio::io_context& io,
                               Configuration config,
                               FoundCallback&& onFound,
                               LostCallback&& onLost)
  : BeamTrackerBase(io,
                    std::move(onFound),
                    std::move(onLost),
                    std::get<TcpDiscovery>(config.discovery).beamExpirationTime)
  , io_(io)
  , config_(std::move(config))
  , socket_(io_)
  , timer_(io)
{
  asio::ip::tcp::resolver resolver(io);
  configureTcpSocket(socket_, config_);

  const auto& tcpDiscoveryConfig = std::get<TcpDiscovery>(config_.discovery);

  bool found = false;
  const auto results =
    resolver.resolve(tcpDiscoveryConfig.hubAddress.host, std::to_string(tcpDiscoveryConfig.hubAddress.port));

  for (const auto& entry: results)
  {
    if (auto endpoint = entry.endpoint(); endpoint.address().is_v4())
    {
      endpoint_ = std::move(endpoint);
      found = true;
      break;
    }
  }

  if (!found)
  {
    std::string err;
    err.append("did not find any valid IPv4 endpoint for ");
    err.append(tcpDiscoveryConfig.hubAddress.host);
    err.append(":");
    err.append(std::to_string(tcpDiscoveryConfig.hubAddress.port));
    throw std::runtime_error(err);
  }

  buffer_.resize(BeamerBase::maxBeamSize);
}

TcpBeamTracker::~TcpBeamTracker()
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

void TcpBeamTracker::onConnected(std::function<void()>&& func) { onConnected_ = std::move(func); }

void TcpBeamTracker::onDisconnected(std::function<void()>&& func) { onDisconnected_ = std::move(func); }

void TcpBeamTracker::start(kernel::RunApi& api)
{
  if (api.stopRequested())
  {
    return;
  }

  api_ = &api;
  connect();
}

asio::ip::tcp::socket& TcpBeamTracker::getSocket() { return socket_; }

void TcpBeamTracker::connect()
{
  auto callback = [us = shared_from_this()](const auto& err)
  {
    if (err)
    {
      us->timer_.cancel();
      us->timer_.expires_after(reconnectTimeout);
      us->timer_.async_wait(
        [us](std::error_code ec)
        {
          if (!ec)
          {
            us->connect();
          }
        });
    }
    else
    {
      getLogger()->info("connected to discovery hub {}:{}", us->endpoint_.address().to_string(), us->endpoint_.port());

      if (us->onConnected_)
      {
        us->onConnected_();
      }

      us->receive();
    }
  };

  if (!socket_.is_open())
  {
    socket_.open(endpoint_.protocol());
  }

  getLogger()->info("will start connection to discovery hub {}:{}", endpoint_.address().to_string(), endpoint_.port());
  socket_.async_connect(endpoint_, std::move(callback));
}

void TcpBeamTracker::receive()
{
  auto receive = [us = shared_from_this()](std::error_code ec, std::size_t length)
  {
    std::ignore = length;
    if (ec)
    {
      us->socket_.close();
      getLogger()->info(
        "lost connection to discovery hub {}:{}", us->endpoint_.address().to_string(), us->endpoint_.port());

      if (us->onDisconnected_)
      {
        us->onDisconnected_();
      }

      us->connect();
    }
    else
    {
      us->beamReceived(*us->api_, readFromBuffer<SessionPresenceBeam>(us->buffer_));
      us->receive();
    }
  };

  asio::async_read(socket_, asio::buffer(buffer_), std::move(receive));
}

}  // namespace sen::components::ether
