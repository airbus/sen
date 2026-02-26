// === udp.h ===========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "database.h"
#include "sen/core/base/compiler_macros.h"

// asio
#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>

// std
#include <string>

namespace sen::components::influx
{

/// UDP transport
class UDP: public Transport
{
  SEN_NOCOPY_NOMOVE(UDP)

public:
  UDP(asio::io_context& ioContext, const std::string& hostname, int port);
  ~UDP() override = default;

public:
  void send(std::string&& message) override;

private:
  asio::io_context& ioContext_;
  asio::ip::udp::socket socket_;
  asio::ip::udp::endpoint endpoint_;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline UDP::UDP(asio::io_context& ioContext, const std::string& hostname, int port)
  : ioContext_(ioContext), socket_(ioContext_, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0))
{
  asio::ip::udp::resolver resolver(ioContext_);
  if (auto results = resolver.resolve(asio::ip::udp::v4(), hostname, std::to_string(port)); !results.empty())
  {
    endpoint_ = results.begin()->endpoint();
  }
}

inline void UDP::send(std::string&& message)
{
  try
  {
    socket_.send_to(asio::buffer(message, message.size()), endpoint_);
  }
  catch (const asio::system_error& e)
  {
    throwRuntimeError(e.what());
  }
}

}  // namespace sen::components::influx
