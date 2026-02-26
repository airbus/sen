// === multicast_beamer.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "multicast_beamer.h"

// component
#include "beamer.h"
#include "util.h"

// sen
#include "sen/core/base/assert.h"

// generated code
#include "stl/configuration.stl.h"

// asio
#include <asio/buffer.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/socket_base.hpp>

// std
#include <cstdint>
#include <system_error>
#include <vector>

namespace sen::components::ether
{

MulticastBeamer::MulticastBeamer(asio::io_context& io, const Configuration& config)
  : BeamerBase(io, std::get<MulticastDiscovery>(config.discovery).beamPeriod)
  , endpoint_(asio::ip::make_address(std::get<MulticastDiscovery>(config.discovery).mcastGroup),
              std::get<MulticastDiscovery>(config.discovery).port)
  , socket_(io)
{
  configureMulticastSocket(
    socket_, endpoint_, std::get<MulticastDiscovery>(config.discovery).device, config, getLogger().get());
}

MulticastBeamer::~MulticastBeamer()
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
    // no code needed... swallow the exception
  }
}

void MulticastBeamer::sendBeam(const std::vector<uint8_t>& beam)
{
  auto sent = [](std::error_code ec, auto /* unused */)
  {
    if (ec)
    {
      throwRuntimeError(ec.message());
    }
  };

  socket_.async_send_to(asio::buffer(beam), endpoint_, sent);
}

}  // namespace sen::components::ether
