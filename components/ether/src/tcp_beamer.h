// === tcp_beamer.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_TCP_BEAMER_H
#define SEN_COMPONENTS_ETHER_SRC_TCP_BEAMER_H

#include "beamer.h"

// sen
#include "sen/core/base/duration.h"

// stl
#include "stl/configuration.stl.h"

// asio
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

namespace sen::components::ether
{

class TcpBeamer final: public BeamerBase
{
  SEN_NOCOPY_NOMOVE(TcpBeamer)

public:
  explicit TcpBeamer(asio::io_context& io,
                     asio::ip::tcp::socket& socket,
                     ::sen::Duration beamPeriod,
                     const Configuration& config);
  ~TcpBeamer() final;

public:
  void setConnected(bool connected);

protected:
  void sendBeam(const std::vector<uint8_t>& beam) final;

private:
  asio::ip::tcp::socket& socket_;
  bool connected_ = false;
};

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_TCP_BEAMER_H
