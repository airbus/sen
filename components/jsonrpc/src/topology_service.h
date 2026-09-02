// === topology_service.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_JSONRPC_TOPOLOGY_SERVICE_H
#define SEN_COMPONENTS_JSONRPC_TOPOLOGY_SERVICE_H

// local
#include "messages.h"  // for ConnectionId

// generated
#include "stl/jsonrpc.stl.h"  // for SessionInfoList

// sen
#include "sen/core/base/class_helpers.h"

// nlohmann
#include "nlohmann/json.hpp"

// std
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace sen::kernel
{
class RunApi;
class SessionInfoProvider;
}  // namespace sen::kernel

namespace sen::components::jsonrpc
{

class Dispatcher;

/// One per process. Mirrors the kernel's session/bus discovery surface and fans changes out
/// to every connection that has called `subscribeTopology`. Lives for the dispatcher's
/// lifetime so the kernel callbacks (which can only carry one callback per `SourceInfo`)
/// have a stable single owner.
///
/// Threading: constructed, called, and destroyed on the dispatcher thread. The kernel fires
/// the discovery callbacks from `drainInputs()` on the same thread, so no marshalling.
class TopologyService
{
public:
  SEN_NOCOPY_NOMOVE(TopologyService)

  TopologyService(kernel::RunApi& api, Dispatcher& dispatcher);
  ~TopologyService();

public:
  /// Snapshot of the currently-known sessions and their buses, as the typed value the
  /// `listTopology` impl returns.
  [[nodiscard]] SessionInfoList snapshot() const;

  /// Add a connection to the push set; immediately delivers one `topologyChanged` to it
  /// with the current snapshot. Idempotent.
  void subscribe(ConnectionId connId);

  /// Remove a connection from the push set. Idempotent.
  void unsubscribe(ConnectionId connId);

private:
  void onSessionDetected(const std::string& sessionName);
  void onSessionUndetected(const std::string& sessionName);
  void onBusChanged();
  void broadcast();

  /// Attaches / detaches the per-provider bus-change callbacks. Called when subscribers_
  /// transitions empty <-> non-empty so the providers don't pay the per-event callback cost
  /// when nobody is listening.
  void wireProviderCallbacks();
  void unwireProviderCallbacks();

  kernel::RunApi& api_;
  Dispatcher& dispatcher_;
  std::unordered_map<std::string, std::shared_ptr<sen::kernel::SessionInfoProvider>> providers_;
  std::unordered_set<ConnectionId> subscribers_;
};

}  // namespace sen::components::jsonrpc

#endif  // SEN_COMPONENTS_JSONRPC_TOPOLOGY_SERVICE_H
