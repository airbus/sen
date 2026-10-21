// === unix_datagram.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_INFLUX_SRC_UNIX_DATAGRAM_H
#define SEN_COMPONENTS_INFLUX_SRC_UNIX_DATAGRAM_H

// component
#include "database.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"

// asio
#include <asio/io_context.hpp>
#include <asio/local/datagram_protocol.hpp>

// std
#include <iostream>
#include <string>
#include <tuple>

// Without ASIO_HAS_LOCAL_SOCKETS, which Windows does not define, asio/local declares nothing and
// the types below do not exist. component.cpp refuses the transport at configuration time instead.
#if defined(ASIO_HAS_LOCAL_SOCKETS)

namespace sen::components::influx
{

/// A Unix datagram socket, Telegraf's "unixgram" scheme. One datagram is one message, so no line
/// separator is needed and the message size is bounded.
class UnixDatagram: public Transport
{
  SEN_NOCOPY_NOMOVE(UnixDatagram)

public:
  UnixDatagram(asio::io_context& ioContext, const std::string& path);
  ~UnixDatagram() override = default;

protected:
  void doSend(sen::Span<const char> data) override;

private:
  asio::io_context& ioContext_;
  asio::local::datagram_protocol::socket socket_;
  asio::local::datagram_protocol::endpoint endpoint_;
  /// Latched so an absent socket is named once rather than on every cycle.
  bool reported_ {false};
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline UnixDatagram::UnixDatagram(asio::io_context& ioContext, const std::string& path)
  : ioContext_(ioContext), socket_(ioContext_), endpoint_(path)
{
  asio::error_code error;
  std::ignore = socket_.open(asio::local::datagram_protocol(), error);

  // Windows carries AF_UNIX streams but not datagrams, which fails here.
  if (error)
  {
    throwRuntimeError("cannot open a unix datagram socket for '" + path + "': " + error.message() +
                      ". This platform may not provide datagram unix sockets; use the Unix transport instead.");
  }
}

inline void UnixDatagram::doSend(sen::Span<const char> data)
{
  asio::error_code error;
  std::ignore = socket_.send_to(asio::buffer(data.data(), data.size()), endpoint_, 0, error);

  if (error)
  {
    // Latched as udp.h latches its own: at the default sampling period an absent socket would
    // otherwise write about sixty lines a second for as long as the run lasts.
    if (!reported_)
    {
      std::cerr << "influx: could not send to the telegraf socket: " << error.message()
                << "; datagrams are still being sent" << std::endl;
      reported_ = true;
    }

    return;
  }

  reported_ = false;
}

}  // namespace sen::components::influx

#endif  // defined(ASIO_HAS_LOCAL_SOCKETS)

#endif  // SEN_COMPONENTS_INFLUX_SRC_UNIX_DATAGRAM_H
