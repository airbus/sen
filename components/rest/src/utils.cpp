// === utils.cpp =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "utils.h"

// component
#include "http_response.h"
#include "json_response.h"
#include "locators.h"

// generated code
#include "stl/types.stl.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/var.h"

// spdlog
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

// json
#include "nlohmann/json.hpp"

// std
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

using Json = nlohmann::json;

namespace sen::components::rest
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

constexpr std::string_view sseObjectRemoved = "event: object_removed\n";
constexpr std::string_view sseObjectAdded = "event: object_added\n";
constexpr std::string_view sseEvent = "event: event\n";
constexpr std::string_view sseMethodInvoke = "event: invoke\n";
constexpr std::string_view ssePropertyUpdate = "event: property\n";

//--------------------------------------------------------------------------------------------------------------
// Functions
//--------------------------------------------------------------------------------------------------------------

std::shared_ptr<spdlog::logger> getLogger()
{
  constexpr auto* loggerName = "rest";
  static auto logger = spdlog::get(loggerName);
  if (!logger)
  {
    logger = spdlog::stdout_color_mt(loggerName);
  }

  return logger;
}

[[nodiscard]] JsonResponse getErrorLocatorParams(const LocatorError& error)
{
  switch (error.errorType)
  {
    case LocatorErrorType::emptySessionName:
      return JsonResponse {httpBadRequestError, Error {"Session name cannot be empty"}};

    case LocatorErrorType::emptyBusName:
      return JsonResponse {httpBadRequestError, Error {"Bus name cannot be empty"}};

    case LocatorErrorType::emptyObjectName:
      return JsonResponse {httpBadRequestError, Error {"Object name cannot be empty"}};

    case LocatorErrorType::emptyPropertyName:
      return JsonResponse {httpBadRequestError, Error {"Property name cannot be empty"}};

    case LocatorErrorType::emptyMethodName:
      return JsonResponse {httpBadRequestError, Error {"Method name cannot be empty"}};

    default:
      return JsonResponse {httpBadRequestError, Error {"Invalid params"}};
  }
}

[[nodiscard]] const JsonResponse& getErrorInvalidParams()
{
  static const JsonResponse errorInvalidParams(httpBadRequestError, Error {"Invalid params"});
  return errorInvalidParams;
}

[[nodiscard]] const JsonResponse& getErrorNotFound()
{
  static const JsonResponse errorNotFound(httpNotFoundError, Error {"not found"});
  return errorNotFound;
}

[[nodiscard]] std::string urlSanitizeLocalName(std::string name)
{
  size_t idx = name.find_last_of('.');
  if (idx != std::string::npos)
  {
    name = name.substr(idx + 1);
  }

  return name;
}

ArgsError checkValidExpectedArgs(const Span<const Arg> expectedArgs, const VarList& args)
{
  if (expectedArgs.size() != args.size())
  {
    return "Expected " + std::to_string(expectedArgs.size()) + " arguments but got " + std::to_string(args.size());
  }

  return std::nullopt;
}

std::string toSSE(const Notification& notification)
{
  std::string sse;
  switch (notification.type)
  {
    case NotificationType::evt:
      sse.append(sseEvent);
      break;

    case NotificationType::invoke:
      sse.append(sseMethodInvoke);
      break;

    case NotificationType::property:
      sse.append(ssePropertyUpdate);
      break;

    case NotificationType::objectAdded:
      sse.append(sseObjectAdded);
      break;

    case NotificationType::objectRemoved:
      sse.append(sseObjectRemoved);
      break;

    default:
      SEN_ASSERT(false);
  }

  auto data = Json {
    {"time", notification.time.toUtcString()},
    {"interest", notification.interest},
    {"data", Json::parse(notification.data)},
  };
  sse.append("data: ");
  sse.append(data.dump());
  sse.append("\n\n");

  return sse;
}

[[nodiscard]] std::string tolower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return std::tolower(c); });
  return value;
}

}  // namespace sen::components::rest
