// === server.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_JSONRPC_SERVER_H
#define SEN_COMPONENTS_JSONRPC_SERVER_H

// local
#include "dispatcher.h"  // for InterestEntry (Server holds the per-connection interest map)

// sen
#include "sen/core/base/compiler_macros.h"

// generated code
#include "stl/jsonrpc.stl.h"

// std
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace sen::kernel
{
class RunApi;
}

namespace sen::components::jsonrpc
{

class Dispatcher;

/// Concrete `JsonRpcServer` implementation: one instance per WebSocket connection on the
/// `local.jsonrpc` bus. `dispatcher` and `api` must outlive the `Server`.
class Server final: public JsonRpcServerBase
{
  SEN_NOCOPY_NOMOVE(Server)

public:
  Server(std::string name, Identity identity, ConnectionId connectionId, Dispatcher& dispatcher, kernel::RunApi& api);
  ~Server() override;

public:
  [[nodiscard]] ConnectionId connectionId() const noexcept { return connectionId_; }

  [[nodiscard]] std::unordered_map<std::string, InterestEntry>& interests() noexcept { return interests_; }
  [[nodiscard]] const std::unordered_map<std::string, InterestEntry>& interests() const noexcept { return interests_; }

  /// Type names already bundled into this connection's `interestUpdate.types` payloads. Lets the
  /// next update skip specs the client has cached. `getType` does not consult or mutate this set.
  [[nodiscard]] std::unordered_set<std::string>& knownTypes() noexcept { return knownTypes_; }

  /// Type names already shipped as schema fragments on this connection. Tracked separately
  /// from `knownTypes` because the schema is opt-in per-interest: an interest with
  /// `withSchemas=false` may have already shipped the spec without a schema, and a later
  /// interest with `withSchemas=true` needs to catch up.
  [[nodiscard]] std::unordered_set<std::string>& knownSchemas() noexcept { return knownSchemas_; }

public:  // health + introspection
  std::string pingImpl() const final;
  ::sen::kernel::StringList getTypesImpl() const final;
  TypeLookupResult getTypeImpl(const std::string& qualifiedName, const MaybeFlag& withSchema) const final;

public:  // topology
  SessionInfoList listTopologyImpl() const final;
  void subscribeTopologyImpl() final;
  void unsubscribeTopologyImpl() final;

public:  // interests
  void createInterestImpl(const std::string& interestName,
                          const std::string& query,
                          const MaybeSubscribeBlock& subscribe,
                          const MaybeFlag& withSchemas) final;
  void releaseInterestImpl(const std::string& interestName) final;
  ObjectInfos listObjectsImpl(const std::string& interestName) const final;

public:  // read / write
  std::string getPropertyImpl(const std::string& interestName,
                              const std::string& objectName,
                              const std::string& propertyName) const final;

  ObjectStateList getObjectsBatchStateImpl(const std::string& interestName,
                                           const MaybeStringList& objectNames,
                                           const MaybeStringList& propertyNames) const final;

  // STL-presence stub: the wire path is a direct arm in `Dispatcher` (typed JSON, single tick).
  void setPropertyImpl(const std::string& interestName,
                       const std::string& objectName,
                       const std::string& propertyName,
                       const std::string& value) final;

public:  // sticky subscription
  void subscribePropertyImpl(const std::string& interestName,
                             const std::string& objectName,
                             const std::string& propertyName,
                             const MaybeRateHz& maxRateHz) final;
  void unsubscribePropertyImpl(const std::string& interestName,
                               const std::string& objectName,
                               const std::string& propertyName) final;
  void subscribeEventImpl(const std::string& interestName,
                          const std::string& objectName,
                          const std::string& eventName) final;
  void unsubscribeEventImpl(const std::string& interestName,
                            const std::string& objectName,
                            const std::string& eventName) final;
  void subscribeAllImpl(const std::string& interestName,
                        const std::string& objectName,
                        const MaybeRateHz& maxRateHz) final;
  void unsubscribeAllImpl(const std::string& interestName, const std::string& objectName) final;

public:
  // STL-presence stub: same arrangement as `setProperty` - direct arm in `Dispatcher`.
  std::string invokeImpl(const std::string& interestName,
                         const std::string& objectName,
                         const std::string& methodName,
                         const std::string& argsJson) final;

private:
  Dispatcher& dispatcher_;
  kernel::RunApi& api_;
  ConnectionId connectionId_;
  std::unordered_map<std::string, InterestEntry> interests_;
  std::unordered_set<std::string> knownTypes_;
  std::unordered_set<std::string> knownSchemas_;
};

}  // namespace sen::components::jsonrpc

#endif  // SEN_COMPONENTS_JSONRPC_SERVER_H
