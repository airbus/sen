// === dispatcher.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "dispatcher.h"

// local
#include "server.h"
#include "server_registry.h"
#include "topology_service.h"
#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/callback.h"
#include "sen/kernel/component_api.h"

// generated
#include "stl/sen/kernel/basic_types.stl.h"

// spdlog
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

// std
#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace sen::components::jsonrpc
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

/// Per-tick cap on inbound RPCs: `processBatch()` does exactly one bulk dequeue of this size,
/// then yields the tick to `flushPendingPropertyNotifications` and the next kernel tick. Bounds
/// worst-case tick time so one busy client cannot monopolize the dispatcher. At 60 Hz this is
/// ~7680 RPCs/s ceiling.
constexpr std::size_t inboundDrainBatchSize = 128U;

/// System-wide soft cap on outbound queue depth. When exceeded, `pushNotification` drops new
/// `reliable=false` notifications across all connections (RPC responses and reliable
/// notifications still enqueue). Catches burst-producer floods that the per-connection
/// backpressure check cannot see: dispatcher serializing faster than uWS drains, kernel-side
/// stall on the uWS thread, etc.
constexpr std::size_t outboundQueueSoftCap = 8192U;

[[nodiscard]] std::string serializeSuccess(const nlohmann::json& id, const nlohmann::json& result)
{
  return nlohmann::json {{"jsonrpc", "2.0"}, {"result", result}, {"id", id}}.dump();
}

[[nodiscard]] std::string serializeError(const nlohmann::json& id, const JsonRpcError& err)
{
  nlohmann::json envelope {
    {"jsonrpc", "2.0"}, {"error", {{"code", static_cast<int>(err.code)}, {"message", err.message}}}, {"id", id}};
  if (err.data.has_value())
  {
    envelope["error"]["data"] = err.data.value();
  }
  return envelope.dump();
}

/// Three-state outcome of validating the JSON-RPC `id` field.
struct ParsedRequestId
{
  enum class Kind
  {
    notification,  ///< `id` absent: a notification, no response expected
    request,       ///< `id` is a number / string / null: a regular request
    invalidType,   ///< `id` is present but not a valid type per the spec
  };
  Kind kind {Kind::notification};
  nlohmann::json id;  ///< valid when `kind == request`
};

[[nodiscard]] ParsedRequestId parseRequestId(const nlohmann::json& envelope)
{
  const auto it = envelope.find("id");
  if (it == envelope.end())
  {
    return {ParsedRequestId::Kind::notification, {}};
  }
  if (it->is_number() || it->is_string() || it->is_null())
  {
    return {ParsedRequestId::Kind::request, *it};
  }
  return {ParsedRequestId::Kind::invalidType, {}};
}

/// Translates an async callback failure into a JSON-RPC error envelope.
void respondWithCallbackError(const RequestContext& ctx, std::string_view methodName, std::exception_ptr error)
{
  try
  {
    std::rethrow_exception(error);
  }
  catch (const JsonRpcException& e)
  {
    ctx.respond(sen::Err(e.error()));
  }
  catch (const std::exception& e)
  {
    ctx.respond(sen::Err(
      makeError(JsonRpcErrorCode::internalError, std::string {methodName} + ": handler threw an exception", e.what())));
  }
  catch (...)
  {
    ctx.respond(
      sen::Err(makeError(JsonRpcErrorCode::internalError,
                         std::string {methodName} + ": handler threw a non-std exception (type unavailable)")));
  }
}

/// Async-invoke `method` on `target` and respond with `varToJson(result)` on success or a
/// JSON-RPC error envelope on failure. Used by the meta-dispatch path for typed STL methods.
template <typename Target>
void respondWithInvokeResult(sen::impl::WorkQueue* workQueue,
                             RequestContext ctx,
                             Target& target,
                             const sen::Method* method,
                             sen::VarList& args,
                             std::string methodName)
{
  auto sharedCtx = std::make_shared<RequestContext>(std::move(ctx));
  target.invokeUntyped(
    method,
    args,
    {workQueue,
     [sharedCtx, method, methodName = std::move(methodName)](const sen::MethodResult<sen::Var>& result)
     {
       if (result.isOk())
       {
         sharedCtx->respond(sen::Ok(varToWireJson(result.getValue(), *method->getReturnType())));
         return;
       }
       respondWithCallbackError(*sharedCtx, methodName, result.getError());
     }});
}

/// Sibling of `respondWithInvokeResult` that encodes the kernel-returned value as a JSON-encoded
/// string. Used by `invoke` to honor the STL `-> string` signature.
template <typename Target>
void respondWithEncodedInvokeResult(sen::impl::WorkQueue* workQueue,
                                    RequestContext ctx,
                                    Target& target,
                                    const sen::Method* method,
                                    sen::VarList& args,
                                    std::string methodName)
{
  auto sharedCtx = std::make_shared<RequestContext>(std::move(ctx));
  target.invokeUntyped(
    method,
    args,
    {workQueue,
     [sharedCtx, method, methodName = std::move(methodName)](const sen::MethodResult<sen::Var>& result)
     {
       if (result.isOk())
       {
         sharedCtx->respond(sen::Ok(nlohmann::json(varToWireJson(result.getValue(), *method->getReturnType()).dump())));
         return;
       }
       respondWithCallbackError(*sharedCtx, methodName, result.getError());
     }});
}

/// Reshapes a wire `params` object into a positional JSON array matching `method`'s STL arg signature.
[[nodiscard]] bool buildPositionalParams(const RequestContext& ctx,
                                         const sen::Method& method,
                                         std::string_view methodName,
                                         const nlohmann::json& params,
                                         nlohmann::json& out)
{
  out = nlohmann::json::array();
  for (const auto& metaArg: method.getArgs())
  {
    if (auto it = params.find(metaArg.name); it != params.end())
    {
      out.push_back(*it);
      continue;
    }
    if (metaArg.type->isOptionalType())
    {
      out.push_back(nullptr);
      continue;
    }
    ctx.respond(sen::Err(makeError(JsonRpcErrorCode::invalidParams,
                                   std::string {methodName} + ": missing required field: " + metaArg.name)));
    return false;
  }
  return true;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// RequestContext
//--------------------------------------------------------------------------------------------------------------

RequestContext::RequestContext(ConnectionId connectionId,
                               std::optional<nlohmann::json> requestId,
                               OutboundQueue* outbound) noexcept
  : connectionId_(connectionId), requestId_(std::move(requestId)), outbound_(outbound)
{
}

RequestContext::RequestContext(RequestContext&& other) noexcept
  : connectionId_(other.connectionId_)
  , requestId_(std::move(other.requestId_))
  , outbound_(other.outbound_)
  , responded_(other.responded_.load(std::memory_order_relaxed))
{
}

RequestContext& RequestContext::operator=(RequestContext&& other) noexcept
{
  if (this != &other)
  {
    connectionId_ = other.connectionId_;
    requestId_ = std::move(other.requestId_);
    outbound_ = other.outbound_;
    responded_.store(other.responded_.load(std::memory_order_relaxed), std::memory_order_relaxed);
  }
  return *this;
}

void RequestContext::respond(sen::Result<nlohmann::json, JsonRpcError> result) const noexcept
{
  if (!requestId_.has_value())
  {
    return;
  }
  // Atomic exchange so a racing second caller hits the assert before re-enqueuing.
  const bool already = responded_.exchange(true, std::memory_order_acq_rel);
  SEN_ASSERT(!already);
  try
  {
    std::string payload = result.isOk() ? serializeSuccess(requestId_.value(), result.getValue())
                                        : serializeError(requestId_.value(), result.getError());
    outbound_->enqueue(OutboundMessage {connectionId_, std::move(payload)});
  }
  catch (const std::exception& e)
  {
    SPDLOG_LOGGER_ERROR(getLogger(),
                        "RequestContext::respond: failed to serialize/enqueue response for connection {}: {}",
                        connectionId_.get(),
                        e.what());
  }
}

//--------------------------------------------------------------------------------------------------------------
// Dispatcher
//--------------------------------------------------------------------------------------------------------------

Dispatcher::Dispatcher(InboundQueue& inbound, OutboundQueue& outbound, kernel::RunApi& api)
  : inboundQueue_(inbound)
  , outboundQueue_(outbound)
  , api_(api)
  , inboundConsumerToken_(inbound)
  , serverRegistry_(
      std::make_unique<ServerRegistry>(api.getSource(kernel::BusAddress {"local", "jsonrpc"}), *this, api))
  , topology_(std::make_unique<TopologyService>(api, *this))
{
}

Dispatcher::~Dispatcher() = default;

std::chrono::steady_clock::time_point Dispatcher::currentTime() const
{
  // The absolute moment is irrelevant; flushObjectBundle only ever takes differences.
  return std::chrono::steady_clock::time_point {api_.getTime().sinceEpoch().toChrono()};
}

ConnectionState& Dispatcher::getOrCreateConnectionState(ConnectionId connectionId)
{
  return connections_[connectionId];
}

Server* Dispatcher::getServer(ConnectionId connectionId) noexcept { return serverRegistry_->find(connectionId); }

bool Dispatcher::enqueueOutbound(ConnectionId connectionId, std::string envelope, bool reliable)
{
  // `find` rather than `getOrCreate` so an outbound doesn't materialize state for an unknown
  // connection (treated as not-high).
  if (!reliable)
  {
    if (outboundQueue_.size_approx() > outboundQueueSoftCap)
    {
      ++outboundQueueDepthDrops_;
      if ((outboundQueueDepthDrops_ & (outboundQueueDepthDrops_ - 1U)) == 0U)
      {
        SPDLOG_LOGGER_WARN(getLogger(),
                           "Dispatcher: outbound queue soft cap ({}) exceeded, dropped {} unreliable "
                           "notifications cumulatively",
                           outboundQueueSoftCap,
                           outboundQueueDepthDrops_);
      }
      return false;
    }
    if (auto it = connections_.find(connectionId); it != connections_.end() && it->second.backpressureHigh)
    {
      ++it->second.droppedNotifications;
      return false;
    }
  }
  outboundQueue_.enqueue(OutboundMessage {connectionId, std::move(envelope)});
  return true;
}

void Dispatcher::pushNotification(ConnectionId connectionId,
                                  std::string_view method,
                                  nlohmann::json params,
                                  bool reliable)
{
  // Method names are dispatcher-controlled identifiers, so the envelope is built by string
  // append and skips JSON escaping. The assert pins that contract.
  SEN_DEBUG_ASSERT(method.find_first_of("\"\\\n\r\t") == std::string_view::npos);
  auto paramsStr = std::move(params).dump();
  std::string envelope;
  envelope.reserve(method.size() + paramsStr.size() + 32U);
  envelope.append(R"({"jsonrpc":"2.0","method":")");
  envelope.append(method);
  envelope.append(R"(","params":)");
  envelope.append(paramsStr);
  envelope.push_back('}');
  std::ignore = enqueueOutbound(connectionId, std::move(envelope), reliable);
}

void Dispatcher::processBatch()
{
  std::array<InboundMessage, inboundDrainBatchSize> batch;
  const std::size_t count = inboundQueue_.try_dequeue_bulk(inboundConsumerToken_, batch.begin(), batch.size());
  for (std::size_t i = 0; i < count; ++i)
  {
    handleInbound(std::move(batch[i]));
  }
  flushPendingPropertyNotifications();
}

void Dispatcher::markDirty(ConnectionId connId, std::string_view interestName, std::string_view objectName)
{
  dirty_.insert(DirtyKey {connId, std::string {interestName}, std::string {objectName}});
}

void Dispatcher::flushPendingPropertyNotifications()
{
  if (dirty_.empty())
  {
    return;
  }
  // Swap into a local so any re-marks during the flush land in the next tick instead of
  // mutating what we are walking. Property kernel callbacks marshal onto this same thread, so
  // no synchronization is needed.
  decltype(dirty_) toProcess;
  toProcess.swap(dirty_);
  const auto now = currentTime();

  // Sort by (connId, interestName) so consecutive keys in the same group reuse the cached
  // Server + InterestEntry pointers. flushObjectBundle never mutates the surrounding maps,
  // so the pointers stay valid for the run.
  std::vector<DirtyKey> sortedKeys(std::make_move_iterator(toProcess.begin()),
                                   std::make_move_iterator(toProcess.end()));
  std::sort(sortedKeys.begin(),
            sortedKeys.end(),
            [](const DirtyKey& a, const DirtyKey& b)
            {
              if (a.connId != b.connId)
              {
                return a.connId < b.connId;
              }
              if (a.interestName != b.interestName)
              {
                return a.interestName < b.interestName;
              }
              return a.objectName < b.objectName;
            });

  Server* server = nullptr;
  ConnectionId currentConnId {};
  bool haveConnId = false;
  InterestEntry* entry = nullptr;
  std::string currentInterestName;

  for (auto& key: sortedKeys)
  {
    if (!haveConnId || key.connId != currentConnId)
    {
      server = serverRegistry_->find(key.connId);
      currentConnId = key.connId;
      haveConnId = true;
      entry = nullptr;
      currentInterestName.clear();
    }
    if (server == nullptr)
    {
      continue;
    }

    if (entry == nullptr || key.interestName != currentInterestName)
    {
      auto& interests = server->interests();
      auto interestIt = interests.find(key.interestName);
      entry = (interestIt == interests.end()) ? nullptr : &interestIt->second;
      currentInterestName = key.interestName;
    }
    if (entry == nullptr)
    {
      continue;
    }

    auto subsIt = entry->objectSubs.find(key.objectName);
    if (subsIt == entry->objectSubs.end())
    {
      continue;
    }
    auto& subs = subsIt->second;
    flushObjectBundle(key.connId, key.interestName, key.objectName, subs, now);
    if (!subs.pendingValues.empty())
    {
      // Throttle window hadn't elapsed - keep this entry dirty so the next tick re-checks.
      dirty_.insert(std::move(key));
    }
  }
}

void Dispatcher::flushObjectBundle(ConnectionId connId,
                                   const std::string& interestName,
                                   const std::string& objectName,
                                   ObjectSubs& subs,
                                   std::chrono::steady_clock::time_point now)
{
  if (subs.pendingValues.empty())
  {
    return;
  }
  if (subs.interval.getNanoseconds() > 0 && subs.lastEmitTime.has_value() &&
      now - *subs.lastEmitTime < subs.interval.toChrono())
  {
    return;
  }
  // Reliable iff any wired property guard is `confirmed`. Derived from the cached per-guard
  // flag so the answer cannot drift from the active set.
  const bool reliable = std::any_of(subs.activePropertyGuards.begin(),
                                    subs.activePropertyGuards.end(),
                                    [](const auto& kv) { return kv.second.reliable; });

  // Hand-build the envelope to skip per-flush nlohmann::json allocation. Trust model:
  // property names are kernel-meta identifiers (splice raw); interest + object names come
  // from the wire (escape via nlohmann::json(...).dump()); values are already-validated
  // JSON blobs (splice their dump()).
  std::string envelope;
  envelope.reserve(128U + subs.pendingValues.size() * 64U);
  envelope.append(R"({"jsonrpc":"2.0","method":"propertyChanged","params":{"interestName":)");
  envelope.append(nlohmann::json(interestName).dump());
  envelope.append(R"(,"objectName":)");
  envelope.append(nlohmann::json(objectName).dump());
  envelope.append(R"(,"values":[)");
  bool firstValue = true;
  for (const auto& [propertyName, value]: subs.pendingValues)
  {
    if (!firstValue)
    {
      envelope.push_back(',');
    }
    firstValue = false;
    envelope.append(R"({"propertyName":")");
    envelope.append(propertyName);
    envelope.append(R"(","value":)");
    envelope.append(nlohmann::json(value.dump()).dump());
    envelope.push_back('}');
  }
  envelope.append(R"(],"timestamp":")");
  envelope.append(subs.latestChangeTime.toUtcStringNs());
  envelope.append(R"("}})");
  std::ignore = enqueueOutbound(connId, std::move(envelope), reliable);
  subs.lastEmitTime = now;
  subs.pendingValues.clear();
}

void Dispatcher::handleInbound(InboundMessage&& msg)
{
  const auto connId = msg.connectionId;
  std::visit(
    [this, connId](auto&& payload)
    {
      using T = std::decay_t<decltype(payload)>;
      if constexpr (std::is_same_v<T, std::string>)
      {
        handleTextFrame(connId, payload);
      }
      else if constexpr (std::is_same_v<T, ClientConnected>)
      {
        handleClientConnected(connId, std::move(payload));
      }
      else if constexpr (std::is_same_v<T, ClientDisconnected>)
      {
        handleClientDisconnected(connId);
      }
      else if constexpr (std::is_same_v<T, BackpressureUpdate>)
      {
        handleBackpressureUpdate(connId, payload);
      }
    },
    std::move(msg.payload));
}

void Dispatcher::handleClientConnected(ConnectionId connectionId, ClientConnected&& payload)
{
  serverRegistry_->onConnect(connectionId, std::move(payload.identity.subject));
}

void Dispatcher::handleClientDisconnected(ConnectionId connectionId)
{
  topology_->unsubscribe(connectionId);
  serverRegistry_->onDisconnect(connectionId);
  connections_.erase(connectionId);
}

void Dispatcher::handleBackpressureUpdate(ConnectionId connectionId, BackpressureUpdate payload)
{
  const bool nowHigh = payload.bufferedAmount > 0;
  if (!nowHigh)
  {
    auto it = connections_.find(connectionId);
    if (it == connections_.end())
    {
      return;
    }
  }
  auto& state = getOrCreateConnectionState(connectionId);
  const bool wasHigh = state.backpressureHigh;
  state.backpressureHigh = nowHigh;
  if (!wasHigh && nowHigh)
  {
    SPDLOG_LOGGER_INFO(getLogger(),
                       "Dispatcher: connection {} entering backpressure ({} bytes buffered)",
                       connectionId.get(),
                       payload.bufferedAmount);
  }
  if (wasHigh && !nowHigh)
  {
    SPDLOG_LOGGER_INFO(getLogger(),
                       "Dispatcher: connection {} backpressure cleared ({} drops during the high window)",
                       connectionId.get(),
                       state.droppedNotifications);
  }
  if (wasHigh && !nowHigh && state.droppedNotifications > 0)
  {
    const auto count = state.droppedNotifications;
    state.droppedNotifications = 0;
    pushNotification(connectionId, "notificationsDropped", nlohmann::json {{"count", count}}, /*reliable=*/true);
  }
}

void Dispatcher::handleTextFrame(ConnectionId connectionId, std::string_view frame)
{
  nlohmann::json envelope;
  try
  {
    envelope = nlohmann::json::parse(frame);
  }
  catch (const nlohmann::json::parse_error& e)
  {
    RequestContext ctx {connectionId, nlohmann::json(nullptr), &outboundQueue_};
    ctx.respond(sen::Err(JsonRpcError {JsonRpcErrorCode::parseError, "Parse error", e.what()}));
    return;
  }

  // Batch support is deferred; arrays are rejected outright rather than parsed.
  if (!envelope.is_object())
  {
    RequestContext ctx {connectionId, nlohmann::json(nullptr), &outboundQueue_};
    ctx.respond(sen::Err(makeError(JsonRpcErrorCode::invalidRequest, "Invalid Request")));
    return;
  }

  const auto parsedId = parseRequestId(envelope);
  if (parsedId.kind == ParsedRequestId::Kind::invalidType)
  {
    RequestContext ctx {connectionId, nlohmann::json(nullptr), &outboundQueue_};
    ctx.respond(
      sen::Err(makeError(JsonRpcErrorCode::invalidRequest, "Invalid Request: 'id' must be number, string, or null")));
    return;
  }
  const auto requestId = parsedId.kind == ParsedRequestId::Kind::request ? std::optional<nlohmann::json> {parsedId.id}
                                                                         : std::optional<nlohmann::json> {};

  RequestContext ctx {connectionId, requestId, &outboundQueue_};

  const auto versionIt = envelope.find("jsonrpc");
  if (versionIt == envelope.end() || !versionIt->is_string() || versionIt->get_ref<const std::string&>() != "2.0")
  {
    ctx.respond(sen::Err(makeError(JsonRpcErrorCode::invalidRequest, "Invalid Request: 'jsonrpc' must be \"2.0\"")));
    return;
  }

  const auto methodIt = envelope.find("method");
  if (methodIt == envelope.end() || !methodIt->is_string())
  {
    ctx.respond(
      sen::Err(makeError(JsonRpcErrorCode::invalidRequest, "Invalid Request: missing or non-string 'method'")));
    return;
  }
  const auto& methodName = methodIt->get_ref<const std::string&>();

  nlohmann::json params = nlohmann::json::object();
  if (auto paramsIt = envelope.find("params"); paramsIt != envelope.end())
  {
    params = *paramsIt;
  }

  // ClientConnected creates a Server before any text frame can arrive; missing it is structural.
  auto* server = serverRegistry_->find(ctx.connectionId());
  if (server == nullptr)
  {
    ctx.respond(sen::Err(makeError(JsonRpcErrorCode::internalError, "no JsonRpcServer for this connection")));
    return;
  }

  // setProperty + invoke bypass dispatchViaMeta to thread the async kernel-callback response
  // back in the same tick. See architecture.md "Method dispatch".
  if (methodName == "getProperty")
  {
    handleGetProperty(std::move(ctx), *server, params);
    return;
  }
  if (methodName == "setProperty")
  {
    handleSetProperty(std::move(ctx), *server, params);
    return;
  }
  if (methodName == "invoke")
  {
    handleInvoke(std::move(ctx), *server, params);
    return;
  }
  dispatchViaMeta(std::move(ctx), *server, methodName, params);
}

void Dispatcher::handleGetProperty(RequestContext ctx, Server& server, const nlohmann::json& params)
{
  auto fields = requireStringFields(ctx, params, "getProperty", "interestName", "objectName", "propertyName");
  if (!fields)
  {
    return;
  }
  auto& [interestName, objectName, propertyName] = *fields;
  try
  {
    // STL `getProperty -> string`: send the JSON-encoded value verbatim.
    auto encoded = server.getPropertyImpl(interestName, objectName, propertyName);
    ctx.respond(sen::Ok(nlohmann::json(std::move(encoded))));
  }
  catch (const JsonRpcException& e)
  {
    ctx.respond(sen::Err(e.error()));
  }
  catch (const std::exception& e)
  {
    ctx.respond(
      sen::Err(makeError(JsonRpcErrorCode::internalError, "getProperty: handler threw an exception", e.what())));
  }
}

void Dispatcher::handleSetProperty(RequestContext ctx, Server& server, const nlohmann::json& params)
{
  auto fields = requireStringFields(ctx, params, "setProperty", "interestName", "objectName", "propertyName");
  if (!fields)
  {
    return;
  }
  auto& [interestName, objectName, propertyName] = *fields;
  // STL `setProperty(... value: string)`: `value` arrives as a JSON-encoded string. Reject any
  // other shape so the contract matches the STL declaration; parse the inner string into the
  // structured value the setter expects.
  const auto valueIt = params.find("value");
  if (valueIt == params.end() || !valueIt->is_string())
  {
    ctx.respond(sen::Err(makeError(JsonRpcErrorCode::invalidParams,
                                   "setProperty: requires {interestName, objectName, propertyName, value: string}")));
    return;
  }
  nlohmann::json valueJson;
  try
  {
    valueJson = nlohmann::json::parse(valueIt->get_ref<const std::string&>());
  }
  catch (const nlohmann::json::parse_error& e)
  {
    ctx.respond(
      sen::Err(makeError(JsonRpcErrorCode::invalidParams, "setProperty: 'value' is not valid JSON", e.what())));
    return;
  }
  std::shared_ptr<sen::Object> object;
  const sen::Method* setter = nullptr;
  try
  {
    auto& entry = requireInterest(server, "setProperty", interestName);
    object = requireObjectInInterest(entry, "setProperty", objectName);
    const sen::Property& property = requireProperty(*object, "setProperty", propertyName);
    if (property.getCategory() != sen::PropertyCategory::dynamicRW)
    {
      const char* reason;
      switch (property.getCategory())
      {
        case sen::PropertyCategory::staticRW:
          reason = "staticRW (configurable at YAML load time, not at runtime)";
          break;
        case sen::PropertyCategory::staticRO:
          reason = "staticRO (set in code only)";
          break;
        case sen::PropertyCategory::dynamicRO:
          reason = "dynamicRO (producer-only)";
          break;
        default:
          reason = "not runtime-writable";
          break;
      }
      throw JsonRpcException(JsonRpcErrorCode::notWritable, "setProperty: '" + propertyName + "' is " + reason);
    }
    setter = &property.getSetterMethod();
  }
  catch (const JsonRpcException& e)
  {
    ctx.respond(sen::Err(e.error()));
    return;
  }

  // Wrap in a one-element array so coercion goes through `parseInvokeArgs`, identical to invoke.
  auto argsResult = parseInvokeArgs(nlohmann::json::array({std::move(valueJson)}), *setter, "setProperty");
  if (argsResult.isError())
  {
    ctx.respond(sen::Err(std::move(argsResult).getError()));
    return;
  }
  auto args = std::move(argsResult).getValue();
  auto sharedCtx = std::make_shared<RequestContext>(std::move(ctx));
  object->invokeUntyped(setter,
                        args,
                        {api_.getWorkQueue(),
                         [sharedCtx](const sen::MethodResult<sen::Var>& result)
                         {
                           if (result.isOk())
                           {
                             sharedCtx->respond(sen::Ok(nlohmann::json(nullptr)));
                             return;
                           }
                           respondWithCallbackError(*sharedCtx, "setProperty", result.getError());
                         }});
}

void Dispatcher::handleInvoke(RequestContext ctx, Server& server, const nlohmann::json& params)
{
  auto fields = requireStringFields(ctx, params, "invoke", "interestName", "objectName", "methodName");
  if (!fields)
  {
    return;
  }
  auto& [interestName, objectName, methodNameField] = *fields;
  std::shared_ptr<sen::Object> object;
  const sen::Method* method = nullptr;
  try
  {
    auto& entry = requireInterest(server, "invoke", interestName);
    object = requireObjectInInterest(entry, "invoke", objectName);
    method = object->getClass()->searchMethodByName(methodNameField);
    if (method == nullptr)
    {
      throw JsonRpcException(JsonRpcErrorCode::methodNotFound, "invoke: method not found: " + methodNameField);
    }
  }
  catch (const JsonRpcException& e)
  {
    ctx.respond(sen::Err(e.error()));
    return;
  }

  nlohmann::json argsField = nlohmann::json::array();
  if (auto argsIt = params.find("argsJson"); argsIt != params.end())
  {
    if (!argsIt->is_string())
    {
      ctx.respond(sen::Err(
        makeError(JsonRpcErrorCode::invalidParams, "invoke: 'argsJson' must be a string of JSON-encoded args")));
      return;
    }
    try
    {
      argsField = nlohmann::json::parse(argsIt->get_ref<const std::string&>());
    }
    catch (const nlohmann::json::parse_error& e)
    {
      ctx.respond(
        sen::Err(makeError(JsonRpcErrorCode::invalidParams, "invoke: 'argsJson' is not valid JSON", e.what())));
      return;
    }
  }
  auto argsResult = parseInvokeArgs(argsField, *method, "invoke");
  if (argsResult.isError())
  {
    ctx.respond(sen::Err(std::move(argsResult).getError()));
    return;
  }
  auto args = std::move(argsResult).getValue();
  respondWithEncodedInvokeResult(api_.getWorkQueue(), std::move(ctx), *object, method, args, "invoke");
}

void Dispatcher::dispatchViaMeta(RequestContext ctx,
                                 Server& server,
                                 std::string_view methodName,
                                 const nlohmann::json& params)
{
  const sen::Method* method = server.getClass()->searchMethodByName(methodName);
  if (method == nullptr)
  {
    ctx.respond(sen::Err(makeError(JsonRpcErrorCode::methodNotFound, "Method not found: " + std::string {methodName})));
    return;
  }
  nlohmann::json positional;
  if (!buildPositionalParams(ctx, *method, methodName, params, positional))
  {
    return;
  }
  auto argsResult = parseInvokeArgs(positional, *method, methodName);
  if (argsResult.isError())
  {
    ctx.respond(sen::Err(std::move(argsResult).getError()));
    return;
  }
  auto args = std::move(argsResult).getValue();
  respondWithInvokeResult(api_.getWorkQueue(), std::move(ctx), server, method, args, std::string {methodName});
}

}  // namespace sen::components::jsonrpc
