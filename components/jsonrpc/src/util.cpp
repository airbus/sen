// === util.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "util.h"

// local
#include "server.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/method.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/component_api.h"

// generated
#include "stl/sen/kernel/type_specs.stl.h"

// std
#include <cstddef>
#include <exception>
#include <string>
#include <variant>

// spdlog
#include <spdlog/logger.h>

namespace sen::components::jsonrpc
{

std::shared_ptr<spdlog::logger> getLogger() { return kernel::KernelApi::getOrCreateLogger("jsonrpc"); }

std::shared_ptr<sen::Object> findObject(const InterestEntry& entry, std::string_view name)
{
  const auto it = entry.objectsByName.find(std::string {name});
  if (it == entry.objectsByName.end())
  {
    return {};
  }
  return it->second.lock();
}

namespace
{

// Bounds walkVarToJson / walkJsonToVar recursion so a hostile wire payload cannot blow the
// C++ stack.
constexpr std::size_t maxVarDepth = 64;

// Mirrors libs/core/src/meta/var.cpp's `varToJson` / `jsonToVar` to skip the
// `sen::toJson(string) -> parse -> dump` round-trip on the hot path.

[[nodiscard]] nlohmann::json walkVarToJson(const sen::Var& var, std::size_t depth = 0)
{
  using Json = nlohmann::json;
  if (depth > maxVarDepth)
  {
    throw std::runtime_error("walkVarToJson: nesting exceeds maxVarDepth");
  }
  const std::size_t childDepth = depth + 1U;
  // i64 / u64 / Duration ship as decimal strings (precision-exact); narrower scalars stay as
  // JSON numbers. Catch-all is avoided so the per-arm choice is explicit.
  return std::visit(
    sen::Overloaded {[](std::monostate) -> Json { return {}; },
                     [](bool val) -> Json { return val; },
                     [](int8_t val) -> Json { return val; },
                     [](int16_t val) -> Json { return val; },
                     [](int32_t val) -> Json { return val; },
                     [](uint8_t val) -> Json { return val; },
                     [](uint16_t val) -> Json { return val; },
                     [](uint32_t val) -> Json { return val; },
                     [](float val) -> Json { return val; },
                     [](double val) -> Json { return val; },
                     [](const std::string& val) -> Json { return val; },
                     [](int64_t val) -> Json { return std::to_string(val); },
                     [](uint64_t val) -> Json { return std::to_string(val); },
                     [](const sen::Duration& val) -> Json { return std::to_string(val.getNanoseconds()); },
                     [](const sen::TimeStamp& val) -> Json { return val.toUtcStringNs(); },
                     [childDepth](const sen::VarList& val) -> Json
                     {
                       Json result(Json::value_t::array);
                       for (const auto& elem: val)
                       {
                         result.emplace_back(walkVarToJson(elem, childDepth));
                       }
                       return result;
                     },
                     [childDepth](const sen::VarMap& val) -> Json
                     {
                       Json result(Json::value_t::object);
                       for (const auto& [key, elemVal]: val)
                       {
                         result[key] = walkVarToJson(elemVal, childDepth);
                       }
                       return result;
                     },
                     [childDepth](const sen::KeyedVar& tuple) -> Json
                     {
                       Json result(Json::value_t::object);
                       result["type"] = std::get<0U>(tuple);
                       result["value"] = walkVarToJson(*std::get<1U>(tuple), childDepth);
                       return result;
                     }},
    static_cast<const sen::Var::ValueType&>(var));
}

[[nodiscard]] sen::Var walkJsonToVar(const nlohmann::json& val, std::size_t depth = 0)
{
  using Json = nlohmann::json;
  if (depth > maxVarDepth)
  {
    throw std::runtime_error("walkJsonToVar: nesting exceeds maxVarDepth");
  }
  const std::size_t childDepth = depth + 1U;
  switch (val.type())
  {
    case Json::value_t::null:
      return std::monostate {};
    case Json::value_t::object:
    {
      sen::VarMap result;
      for (const auto& el: val.items())
      {
        result.try_emplace(el.key(), walkJsonToVar(el.value(), childDepth));
      }
      return result;
    }
    case Json::value_t::array:
    {
      sen::VarList result;
      result.reserve(val.size());
      for (const auto& elem: val)
      {
        result.push_back(walkJsonToVar(elem, childDepth));
      }
      return result;
    }
    case Json::value_t::string:
      return val.get<std::string>();
    case Json::value_t::boolean:
      return val.get<bool>();
    case Json::value_t::number_integer:
      return val.get<int64_t>();
    case Json::value_t::number_unsigned:
      return val.get<uint64_t>();
    case Json::value_t::number_float:
      return val.get<float64_t>();
    default:
      throw std::runtime_error("Json type not supported");
  }
}

}  // namespace

nlohmann::json varToJson(const sen::Var& var) { return walkVarToJson(var); }

nlohmann::json varToWireJson(sen::Var var, const sen::Type& type)
{
  // adaptVariant with useStrings=true rewrites every embedded KeyedVar to a VarMap with a string
  // "type" tag, matching the wire-shape contract expected by external consumers.
  std::ignore = sen::impl::adaptVariant(type, var, std::nullopt, /*useStrings=*/true);
  return walkVarToJson(var);
}

::sen::kernel::CustomTypeSpec makeWireCustomTypeSpec(const sen::CustomType& customType)
{
  auto spec = ::sen::kernel::makeCustomTypeSpec(&customType);

  // `sen.TimeStamp` is a `QuantityType` in the kernel but `varToJson` serializes it as a UTC
  // string (see `libs/core/src/meta/var.cpp`). Advertising it as a Quantity makes typed clients
  // reject the string payload. Override to an alias of the built-in `TimeStamp` so the wire
  // spec agrees with the wire value.
  if (spec.qualifiedName == "sen.TimeStamp" && std::holds_alternative<::sen::kernel::QuantityTypeSpec>(spec.data))
  {
    spec.data = ::sen::kernel::AliasTypeSpec {"TimeStamp"};
  }

  return spec;
}

nlohmann::json argListToWireJson(sen::VarList args, sen::Span<const sen::Arg> metaArgs)
{
  auto array = nlohmann::json::array();
  const std::size_t count = std::min(args.size(), metaArgs.size());
  for (std::size_t i = 0; i < count; ++i)
  {
    array.push_back(varToWireJson(std::move(args[i]), *metaArgs[i].type));
  }
  // Tail of args beyond the meta signature (should not happen if the kernel obeys the contract,
  // but we don't drop data silently).
  for (std::size_t i = count; i < args.size(); ++i)
  {
    array.push_back(varToJson(args[i]));
  }
  return array;
}

namespace impl
{
bool extractStringFields(const RequestContext& ctx,
                         const nlohmann::json& params,
                         std::string_view methodName,
                         sen::Span<const std::string_view> names,
                         sen::Span<std::string> out)
{
  SEN_ASSERT(names.size() == out.size());
  for (std::size_t i = 0; i < names.size(); ++i)
  {
    auto it = params.find(names[i]);
    if (it == params.end() || !it->is_string())
    {
      std::string message {methodName};
      message += ": requires {";
      bool first = true;
      for (const auto& name: names)
      {
        if (!first)
        {
          message += ", ";
        }
        first = false;
        message.append(name);
        message += ": string";
      }
      message += "}";
      ctx.respond(sen::Err(makeError(JsonRpcErrorCode::invalidParams, std::move(message))));
      return false;
    }
    out[i] = it->get<std::string>();
  }
  return true;
}
}  // namespace impl

InterestEntry& requireInterest(Server& server, std::string_view methodName, const std::string& interestName)
{
  auto& interests = server.interests();
  if (const auto it = interests.find(interestName); it != interests.end())
  {
    return it->second;
  }
  std::string message {methodName};
  message += ": unknown interest: ";
  message += interestName;
  throw JsonRpcException(JsonRpcErrorCode::unknownInterest, std::move(message));
}

std::shared_ptr<sen::Object> requireObjectInInterest(const InterestEntry& entry,
                                                     std::string_view methodName,
                                                     const std::string& objectName)
{
  if (auto object = findObject(entry, objectName))
  {
    return object;
  }
  std::string message {methodName};
  message += ": object not in interest: ";
  message += objectName;
  throw JsonRpcException(JsonRpcErrorCode::objectNotInInterest, std::move(message));
}

const sen::Property& requireProperty(const sen::Object& object,
                                     std::string_view methodName,
                                     const std::string& propertyName,
                                     sen::ClassType::SearchMode mode)
{
  if (const auto* property = object.getClass()->searchPropertyByName(propertyName, mode))
  {
    return *property;
  }
  std::string message {methodName};
  message += ": unknown property: ";
  message += propertyName;
  throw JsonRpcException(JsonRpcErrorCode::unknownMember, std::move(message));
}

const sen::Event& requireEvent(const sen::Object& object,
                               std::string_view methodName,
                               const std::string& eventName,
                               sen::ClassType::SearchMode mode)
{
  if (const auto* event = object.getClass()->searchEventByName(eventName, mode))
  {
    return *event;
  }
  std::string message {methodName};
  message += ": unknown event: ";
  message += eventName;
  throw JsonRpcException(JsonRpcErrorCode::unknownMember, std::move(message));
}

sen::Result<sen::Duration, JsonRpcError> parseOptionalMaxRateHz(const nlohmann::json& params,
                                                                std::string_view methodName)
{
  auto rateIt = params.find("maxRateHz");
  if (rateIt == params.end())
  {
    return sen::Ok(sen::Duration {});
  }

  if (!rateIt->is_number() || rateIt->get<double>() <= 0.0)
  {
    std::string message {methodName};
    message += ": 'maxRateHz' must be a number > 0";
    return sen::Err(makeError(JsonRpcErrorCode::invalidParams, std::move(message)));
  }

  return sen::Ok(sen::Duration::fromHertz(rateIt->get<double>()));
}

sen::Result<void, std::string> adaptInvokeArgs(const sen::Method& method, sen::VarList& args)
{
  const auto& metaArgs = method.getArgs();
  if (args.size() != metaArgs.size())
  {
    return sen::Err("expected " + std::to_string(metaArgs.size()) + " args, got " + std::to_string(args.size()));
  }

  for (std::size_t i = 0; i < metaArgs.size(); ++i)
  {
    if (auto r = sen::impl::adaptVariant(*metaArgs[i].type, args[i], std::nullopt, true); r.isError())
    {
      return sen::Err("arg " + std::to_string(i) + " (" + metaArgs[i].name + "): " + r.getError());
    }
  }
  return sen::Ok();
}

sen::Result<sen::VarList, JsonRpcError> parseInvokeArgs(const nlohmann::json& argsJson,
                                                        const sen::Method& method,
                                                        std::string_view methodName)
{
  // Distinct prefixes for parse-vs-adapt failures so the wire error names the failure kind.
  const auto parsePrefix = std::string {methodName} + ": cannot decode args from JSON";
  const auto adaptPrefix = std::string {methodName} + ": invalid args";
  sen::VarList args;
  if (!argsJson.is_null())
  {
    try
    {
      args = walkJsonToVar(argsJson).get<sen::VarList>();
    }
    catch (const std::exception& e)
    {
      return sen::Err(makeError(JsonRpcErrorCode::invalidParams, parsePrefix, std::string {e.what()}));
    }
  }

  try
  {
    if (auto adapt = adaptInvokeArgs(method, args); adapt.isError())
    {
      return sen::Err(makeError(JsonRpcErrorCode::invalidParams, adaptPrefix, std::move(adapt).getError()));
    }
  }
  catch (const std::exception& e)
  {
    return sen::Err(makeError(JsonRpcErrorCode::invalidParams, adaptPrefix, std::string {e.what()}));
  }
  return sen::Ok(std::move(args));
}

}  // namespace sen::components::jsonrpc
