// === beam_tracker.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "beam_tracker.h"

// component
#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/duration.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/util.h"

// generated code
#include "stl/discovery.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// asio
#include <asio/io_context.hpp>
#include <asio/steady_timer.hpp>

// std
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace sen::components::ether
{

namespace
{

/// Returns the overriden value of the beam expiry time in case it is provided
[[nodiscard]] std::optional<::sen::Duration> computeBeamExpirationTime(::sen::Duration configuredExpirationTime)
{
  // return the expiry time if configured
  if (configuredExpirationTime != 0)
  {
    getLogger()->info("overriding beam tracker expiry time with value set in the ether discovery configuration ({} ns)",
                      configuredExpirationTime.getNanoseconds());
    return configuredExpirationTime.toChrono();
  }

  // override with env variable if defined
  if (const auto* value = std::getenv("BEAM_TRACKER_EXPIRATION_TIME_MS"); value != nullptr)
  {
    getLogger()->info("overriding beam tracker expiration time with environment variable ({} ms)", value);
    return ::sen::Duration {std::chrono::milliseconds(std::stoul(value))};
  }

  // return nullopt in case the expiry time is not overriden
  return std::nullopt;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Track
//--------------------------------------------------------------------------------------------------------------

struct Track
{
  BeamInfo info;
  asio::steady_timer timer;

  [[nodiscard]] inline static std::unique_ptr<Track> make(const SessionPresenceBeam& beam,
                                                          asio::io_context& io) noexcept
  {
    Track track {{beam.info, beam.endpoints}, asio::steady_timer(io)};
    return std::make_unique<Track>(std::move(track));
  }
};

//--------------------------------------------------------------------------------------------------------------
// BeamTrackerBase
//--------------------------------------------------------------------------------------------------------------

BeamTrackerBase::BeamTrackerBase(asio::io_context& io,
                                 FoundCallback&& onFound,
                                 LostCallback&& onLost,
                                 ::sen::Duration beamExpiryTime)
  : onFound_(std::move(onFound))
  , onLost_(std::move(onLost))
  , io_(io)
  , logger_(getLogger().get())
  , beamExpirationTime_(computeBeamExpirationTime(beamExpiryTime))
{
  SEN_ASSERT(onFound_ != nullptr);
  SEN_ASSERT(onLost_ != nullptr);
}

BeamTrackerBase::~BeamTrackerBase() = default;

void BeamTrackerBase::notifyFound(const BeamInfo& beamInfo)
{
  if (onFound_)
  {
    onFound_(beamInfo);
  }
}

void BeamTrackerBase::notifyLost(const kernel::ProcessInfo& processInfo)
{
  if (onLost_)
  {
    onLost_(processInfo);
  }
}

void BeamTrackerBase::beamReceived(kernel::RunApi& api, SessionPresenceBeam&& beam)
{
  std::scoped_lock<std::recursive_mutex> lock(tracksMutex_);

  logger_->trace("received beam {}:{}", beam.info.hostName, beam.info.sessionName);

  if (auto* existingTrack = findTrack(beam.info); existingTrack)
  {
    refreshTrack(api, existingTrack, beam);
  }
  else
  {
    logger_->trace("first time we see beam {}:{}", beam.info.hostName, beam.info.sessionName);

    tracks_.push_back(Track::make(beam, io_));

    const auto& track = tracks_.back();

    notifyFound(track->info);

    const auto trackHost = track->info.process.hostId;

    // if in another host, or in the same host, but different process
    if (const auto ourHost = sen::kernel::getHostId();
        trackHost != ourHost ||
        (trackHost == ourHost && track->info.process.processId != sen::kernel::getUniqueSenProcessId()))
    {
      logger_->trace("registering beam {}:{} in kernel", beam.info.hostName, beam.info.sessionName);

      // let the kernel know that there is a new process
      kernel::impl::remoteProcessDetected(api, track->info.process);
    }

    refreshTrack(api, track.get(), beam);
  }
}

void BeamTrackerBase::refreshTrack(kernel::RunApi& api, Track* track, const SessionPresenceBeam& beam)
{
  const auto locator = track->info.process;
  auto loseBeam = [this, locator, &api](std::error_code ec)
  {
    if (!ec)
    {
      removeTrack(locator, api);
    }
  };

  track->timer.cancel();
  track->timer.expires_after(beamExpirationTime_.value_or(beam.beamPeriod * 3).toChrono());
  track->timer.async_wait(std::move(loseBeam));
}

Track* BeamTrackerBase::findTrack(const kernel::ProcessInfo& process)
{
  std::scoped_lock<std::recursive_mutex> lock(tracksMutex_);

  for (const auto& track: tracks_)
  {
    if (track->info.process == process)
    {
      return track.get();
    }
  }

  return nullptr;
}

void BeamTrackerBase::removeTrack(const kernel::ProcessInfo& process, kernel::RunApi& api)
{
  std::scoped_lock<std::recursive_mutex> lock(tracksMutex_);

  auto sameTrack = [process](const auto& track) { return track->info.process == process; };
  auto itr = std::remove_if(tracks_.begin(), tracks_.end(), std::move(sameTrack));
  tracks_.erase(itr, tracks_.end());

  logger_->trace("lost beam {}:{}", process.hostName, process.sessionName);

  notifyLost(process);

  const auto trackHost = process.hostId;
  const auto ourHost = sen::kernel::getHostId();

  // if in another host, or in the same host, but different process
  if (trackHost != ourHost || (trackHost == ourHost && process.processId != sen::kernel::getUniqueSenProcessId()))
  {
    logger_->trace("unregistering beam {}:{} from the kernel", process.hostName, process.sessionName);

    // let the kernel know that the process is gone
    kernel::impl::remoteProcessLost(api, process);
  }
}

}  // namespace sen::components::ether
