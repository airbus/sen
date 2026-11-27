// === unix_stream.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_INFLUX_SRC_UNIX_STREAM_H
#define SEN_COMPONENTS_INFLUX_SRC_UNIX_STREAM_H

// component
#include "bounded_write.h"
#include "database.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"

// asio
#include <asio/io_context.hpp>
#include <asio/local/stream_protocol.hpp>

// std
#include <iostream>
#include <string>
#include <tuple>

// Without ASIO_HAS_LOCAL_SOCKETS, which Windows does not define, asio/local declares nothing and
// the types below do not exist. component.cpp refuses the transport at configuration time instead.
#if defined(ASIO_HAS_LOCAL_SOCKETS)

namespace sen::components::influx
{

/// A Unix stream socket, Telegraf's "unix" scheme. Framed by the line separator, like TCP.
class UnixStream: public Transport
{
  SEN_NOCOPY_NOMOVE(UnixStream)

public:
  UnixStream(asio::io_context& ioContext, const std::string& path);
  ~UnixStream() override = default;

protected:
  void doSend(sen::Span<const char> data) override;
  [[nodiscard]] bool requiresLineSeparator() const noexcept override { return true; }

private:
  /// Dials on first use, so a socket that does not exist yet is not a failure to start.
  [[nodiscard]] bool connected();

  /// Closes whatever was there and dials the socket file once, bounded. False when Telegraf is
  /// still not listening on it.
  [[nodiscard]] bool dial();
  void reportDropping();
  void reportRecovered();

private:
  asio::io_context& ioContext_;
  asio::local::stream_protocol::socket socket_;
  asio::local::stream_protocol::endpoint endpoint_;
  bool connected_ {false};
  bool disconnected_ {false};
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline UnixStream::UnixStream(asio::io_context& ioContext, const std::string& path)
  : ioContext_(ioContext), socket_(ioContext_), endpoint_(path)
{
  // Not connected here, as for TCP: a Telegraf that has not created its socket yet is ordinary.
}

inline bool UnixStream::connected() { return connected_ || dial(); }

/// A socket file answers or refuses at once, with no SYN budget to run down, but a listener whose
/// accept backlog is full makes the dial wait exactly as a network one does. Bounded for that case,
/// and because the bound is what keeps the sending thread joinable at shutdown. connectBounded
/// leaves the socket non-blocking, which is what writeBounded needs; no keep-alive here, since a
/// socket file has no network to probe.
inline bool UnixStream::dial()
{
  // One attempt, as for TCP: waiting for a sink that may never return stops what is being recorded.
  asio::error_code error;
  connected_ = connectBounded(socket_, endpoint_, streamConnectBound, error);

  return connected_;
}

/// Said once sending works again, which `tcp.h` does and this did not: an operator saw "points are
/// being dropped" and never learned it had stopped.
inline void UnixStream::reportRecovered()
{
  if (disconnected_)
  {
    std::cerr << "influx: the telegraf socket is accepting again" << std::endl;
    disconnected_ = false;
  }
}

inline void UnixStream::reportDropping()
{
  if (!disconnected_)
  {
    std::cerr << "influx: the telegraf socket is not answering; points are being dropped" << std::endl;
    disconnected_ = true;
  }
}

inline void UnixStream::doSend(sen::Span<const char> data)
{
  if (!connected())
  {
    reportDropping();
    return;
  }

  asio::error_code error;
  std::ignore = writeBounded(socket_, data, streamWriteBound, error);

  if (!error)
  {
    reportRecovered();
    return;
  }

  if (error == asio::error::timed_out)
  {
    // As for TCP: part of this message is on the wire, so the stream is out of step and the
    // connection ends rather than carrying on or sending the message a second time.
    asio::error_code closing;
    std::ignore = socket_.close(closing);
    connected_ = false;
    reportDropping();
    return;
  }

  if (!dial())
  {
    reportDropping();
    return;
  }

  std::ignore = writeBounded(socket_, data, streamWriteBound, error);

  if (error)
  {
    reportDropping();
    return;
  }

  reportRecovered();
}

}  // namespace sen::components::influx

#endif  // defined(ASIO_HAS_LOCAL_SOCKETS)

#endif  // SEN_COMPONENTS_INFLUX_SRC_UNIX_STREAM_H
