// === tcp.h ===========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "database.h"

// asio
#include <asio/connect.hpp>  // NOLINT(misc-include-cleaner)
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>  // NOLINT(misc-include-cleaner)

// std
#include <iostream>
#include <string>
#include <tuple>

namespace sen::components::influx
{

/// UDP transport
class TCP: public Transport
{
  SEN_NOCOPY_NOMOVE(TCP)

public:
  TCP(asio::io_context& ioContext, const std::string& hostname, int port);
  ~TCP() override = default;

public:
  void send(std::string&& message) override;

private:
  /// Closes the socket and dials the endpoint once. False when the far end is still gone.
  [[nodiscard]] bool reconnect();

  /// Says once that points are being dropped, whichever of the two writes gave up.
  void reportDropping();

private:
  asio::io_context& ioContext_;
  asio::ip::tcp::socket socket_;
  asio::ip::tcp::endpoint endpoint_;
  /// Whether the last send failed, so a sink that stays down is reported once and not per point.
  bool disconnected_ {false};
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline TCP::TCP(asio::io_context& ioContext, const std::string& hostname, int port)
  : ioContext_(ioContext), socket_(ioContext_)
{
  asio::ip::tcp::resolver resolver(ioContext_);
  const auto results = resolver.resolve(hostname, std::to_string(port));
  asio::connect(socket_, results);
  endpoint_ = socket_.remote_endpoint();
}

inline void TCP::reportDropping()
{
  if (!disconnected_)
  {
    std::cerr << "influx: the telegraf endpoint is not answering; points are being dropped" << std::endl;
    disconnected_ = true;
  }
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
  std::ignore = socket_.connect(endpoint_, error);
  return !error;
}

inline void TCP::send(std::string&& message)
{
  message.append("\n");
  const auto buffer = asio::buffer(message, message.size());

  asio::error_code error;
  asio::write(socket_, buffer, error);

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

    asio::write(socket_, buffer, error);
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
