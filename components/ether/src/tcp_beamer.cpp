// === tcp_beamer.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "tcp_beamer.h"

#include "util.h"

// component
#include "beamer.h"

// sen
#include "sen/core/base/duration.h"

// stl
#include "stl/configuration.stl.h"

// asio
#include <asio/buffer.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>

// std
#include <array>
#include <cstdint>
#include <memory>
#include <system_error>
#include <vector>

namespace sen::components::ether
{

TcpBeamer::TcpBeamer(asio::io_context& io,
                     asio::ip::tcp::socket& socket,
                     ::sen::Duration beamPeriod,
                     const Configuration& config)
  : BeamerBase(io, beamPeriod), socket_(socket)
{
  configureTcpSocket(socket_, config);
}

TcpBeamer::~TcpBeamer() = default;

void TcpBeamer::setConnected(bool connected) { connected_ = connected; }

void TcpBeamer::sendBeam(const std::vector<uint8_t>& beam)
{
  if (connected_)
  {
    // Length first; see beamHeaderSize in util.h. The header is kept alive by the handler until
    // the write completes.
    auto header = std::make_shared<std::array<uint8_t, beamHeaderSize>>(encodeBeamHeader(beam.size()));
    const std::array<asio::const_buffer, 2U> framed {asio::buffer(*header), asio::buffer(beam)};

    asio::async_write(socket_,
                      framed,
                      [header](std::error_code ec, auto /* unused */)
                      {
                        if (ec)
                        {
                          getLogger()->info("could not send discovery beam to hub: {} ({})", ec.message(), ec.value());
                        }
                      });
  }
}

}  // namespace sen::components::ether
