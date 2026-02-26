// === discovery.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_DISCOVERY_H
#define SEN_COMPONENTS_ETHER_SRC_DISCOVERY_H

#include "beam_tracker.h"
#include "beamer.h"

// sen
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"

// stl
#include "stl/configuration.stl.h"

// asio
#include <asio/io_context.hpp>

// std
#include <memory>

namespace sen::components::ether
{

// Forward declarations
class DiscoverySystem;

/// Allows reacting to discovery events
class DiscoveryEventListener
{
public:
  SEN_MOVE_ONLY(DiscoveryEventListener)

public:
  DiscoveryEventListener() = default;
  virtual ~DiscoveryEventListener();

public:
  virtual void beamFound(const BeamInfo& info) = 0;
  virtual void beamLost(const kernel::ProcessInfo& processInfo) = 0;

private:
  friend class DiscoverySystem;
  void setSource(DiscoverySystem* source);

private:
  DiscoverySystem* source_ = nullptr;
};

/// Groups all discovery-related logic
class DiscoverySystem final: public std::enable_shared_from_this<DiscoverySystem>
{
public:
  SEN_NOCOPY_NOMOVE(DiscoverySystem)

public:
  [[nodiscard]] static std::shared_ptr<DiscoverySystem> make(const Configuration& config, asio::io_context& io);
  ~DiscoverySystem();

public:
  void prepare(kernel::RunApi& api);

public:
  void addListener(DiscoveryEventListener* listener);
  void removeListener(DiscoveryEventListener* listener, bool allowCallback = true);
  void stopBeaming(const kernel::ProcessInfo& processInfo);
  void startBeaming(const std::string& sessionName, const EnpointList& endpoints);

private:
  DiscoverySystem(const Configuration& config, asio::io_context& io);
  void addBeam(const BeamInfo& info);
  void removeBeam(const kernel::ProcessInfo& processInfo);
  void clearOurselvesAsSource(DiscoveryEventListener* listener) const;

private:
  using Lock = std::scoped_lock<std::recursive_mutex>;

private:
  asio::io_context& io_;
  std::shared_ptr<BeamerBase> beamer_;
  std::shared_ptr<BeamTrackerBase> beamTracker_;
  std::vector<DiscoveryEventListener*> listeners_;
  std::vector<BeamInfo> presentBeams_;
  std::recursive_mutex usageMutex_;
};

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_DISCOVERY_H
