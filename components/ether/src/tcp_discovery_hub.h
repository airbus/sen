// === tcp_discovery_hub.h =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_TCP_DISCOVERY_HUB_H
#define SEN_COMPONENTS_ETHER_SRC_TCP_DISCOVERY_HUB_H

// generated code
#include "stl/configuration.stl.h"

// asio
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

// std
#include <list>

namespace sen::components::ether
{

class TcpDiscoveryHub
{
  SEN_NOCOPY_NOMOVE(TcpDiscoveryHub)

public:
  explicit TcpDiscoveryHub(asio::io_context& io, Configuration config);
  ~TcpDiscoveryHub();

private:
  void doAccept();

private:
  asio::ip::tcp::acceptor acceptor_;
  std::list<asio::ip::tcp::socket> sockets_;
  Configuration config_;
};

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_TCP_DISCOVERY_HUB_H
