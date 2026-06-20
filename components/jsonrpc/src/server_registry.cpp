// === server_registry.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "server_registry.h"

#include "server.h"

// sen
#include "sen/core/obj/object_source.h"

// std
#include <utility>

namespace sen::components::jsonrpc
{

namespace
{

[[nodiscard]] std::string makeServerName(ConnectionId connectionId)
{
  return "client-" + std::to_string(connectionId.get());
}

}  // namespace

ServerRegistry::ServerRegistry(std::shared_ptr<sen::ObjectSource> bus,
                               Dispatcher& dispatcher,
                               kernel::RunApi& api) noexcept
  : bus_(std::move(bus)), dispatcher_(dispatcher), api_(api)
{
}

ServerRegistry::~ServerRegistry()
{
  for (const auto& [_, server]: servers_)
  {
    bus_->remove(server);
  }
}

void ServerRegistry::onConnect(ConnectionId connectionId, std::string subject)
{
  if (servers_.find(connectionId) != servers_.end())
  {
    return;
  }
  auto server = std::make_shared<Server>(
    makeServerName(connectionId), Identity {std::move(subject)}, connectionId, dispatcher_, api_);
  bus_->add(server);
  servers_.emplace(connectionId, std::move(server));
}

void ServerRegistry::onDisconnect(ConnectionId connectionId)
{
  auto it = servers_.find(connectionId);
  if (it == servers_.end())
  {
    return;
  }
  bus_->remove(it->second);
  servers_.erase(it);
}

Server* ServerRegistry::find(ConnectionId connectionId) noexcept
{
  auto it = servers_.find(connectionId);
  return it == servers_.end() ? nullptr : it->second.get();
}

std::size_t ServerRegistry::size() const noexcept { return servers_.size(); }

}  // namespace sen::components::jsonrpc
