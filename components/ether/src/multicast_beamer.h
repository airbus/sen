// === multicast_beamer.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_MULTICAST_BEAMER_H
#define SEN_COMPONENTS_ETHER_SRC_MULTICAST_BEAMER_H

#include "beamer.h"

// stl
#include "stl/configuration.stl.h"

// asio
#include <asio/io_context.hpp>

namespace sen::components::ether
{

class MulticastBeamer final: public BeamerBase
{
  SEN_NOCOPY_NOMOVE(MulticastBeamer)

public:
  MulticastBeamer(asio::io_context& io, const Configuration& config);
  ~MulticastBeamer() final;

protected:
  void sendBeam(const std::vector<uint8_t>& beam) final;

private:
  asio::ip::udp::endpoint endpoint_;
  asio::ip::udp::socket socket_;
};

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_MULTICAST_BEAMER_H
