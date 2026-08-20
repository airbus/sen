// === topology_service.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "topology_service.h"

// local
#include "dispatcher.h"
#include "messages.h"
#include "util.h"

// sen
#include "sen/core/io/util.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/source_info.h"

// generated
#include "stl/jsonrpc.stl.h"

// std
#include <iterator>
#include <string>
#include <utility>

namespace sen::components::jsonrpc
{

TopologyService::TopologyService(kernel::RunApi& api, Dispatcher& dispatcher): api_(api), dispatcher_(dispatcher)
{
  auto& discoverer = api_.getSessionsDiscoverer();
  // SourceInfo holds at most one callback per kind, so the dispatcher-level singleton is the
  // only sustainable owner. Registering immediately fires for every already-known session.
  discoverer.onSourceDetected([this](const std::string& name) { onSessionDetected(name); });
  discoverer.onSourceUndetected([this](const std::string& name) { onSessionUndetected(name); });
}

TopologyService::~TopologyService()
{
  auto& discoverer = api_.getSessionsDiscoverer();
  discoverer.onSourceDetected({});
  discoverer.onSourceUndetected({});
  for (auto& [_, provider]: providers_)
  {
    provider->onSourceDetected({});
    provider->onSourceUndetected({});
  }
}

SessionInfoList TopologyService::snapshot() const
{
  SessionInfoList sessions;
  sessions.reserve(providers_.size());
  for (const auto& [sessionName, provider]: providers_)
  {
    auto buses = provider->getDetectedSources();
    sessions.emplace_back(SessionInfo {
      sessionName,
      ::sen::kernel::StringList(std::make_move_iterator(buses.begin()), std::make_move_iterator(buses.end()))});
  }
  return sessions;
}

namespace
{
[[nodiscard]] nlohmann::json buildTopologyEnvelope(const SessionInfoList& sessions)
{
  return nlohmann::json {
    {"sessions", varToWireJson(sen::toVariant(sessions), *sen::MetaTypeTrait<SessionInfoList>::meta())}};
}
}  // namespace

void TopologyService::subscribe(ConnectionId connId)
{
  const bool firstSubscriber = subscribers_.empty();
  subscribers_.insert(connId);
  if (firstSubscriber)
  {
    wireProviderCallbacks();
  }
  // Deliver the initial snapshot to just this connection so it has authoritative state
  // even if it subscribes between topology changes.
  dispatcher_.pushNotification(connId, "topologyChanged", buildTopologyEnvelope(snapshot()), /*reliable=*/true);
}

void TopologyService::unsubscribe(ConnectionId connId)
{
  if (subscribers_.erase(connId) != 0U && subscribers_.empty())
  {
    unwireProviderCallbacks();
  }
}

void TopologyService::onSessionDetected(const std::string& sessionName)
{
  auto provider = api_.getSessionsDiscoverer().makeSessionInfoProvider(sessionName);
  if (!subscribers_.empty())
  {
    provider->onSourceDetected([this](const std::string&) { onBusChanged(); });
    provider->onSourceUndetected([this](const std::string&) { onBusChanged(); });
  }
  providers_[sessionName] = std::move(provider);
  broadcast();
}

void TopologyService::wireProviderCallbacks()
{
  for (auto& [_, provider]: providers_)
  {
    provider->onSourceDetected([this](const std::string&) { onBusChanged(); });
    provider->onSourceUndetected([this](const std::string&) { onBusChanged(); });
  }
}

void TopologyService::unwireProviderCallbacks()
{
  for (auto& [_, provider]: providers_)
  {
    provider->onSourceDetected({});
    provider->onSourceUndetected({});
  }
}

void TopologyService::onSessionUndetected(const std::string& sessionName)
{
  providers_.erase(sessionName);
  broadcast();
}

void TopologyService::onBusChanged() { broadcast(); }

void TopologyService::broadcast()
{
  if (subscribers_.empty())
  {
    return;
  }
  auto payload = buildTopologyEnvelope(snapshot());
  for (auto connId: subscribers_)
  {
    dispatcher_.pushNotification(connId, "topologyChanged", payload, /*reliable=*/true);
  }
}

}  // namespace sen::components::jsonrpc
