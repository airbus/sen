// === beamer.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_BEAMER_H
#define SEN_COMPONENTS_ETHER_SRC_BEAMER_H

// component
#include "util.h"

// std
#include <mutex>

namespace sen::components::ether
{

class BeamerBase
{
  SEN_NOCOPY_NOMOVE(BeamerBase)

public:
  static constexpr std::size_t maxBeamSize = 508U;

public:
  BeamerBase(asio::io_context& io, ::sen::Duration beamPeriod);
  virtual ~BeamerBase() = default;

public:
  void start(kernel::RunApi& api);
  void stopBeaming(const kernel::ProcessInfo& processInfo);
  void startBeaming(const std::string& sessionName, const EnpointList& endpoints);

protected:
  virtual void sendBeam(const std::vector<uint8_t>& beam) = 0;

private:
  void beam(const asio::error_code& timerError, kernel::RunApi& api);
  void programNextBeam(kernel::RunApi& api);

private:
  struct BeamData
  {
    SessionPresenceBeam beam;
    std::vector<uint8_t> message;
  };

private:
  asio::io_context& io_;
  ::sen::Duration beamPeriod_;
  asio::steady_timer timer_;
  std::mutex beamsMutex_;
  std::vector<BeamData> beams_;
};

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_BEAMER_H
