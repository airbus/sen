// === dispatcher.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_JSONRPC_DISPATCHER_H
#define SEN_COMPONENTS_JSONRPC_DISPATCHER_H

// local
#include "auth.h"
#include "messages.h"

// libs
#include "sen/gen/json.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/result.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/obj/connection_guard.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/subscription.h"

// nlohmann
#include "nlohmann/json.hpp"

// std
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace sen::kernel
{
class KernelApi;
class RunApi;
}  // namespace sen::kernel

namespace sen::components::jsonrpc
{

class Server;
class ServerRegistry;
class TopologyService;

/// Reply handle for one JSON-RPC request. `respond()` must be called exactly once for requests
/// and is a no-op for notifications. Safe to call from any thread; the atomic `responded_`
/// makes the second call abort on `SEN_ASSERT`.
class RequestContext
{
public:
  RequestContext(ConnectionId connectionId, std::optional<nlohmann::json> requestId, OutboundQueue* outbound) noexcept;

  // Move-only: std::atomic<bool> is non-copyable.
  RequestContext(const RequestContext&) = delete;
  RequestContext& operator=(const RequestContext&) = delete;
  RequestContext(RequestContext&& other) noexcept;
  RequestContext& operator=(RequestContext&& other) noexcept;
  ~RequestContext() = default;

public:
  void respond(sen::Result<nlohmann::json, JsonRpcError> result) const noexcept;
  [[nodiscard]] ConnectionId connectionId() const noexcept { return connectionId_; }
  [[nodiscard]] bool isNotification() const noexcept { return !requestId_.has_value(); }

private:
  ConnectionId connectionId_;
  std::optional<nlohmann::json> requestId_;
  OutboundQueue* outbound_;
  mutable std::atomic<bool> responded_ {false};  // mutable so respond() stays const
};

/// Wire subscription for one property: the kernel guard plus a cached `reliable` flag taken
/// from the property's transport mode, so the flush path needs no meta lookup.
struct PropertyGuard
{
  sen::ConnectionGuard guard;
  bool reliable {false};
};

/// Per-object subscription state. Sticky: the requested sets persist across object churn while
/// the active guards track only the currently matched class members.
///
/// A requested member has one of three provenances: an explicit per-name subscribe*
/// (tracked in explicitProperties / explicitEvents), a subscribeAll wildcard
/// (wildcardActive), or the interest's standing subscribe block - which lands in the
/// requested set only, deliberately neither explicit nor wildcard, so a manual
/// unsubscribe drops it while a later object re-add re-applies the block.
///
/// Invariants (enforced by `checkObjectSubsInvariants` under SEN_DEBUG_ASSERT):
///   - explicitProperties is a subset of requestedProperties
///   - explicitEvents     is a subset of requestedEvents
///   - activePropertyGuards.keys() is a subset of requestedProperties
///   - activeEventGuards.keys()    is a subset of requestedEvents
///   - dirtyProperties.keys()      is a subset of requestedProperties
struct ObjectSubs
{
  std::unordered_set<std::string> requestedProperties;
  std::unordered_set<std::string> requestedEvents;
  std::unordered_set<std::string> explicitProperties;
  std::unordered_set<std::string> explicitEvents;
  bool wildcardActive {false};
  sen::Duration interval {};
  std::optional<std::chrono::steady_clock::time_point> lastEmitTime;
  /// Properties whose value changed since the last flush. Only the names are recorded
  /// here; the values are read at flush time through `object`. Reading them inside the
  /// property-changed callback would take the object's reader lock while the callback
  /// lock is held - the reverse of the commit path's order (object lock, then callback
  /// lock) and therefore a potential deadlock.
  std::unordered_map<std::string, const sen::Property*> dirtyProperties;
  /// The object the dirty properties belong to; resolved at flush time.
  std::weak_ptr<sen::Object> object;
  sen::TimeStamp latestChangeTime;
  std::unordered_map<std::string, PropertyGuard> activePropertyGuards;
  std::unordered_map<std::string, sen::ConnectionGuard> activeEventGuards;
};

/// Selector for the `createInterest` "subscribe" block: a named list, the wildcard `"*"`, or none.
struct ParsedMemberSelector
{
  enum class Kind
  {
    none,
    wildcard,
    named,
  };

  Kind kind {Kind::none};
  std::vector<std::string> names;  ///< populated only when `kind == named`
};

/// Standing subscription template attached to an interest.
struct InterestSubscribeBlock
{
  ParsedMemberSelector properties;
  ParsedMemberSelector events;
  sen::Duration interval {};
};

/// Kernel-side state for one `createInterest`.
struct InterestEntry
{
  std::unordered_map<std::string, ObjectSubs> objectSubs;
  std::optional<InterestSubscribeBlock> subscribeBlock;
  std::shared_ptr<sen::Subscription<sen::Object>> subscription;
  std::unordered_map<std::string, std::weak_ptr<sen::Object>> objectsByName;
  bool withSchemas {false};  // when true, `interestUpdate.typeSchemas` is populated.
};

/// Per-connection transport state owned by the dispatcher.
struct ConnectionState
{
  bool backpressureHigh {false};
  std::size_t droppedNotifications {0};
};

/// Consumer of the inbound queue, producer of the outbound queue.
class Dispatcher
{
  SEN_NOCOPY_NOMOVE(Dispatcher)

public:
  Dispatcher(InboundQueue& inbound, OutboundQueue& outbound, kernel::RunApi& api);
  ~Dispatcher();

  /// Drains a bounded batch from the inbound queue. Bounded so a flood cannot starve the tick.
  void processBatch();

  [[nodiscard]] ConnectionState& getOrCreateConnectionState(ConnectionId connectionId);

  /// Returns the per-connection `Server`, or null before `ClientConnected` / after disconnect.
  [[nodiscard]] Server* getServer(ConnectionId connectionId) noexcept;

  /// Shared JSON-Schema generator. Templates parse once at startup; all rendering happens on
  /// the run() thread (same as the rest of the dispatcher), so no synchronization.
  [[nodiscard]] sen::gen::JsonGenerator& jsonGenerator() noexcept { return jsonGenerator_; }

  /// Pushes a JSON-RPC notification onto the outbound queue.
  void pushNotification(ConnectionId connectionId,
                        std::string_view method,
                        nlohmann::json params,
                        bool reliable = false);

  /// Marks an `ObjectSubs` as having pending property values that need flushing.
  void markDirty(ConnectionId connId, std::string_view interestName, std::string_view objectName);

  /// Per-process session/bus topology tracker. Servers call into it from their
  /// `subscribeTopology` / `unsubscribeTopology` / `listTopology` impls.
  [[nodiscard]] TopologyService& topology() noexcept { return *topology_; }

private:
  // Plain aggregate used as a hash-map key; the public members are the key itself and
  // the member functions only implement the map contract.
  // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
  struct DirtyKey
  {
    ConnectionId connId;
    std::string interestName;
    std::string objectName;
    bool operator==(const DirtyKey& other) const noexcept
    {
      return connId == other.connId && interestName == other.interestName && objectName == other.objectName;
    }
  };
  // NOLINTEND(misc-non-private-member-variables-in-classes)

  struct DirtyKeyHash
  {
    std::size_t operator()(const DirtyKey& key) const noexcept
    {
      return sen::hashCombine(sen::hashSeed, key.connId.get(), key.interestName, key.objectName);
    }
  };

private:
  void handleInbound(InboundMessage&& msg);
  void handleTextFrame(ConnectionId connectionId, std::string_view frame);
  void handleClientConnected(ConnectionId connectionId, ClientConnected&& payload);
  void handleClientDisconnected(ConnectionId connectionId);
  void handleBackpressureUpdate(ConnectionId connectionId, BackpressureUpdate payload);
  void handleGetProperty(RequestContext ctx, Server& server, const nlohmann::json& params);
  void handleSetProperty(RequestContext ctx, Server& server, const nlohmann::json& params);
  void handleInvoke(RequestContext ctx, Server& server, const nlohmann::json& params);
  void dispatchViaMeta(RequestContext ctx, Server& server, std::string_view methodName, const nlohmann::json& params);
  void flushPendingPropertyNotifications();
  void flushObjectBundle(ConnectionId connId,
                         const std::string& interestName,
                         const std::string& objectName,
                         ObjectSubs& subs,
                         std::chrono::steady_clock::time_point now);

  /// Drops on backpressure if `!reliable`; otherwise enqueues. Returns whether the frame was kept.
  bool enqueueOutbound(ConnectionId connectionId, std::string envelope, bool reliable);

  /// Virtual time from `api_.getTime()` so tests under `TestKernel` advance deterministically.
  [[nodiscard]] std::chrono::steady_clock::time_point currentTime() const;

private:
  InboundQueue& inboundQueue_;
  OutboundQueue& outboundQueue_;
  kernel::RunApi& api_;
  moodycamel::ConsumerToken inboundConsumerToken_;
  std::unordered_map<ConnectionId, ConnectionState> connections_;
  std::unique_ptr<ServerRegistry> serverRegistry_;
  std::unique_ptr<TopologyService> topology_;
  std::unordered_set<DirtyKey, DirtyKeyHash> dirty_;
  std::size_t outboundQueueDepthDrops_ {0};
  sen::gen::JsonGenerator jsonGenerator_;
};

}  // namespace sen::components::jsonrpc

#endif  // SEN_COMPONENTS_JSONRPC_DISPATCHER_H
