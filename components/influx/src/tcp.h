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
#include <thread>

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

public:
  void reconnect();

private:
  asio::io_context& ioContext_;
  asio::ip::tcp::socket socket_;
  asio::ip::tcp::endpoint endpoint_;
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

inline void TCP::reconnect()
{
  while (true)
  {
    try
    {
      socket_.connect(endpoint_);
    }
    catch (...)
    {
      std::cerr << "error connecting to the enpoint - retrying in 1 second" << std::endl;
      std::this_thread::sleep_for(std::chrono::seconds(1));
      continue;
    }

    socket_.wait(asio::ip::tcp::socket::wait_write);
    break;
  }
}

inline void TCP::send(std::string&& message)
{
  try
  {
    message.append("\n");
    const size_t written = asio::write(socket_, asio::buffer(message, message.size()));
    if (written != message.size())
    {
      throwRuntimeError("Error while transmitting data");
    }
  }
  catch (const asio::system_error& e)
  {
    throwRuntimeError(e.what());
  }
}

}  // namespace sen::components::influx
