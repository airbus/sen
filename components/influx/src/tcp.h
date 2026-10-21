// === tcp.h ===========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_INFLUX_SRC_TCP_H
#define SEN_COMPONENTS_INFLUX_SRC_TCP_H

// component
#include "bounded_write.h"
#include "database.h"

// sen
#include "sen/core/base/assert.h"

// asio
#include <asio/connect.hpp>  // NOLINT(misc-include-cleaner)
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/socket_base.hpp>
#include <asio/write.hpp>  // NOLINT(misc-include-cleaner)

// std
#include <iostream>
#include <string>
#include <tuple>

namespace sen::components::influx
{

/// TCP transport
class TCP: public Transport
{
  SEN_NOCOPY_NOMOVE(TCP)

public:
  TCP(asio::io_context& ioContext, const std::string& hostname, int port, IpFamily family);
  ~TCP() override = default;

protected:
  void doSend(sen::Span<const char> data) override;
  [[nodiscard]] bool requiresLineSeparator() const noexcept override { return true; }

private:
  /// Dials on first use, so a sink that is not up yet is not a failure to start.
  [[nodiscard]] bool connected();

  /// Closes the socket and dials the endpoint once. False when the far end is still gone.
  [[nodiscard]] bool reconnect();

  /// Says once that points are being dropped, whichever of the two writes gave up.
  void reportDropping();

private:
  static void prepareTheSocket(asio::ip::tcp::socket& socket);

private:
  asio::io_context& ioContext_;
  asio::ip::tcp::socket socket_;
  asio::ip::tcp::resolver::results_type endpoints_;
  bool connected_ {false};
  /// Whether the last send failed, so a sink that stays down is reported once and not per point.
  bool disconnected_ {false};
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline TCP::TCP(asio::io_context& ioContext, const std::string& hostname, int port, IpFamily family)
  : ioContext_(ioContext), socket_(ioContext_)
{
  asio::ip::tcp::resolver resolver(ioContext_);

  // tcp4 and tcp6 ask the resolver for that family alone, so a name with records in both does not
  // silently connect over the other one.
  endpoints_ = (family == IpFamily::v4)   ? resolver.resolve(asio::ip::tcp::v4(), hostname, std::to_string(port))
               : (family == IpFamily::v6) ? resolver.resolve(asio::ip::tcp::v6(), hostname, std::to_string(port))
                                          : resolver.resolve(hostname, std::to_string(port));

  if (endpoints_.empty())
  {
    throwRuntimeError("no address for '" + hostname + "'");
  }

  // Not connected here: a Telegraf that is not up yet is ordinary, and doSend dials on demand.
}

inline void TCP::reportDropping()
{
  if (!disconnected_)
  {
    std::cerr << "influx: the telegraf endpoint is not answering; points are being dropped" << std::endl;
    disconnected_ = true;
  }
}

/// Keep-alive asks the operating system to probe an idle connection, on its own configured
/// interval. Non-blocking mode is what lets writeBounded put a deadline on a write at all: asio
/// hands a would_block back only for a socket the caller set non-blocking.
inline void TCP::prepareTheSocket(asio::ip::tcp::socket& socket)
{
  asio::error_code ignored;
  std::ignore = socket.set_option(asio::socket_base::keep_alive {true}, ignored);
  std::ignore = socket.non_blocking(true, ignored);
}

inline bool TCP::connected()
{
  if (connected_)
  {
    return true;
  }

  asio::error_code error;
  std::ignore = asio::connect(socket_, endpoints_, error);
  connected_ = !error;

  if (connected_)
  {
    prepareTheSocket(socket_);
  }

  return connected_;
}

inline bool TCP::reconnect()
{
  // One attempt, not a loop. Waiting here for a sink that may never come back stops the
  // application that is being recorded, which is the wrong way round: the recording is the part
  // that can be missed.
  // Closing a socket the far end has already dropped can fail, and it does not matter here: what
  // the caller is told is whether the dial below answered.
  asio::error_code error;
  std::ignore = socket_.close(error);
  std::ignore = asio::connect(socket_, endpoints_, error);
  connected_ = !error;

  if (connected_)
  {
    prepareTheSocket(socket_);
  }

  return connected_;
}

inline void TCP::doSend(sen::Span<const char> data)
{
  if (!connected())
  {
    reportDropping();
    return;
  }

  asio::error_code error;
  std::ignore = writeBounded(socket_, data, streamWriteBound, error);

  if (error == asio::error::timed_out)
  {
    // The peer is alive and not draining, so part of this message is on the wire and the stream is
    // out of step. The connection ends: writing the rest would arrive after a gap, and sending the
    // message again would deliver the lines that did get there twice.
    asio::error_code closing;
    std::ignore = socket_.close(closing);
    connected_ = false;
    reportDropping();
    return;
  }

  if (error)
  {
    // Telegraf restarting is an ordinary thing to happen, and this used to end the process: the
    // throw reached the kernel's callback, where nothing catches it, and the run died with a
    // crash report because a sink went away. One reconnect and one retry; after that the point
    // is dropped, because the application being recorded must outlive its recorder.
    if (!reconnect())
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
  }

  if (disconnected_)
  {
    std::cerr << "influx: the telegraf endpoint is answering again" << std::endl;
    disconnected_ = false;
  }
}

}  // namespace sen::components::influx

#endif  // SEN_COMPONENTS_INFLUX_SRC_TCP_H
