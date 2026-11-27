// === udp.h ===========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_INFLUX_SRC_UDP_H
#define SEN_COMPONENTS_INFLUX_SRC_UDP_H

// component
#include "database.h"
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"

// asio
#include <asio/io_context.hpp>
#include <asio/ip/udp.hpp>

// std
#include <iostream>
#include <string>
#include <tuple>

namespace sen::components::influx
{

/// UDP transport
class UDP: public Transport
{
  SEN_NOCOPY_NOMOVE(UDP)

public:
  UDP(asio::io_context& ioContext, const std::string& hostname, int port, IpFamily family);
  ~UDP() override = default;

protected:
  void doSend(sen::Span<const char> data) override;

private:
  asio::io_context& ioContext_;
  asio::ip::udp::socket socket_;
  asio::ip::udp::endpoint endpoint_;
  /// Whether the last send reported something, so a closed port is said once and not per datagram.
  bool reported_ {false};
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline UDP::UDP(asio::io_context& ioContext, const std::string& hostname, int port, IpFamily family)
  : ioContext_(ioContext), socket_(ioContext_)
{
  asio::ip::udp::resolver resolver(ioContext_);

  const auto results = (family == IpFamily::v4) ? resolver.resolve(asio::ip::udp::v4(), hostname, std::to_string(port))
                       : (family == IpFamily::v6)
                         ? resolver.resolve(asio::ip::udp::v6(), hostname, std::to_string(port))
                         : resolver.resolve(hostname, std::to_string(port));

  if (results.empty())
  {
    throwRuntimeError("no address for '" + hostname + "'");
  }

  endpoint_ = results.begin()->endpoint();

  // Opened for whatever the resolver answered: a v4 socket cannot carry a v6 endpoint, and the
  // unspecified form may give either.
  socket_.open(endpoint_.protocol());
}

inline void UDP::doSend(sen::Span<const char> data)
{
  // A datagram needs nobody listening to be sent. A closed port answers ICMP, which the next send
  // reports as an error, so it is said once and sending continues.
  asio::error_code error;
  std::ignore = socket_.send_to(asio::buffer(data.data(), data.size()), endpoint_, 0, error);

  if (error)
  {
    if (!reported_)
    {
      std::cerr << "influx: sending to telegraf reported '" << error.message() << "'; datagrams are still being sent"
                << std::endl;
      reported_ = true;
    }

    return;
  }

  reported_ = false;
}

}  // namespace sen::components::influx

#endif  // SEN_COMPONENTS_INFLUX_SRC_UDP_H
