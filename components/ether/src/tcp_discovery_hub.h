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
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <vector>

namespace sen::components::ether
{

/// Broker for TCP discoveries between processes
class TcpDiscoveryHub
{
  SEN_NOCOPY_NOMOVE(TcpDiscoveryHub)

public:
  explicit TcpDiscoveryHub(asio::io_context& io, Configuration config);
  ~TcpDiscoveryHub();

private:
  class Client;

  void doAccept();
  void broadcast(const std::shared_ptr<Client>& sender, const std::vector<uint8_t>& buffer, std::size_t length);
  void disconnect(const std::shared_ptr<Client>& client);

private:
  asio::ip::tcp::acceptor acceptor_;
  std::list<std::shared_ptr<Client>> clients_;
  Configuration config_;
};

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_TCP_DISCOVERY_HUB_H
