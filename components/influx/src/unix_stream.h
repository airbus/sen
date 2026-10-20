// === unix_stream.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_INFLUX_SRC_UNIX_STREAM_H
#define SEN_COMPONENTS_INFLUX_SRC_UNIX_STREAM_H

// component
#include "database.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"

// asio
#include <asio/io_context.hpp>
#include <asio/local/stream_protocol.hpp>
#include <asio/write.hpp>  // NOLINT(misc-include-cleaner)

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
  [[nodiscard]] bool reconnect();
  void reportDropping();

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

inline bool UnixStream::connected()
{
  if (connected_)
  {
    return true;
  }

  asio::error_code error;
  std::ignore = socket_.connect(endpoint_, error);
  connected_ = !error;
  return connected_;
}

inline void UnixStream::reportDropping()
{
  if (!disconnected_)
  {
    std::cerr << "influx: the telegraf socket is not answering; points are being dropped" << std::endl;
    disconnected_ = true;
  }
}

inline bool UnixStream::reconnect()
{
  // One attempt, as for TCP: waiting for a sink that may never return stops what is being recorded.
  asio::error_code error;
  std::ignore = socket_.close(error);
  std::ignore = socket_.connect(endpoint_, error);
  connected_ = !error;
  return connected_;
}

inline void UnixStream::doSend(sen::Span<const char> data)
{
  if (!connected())
  {
    reportDropping();
    return;
  }

  asio::error_code error;
  asio::write(socket_, asio::buffer(data.data(), data.size()), error);

  if (!error)
  {
    disconnected_ = false;
    return;
  }

  if (!reconnect())
  {
    reportDropping();
    return;
  }

  asio::write(socket_, asio::buffer(data.data(), data.size()), error);

  if (error)
  {
    reportDropping();
    return;
  }

  disconnected_ = false;
}

}  // namespace sen::components::influx

#endif  // defined(ASIO_HAS_LOCAL_SOCKETS)

#endif  // SEN_COMPONENTS_INFLUX_SRC_UNIX_STREAM_H
