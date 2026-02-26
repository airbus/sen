// === multicast_beam_tracker.h ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_MULTICAST_BEAM_TRACKER_H
#define SEN_COMPONENTS_ETHER_SRC_MULTICAST_BEAM_TRACKER_H

#include "util.h"

// component
#include "beam_tracker.h"

// stl
#include "stl/configuration.stl.h"

// asio
#include <asio/io_context.hpp>

namespace sen::components::ether
{

class MulticastBeamTracker final: public BeamTrackerBase
{
public:
  SEN_NOCOPY_NOMOVE(MulticastBeamTracker)

public:
  using FoundCallback = std::function<void(const BeamInfo&)>;
  using LostCallback = std::function<void(const kernel::ProcessInfo&)>;

public:
  MulticastBeamTracker(asio::io_context& io,
                       const Configuration& config,
                       FoundCallback&& onFound,
                       LostCallback&& onLost);
  ~MulticastBeamTracker() final;

public:
  void start(kernel::RunApi& api) final;

private:
  void receiveLoop(kernel::RunApi& api);
  void startRcvTimer(kernel::RunApi& api);

private:
  Configuration config_;
  asio::ip::udp::socket socket_;
  asio::steady_timer rcvTimeoutTimer_;
};

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_MULTICAST_BEAM_TRACKER_H
