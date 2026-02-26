// === discovery.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "discovery.h"

// component
#include "multicast_beam_tracker.h"
#include "multicast_beamer.h"
#include "tcp_beam_tracker.h"
#include "tcp_beamer.h"
#include "util.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/configuration.stl.h"
#include "stl/discovery.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <asio/io_context.hpp>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <variant>

namespace sen::components::ether
{

//--------------------------------------------------------------------------------------------------------------
// DiscoveryEventListener
//--------------------------------------------------------------------------------------------------------------

DiscoveryEventListener::~DiscoveryEventListener()
{
  if (source_)
  {
    source_->removeListener(this, false);  // do not allow callbacks
  }
}

void DiscoveryEventListener::setSource(DiscoverySystem* source) { source_ = source; }

//--------------------------------------------------------------------------------------------------------------
// DiscoverySystem
//--------------------------------------------------------------------------------------------------------------

std::shared_ptr<DiscoverySystem> DiscoverySystem::make(const Configuration& config, asio::io_context& io)
{
  return std::shared_ptr<DiscoverySystem>(new DiscoverySystem(config, io));
}

DiscoverySystem::DiscoverySystem(const Configuration& config, asio::io_context& io): io_(io)
{
  auto found = [this](const BeamInfo& info) { addBeam(info); };
  auto lost = [this](const kernel::ProcessInfo& processInfo) { removeBeam(processInfo); };

  std::visit(
    ::sen::Overloaded {
      [&](const MulticastDiscovery& /*discovery*/)
      {
        beamTracker_ = std::make_shared<MulticastBeamTracker>(io_, config, std::move(found), std::move(lost));

        beamer_ = std::make_shared<MulticastBeamer>(io_, config);
      },
      [&](const TcpDiscovery& discovery)
      {
        auto tracker = std::make_shared<TcpBeamTracker>(io_, config, std::move(found), std::move(lost));
        auto beamer = std::make_shared<TcpBeamer>(io_, tracker->getSocket(), discovery.beamPeriod, config);

        tracker->onConnected([beamerPtr = beamer.get()]() { beamerPtr->setConnected(true); });
        tracker->onDisconnected([beamerPtr = beamer.get()]() { beamerPtr->setConnected(false); });

        beamTracker_ = tracker;
        beamer_ = beamer;
      }},
    config.discovery);
}

DiscoverySystem::~DiscoverySystem()
{
  beamTracker_.reset();
  beamer_.reset();

  for (auto* listener: listeners_)
  {
    clearOurselvesAsSource(listener);
  }
}

void DiscoverySystem::startBeaming(const std::string& sessionName, const EnpointList& endpoints)
{
  beamer_->startBeaming(sessionName, endpoints);
}

void DiscoverySystem::stopBeaming(const kernel::ProcessInfo& processInfo) { beamer_->stopBeaming(processInfo); }

void DiscoverySystem::addListener(DiscoveryEventListener* listener)
{
  Lock lock(usageMutex_);

  listener->setSource(this);
  listeners_.push_back(listener);

  // notify about existing beams
  for (const auto& beam: presentBeams_)
  {
    listener->beamFound(beam);
  }
}

void DiscoverySystem::removeListener(DiscoveryEventListener* listener, bool allowCallback)
{
  Lock lock(usageMutex_);

  listeners_.erase(std::remove(listeners_.begin(), listeners_.end(), listener), listeners_.end());

  if (allowCallback)
  {
    clearOurselvesAsSource(listener);
  }
}

void DiscoverySystem::prepare(kernel::RunApi& api)
{
  beamer_->start(api);
  beamTracker_->start(api);
}

void DiscoverySystem::addBeam(const BeamInfo& info)
{
  Lock lock(usageMutex_);
  presentBeams_.push_back(info);

  for (auto* listener: listeners_)
  {
    listener->beamFound(info);
  }
}

void DiscoverySystem::removeBeam(const kernel::ProcessInfo& processInfo)
{
  Lock lock(usageMutex_);
  for (auto* listener: listeners_)
  {
    listener->beamLost(processInfo);
  }

  auto isSame = [&](const auto& elem) { return elem.process == processInfo; };
  auto& list = presentBeams_;
  list.erase(std::remove_if(list.begin(), list.end(), isSame), list.end());
}

void DiscoverySystem::clearOurselvesAsSource(DiscoveryEventListener* listener) const
{
  for (const auto& beam: presentBeams_)
  {
    listener->beamLost(beam.process);
  }
  listener->setSource(nullptr);
}

}  // namespace sen::components::ether
