// === server_registry.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_JSONRPC_SERVER_REGISTRY_H
#define SEN_COMPONENTS_JSONRPC_SERVER_REGISTRY_H

// local
#include "messages.h"

// sen
#include "sen/core/base/compiler_macros.h"

// std
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>

namespace sen
{
class ObjectSource;
}

namespace sen::kernel
{
class RunApi;
}

namespace sen::components::jsonrpc
{

class Dispatcher;
class Server;

/// Per-connection lifecycle owner for `JsonRpcServer` kernel objects on the `local.jsonrpc` bus.
class ServerRegistry
{
  SEN_NOCOPY_NOMOVE(ServerRegistry)

public:
  ServerRegistry(std::shared_ptr<sen::ObjectSource> bus, Dispatcher& dispatcher, kernel::RunApi& api) noexcept;
  ~ServerRegistry();

  /// Idempotent against a duplicate `connectionId` - second call is a no-op.
  void onConnect(ConnectionId connectionId, std::string subject);

  /// No-op if the connection is unknown.
  void onDisconnect(ConnectionId connectionId);

  /// Returns the `Server` for `connectionId` or `nullptr` if unknown / already disconnected.
  [[nodiscard]] Server* find(ConnectionId connectionId) noexcept;

  [[nodiscard]] std::size_t size() const noexcept;

private:
  std::shared_ptr<sen::ObjectSource> bus_;
  Dispatcher& dispatcher_;
  kernel::RunApi& api_;
  std::unordered_map<ConnectionId, std::shared_ptr<Server>> servers_;
};

}  // namespace sen::components::jsonrpc

#endif  // SEN_COMPONENTS_JSONRPC_SERVER_REGISTRY_H
