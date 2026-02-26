// === beam_tracker.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_BEAM_TRACKER_H
#define SEN_COMPONENTS_ETHER_SRC_BEAM_TRACKER_H

#include "util.h"

namespace sen::components::ether
{

struct Track;

/// Base class for tracking beams
class BeamTrackerBase
{
public:
  SEN_NOCOPY_NOMOVE(BeamTrackerBase)

public:
  using FoundCallback = std::function<void(const BeamInfo&)>;
  using LostCallback = std::function<void(const kernel::ProcessInfo&)>;

public:
  BeamTrackerBase(asio::io_context& io, FoundCallback&& onFound, LostCallback&& onLost, ::sen::Duration beamExpiryTime);
  virtual ~BeamTrackerBase();

public:
  virtual void start(kernel::RunApi& api) = 0;

protected:
  void beamReceived(kernel::RunApi& api, SessionPresenceBeam&& beam);

private:
  Track* findTrack(const kernel::ProcessInfo& process);
  void refreshTrack(kernel::RunApi& api, Track* track, const SessionPresenceBeam& beam);
  void removeTrack(const kernel::ProcessInfo& process, kernel::RunApi& api);
  void notifyFound(const BeamInfo& beamInfo);
  void notifyLost(const kernel::ProcessInfo& processInfo);

private:
  FoundCallback onFound_;
  LostCallback onLost_;
  asio::io_context& io_;
  std::vector<std::unique_ptr<Track>> tracks_;
  std::recursive_mutex tracksMutex_;
  spdlog::logger* logger_;
  std::optional<::sen::Duration> beamExpirationTime_;  // optional overriden beam expiry time
};

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_BEAM_TRACKER_H
