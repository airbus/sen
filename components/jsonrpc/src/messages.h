// === messages.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_JSONRPC_MESSAGES_H
#define SEN_COMPONENTS_JSONRPC_MESSAGES_H

// local
#include "auth.h"

// sen
#include "sen/core/base/strong_type.h"

// concurrentqueue
#include <concurrentqueue.h>

// nlohmann
#include "nlohmann/json.hpp"

// std
#include <cstddef>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace sen::components::jsonrpc
{

SEN_STRONG_TYPE(ConnectionId, uint64_t)

/// Synthetic message carrying the authenticated `Identity`, emitted immediately after upgrade and
/// consumed before any inbound text frame.
struct ClientConnected
{
  Identity identity;
};

struct ClientDisconnected
{
};

struct BackpressureUpdate
{
  std::size_t bufferedAmount {0};
};

struct InboundMessage
{
  ConnectionId connectionId {0};
  std::variant<std::string, ClientConnected, ClientDisconnected, BackpressureUpdate> payload;
};

struct OutboundMessage
{
  ConnectionId connectionId {0};
  std::string payload;
};

using InboundQueue = moodycamel::ConcurrentQueue<InboundMessage>;
using OutboundQueue = moodycamel::ConcurrentQueue<OutboundMessage>;

/// Error codes used on the wire. The first block is the JSON-RPC 2.0 reserved range
/// (spec section 5.1); the second block is Sen application codes in the spec's
/// -32000..-32099 "Server error" range, used for semantic failures where the envelope and
/// params are well-formed but the server can't satisfy the request.
enum class JsonRpcErrorCode : int
{
  parseError = -32700,
  invalidRequest = -32600,
  methodNotFound = -32601,
  invalidParams = -32602,
  internalError = -32603,

  unknownInterest = -32001,
  objectNotInInterest = -32002,
  unknownMember = -32003,
  unknownType = -32004,
  notWritable = -32005,
};

/// JSON-RPC 2.0 error object. `message` is a stable prefix; `data` carries variable detail.
struct JsonRpcError
{
  JsonRpcErrorCode code {JsonRpcErrorCode::internalError};
  std::string message;
  std::optional<nlohmann::json> data;
};

[[nodiscard]] inline JsonRpcError makeError(JsonRpcErrorCode code, std::string message)
{
  return JsonRpcError {code, std::move(message), std::nullopt};
}

[[nodiscard]] inline JsonRpcError makeError(JsonRpcErrorCode code, std::string message, nlohmann::json data)
{
  return JsonRpcError {code, std::move(message), std::move(data)};
}

/// Domain exception thrown by `Server` method overrides to surface a typed JSON-RPC error. Other
/// `std::exception`s escaping a method body collapse to `internalError` with `what()` in `data`.
class JsonRpcException: public std::exception
{
public:
  JsonRpcException(JsonRpcErrorCode code, std::string message): error_(makeError(code, std::move(message))) {}

  JsonRpcException(JsonRpcErrorCode code, std::string message, nlohmann::json data)
    : error_(makeError(code, std::move(message), std::move(data)))
  {
  }

  [[nodiscard]] const JsonRpcError& error() const noexcept { return error_; }
  [[nodiscard]] const char* what() const noexcept override { return error_.message.c_str(); }

private:
  JsonRpcError error_;
};

}  // namespace sen::components::jsonrpc

SEN_STRONG_TYPE_HASHABLE(sen::components::jsonrpc::ConnectionId)

#endif  // SEN_COMPONENTS_JSONRPC_MESSAGES_H
