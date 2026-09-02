// === util.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_JSONRPC_UTIL_H
#define SEN_COMPONENTS_JSONRPC_UTIL_H

// local
#include "dispatcher.h"
#include "messages.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/type_specs_utils.h"

// nlohmann
#include "nlohmann/json.hpp"

// std
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

// Forward declarations
namespace spdlog
{
// NOLINTNEXTLINE(readability-identifier-naming): spdlog's own class name
class logger;
}  // namespace spdlog

namespace sen::components::jsonrpc
{

// Forward declarations
class Server;

/// Component-wide logger handle (`"jsonrpc"`).
[[nodiscard]] std::shared_ptr<spdlog::logger> getLogger();

/// Returns the named object from an interest's match set in O(1), or null if absent.
[[nodiscard]] std::shared_ptr<sen::Object> findObject(const InterestEntry& entry, std::string_view name);

[[nodiscard]] InterestEntry& requireInterest(Server& server,
                                             std::string_view methodName,
                                             const std::string& interestName);

[[nodiscard]] std::shared_ptr<sen::Object> requireObjectInInterest(const InterestEntry& entry,
                                                                   std::string_view methodName,
                                                                   const std::string& objectName);

[[nodiscard]] const sen::Property& requireProperty(
  const sen::Object& object,
  std::string_view methodName,
  const std::string& propertyName,
  sen::ClassType::SearchMode mode = sen::ClassType::SearchMode::includeParents);

[[nodiscard]] const sen::Event& requireEvent(
  const sen::Object& object,
  std::string_view methodName,
  const std::string& eventName,
  sen::ClassType::SearchMode mode = sen::ClassType::SearchMode::includeParents);

[[nodiscard]] nlohmann::json varToJson(const sen::Var& var);

/// Like `varToJson`, but first adapts `var` to `type` with `useStrings=true` so any embedded
/// variants are emitted as `{type: "ShortName", value: ...}` rather than the binary-protocol
/// numeric-key form.
[[nodiscard]] nlohmann::json varToWireJson(sen::Var var, const sen::Type& type);

/// Encode a positional VarList for wire delivery: each `args[i]` is adapted against
/// `metaArgs[i].type` and emitted as a JSON array entry. Used for event arg payloads.
[[nodiscard]] nlohmann::json argListToWireJson(sen::VarList args, sen::Span<const sen::Arg> metaArgs);

/// Wire-shaped `CustomTypeSpec` for `customType`.
[[nodiscard]] ::sen::kernel::CustomTypeSpec makeWireCustomTypeSpec(const sen::CustomType& customType);

namespace impl
{
[[nodiscard]] bool extractStringFields(const RequestContext& ctx,
                                       const nlohmann::json& params,
                                       std::string_view methodName,
                                       sen::Span<const std::string_view> names,
                                       sen::Span<std::string> out);
}  // namespace impl

/// Validates that every named field is present in `params` as a string and returns the values.
template <typename... Names>
[[nodiscard]] std::optional<std::array<std::string, sizeof...(Names)>> requireStringFields(const RequestContext& ctx,
                                                                                           const nlohmann::json& params,
                                                                                           std::string_view methodName,
                                                                                           Names... fieldNames)
{
  static_assert(sizeof...(Names) > 0, "requireStringFields needs at least one field name");
  static_assert((std::is_convertible_v<Names, std::string_view> && ...), "names shall convert to std::string_view");

  const std::array<std::string_view, sizeof...(Names)> names {std::string_view {fieldNames}...};
  std::array<std::string, sizeof...(Names)> out {};
  if (!impl::extractStringFields(ctx, params, methodName, names, out))
  {
    return std::nullopt;
  }
  return out;
}

[[nodiscard]] sen::Result<sen::Duration, JsonRpcError> parseOptionalMaxRateHz(const nlohmann::json& params,
                                                                              std::string_view methodName);

[[nodiscard]] sen::Result<void, std::string> adaptInvokeArgs(const sen::Method& method, sen::VarList& args);

[[nodiscard]] sen::Result<sen::VarList, JsonRpcError> parseInvokeArgs(const nlohmann::json& argsJson,
                                                                      const sen::Method& method,
                                                                      std::string_view methodName);

}  // namespace sen::components::jsonrpc

#endif  // SEN_COMPONENTS_JSONRPC_UTIL_H
