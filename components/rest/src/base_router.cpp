// === base_router.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "base_router.h"

// generated code
#include "stl/types.stl.h"

// sen
#include "sen/core/base/assert.h"

// std
#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace sen::components::rest
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

constexpr std::string_view pathParamRegex = ":[a-zA-Z0-9]+";
constexpr std::string_view pathSegmentRegex = "[a-zA-Z0-9._~$@,;=-]+";

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

std::optional<HttpMethod> fromString(std::string methodStr)
{
  static const std::unordered_map<std::string, HttpMethod> strToMethod = {{"GET", HttpMethod::httpGet},
                                                                          {"POST", HttpMethod::httpPost},
                                                                          {"PUT", HttpMethod::httpPut},
                                                                          {"DELETE", HttpMethod::httpDelete}};

  // in-place normalize method to upper case
  std::transform(methodStr.begin(), methodStr.end(), methodStr.begin(), [](auto c) { return std::toupper(c); });

  // lookup the supported method dictionary
  const auto method = strToMethod.find(methodStr);
  if (method != strToMethod.cend())
  {
    return method->second;
  }
  // method not supported
  return std::nullopt;
}

//--------------------------------------------------------------------------------------------------------------
// BaseRouter
//--------------------------------------------------------------------------------------------------------------

void BaseRouter::addRoute(HttpMethod method, const std::string& path, const RouteCallback callback)
{
  const std::regex pattern("^(/(" + std::string(pathParamRegex) + "|" + std::string(pathSegmentRegex) + "))+/?$");

  if (std::regex_match(path, pattern))
  {
    const std::regex paramMatcherRegex("/" + std::string(pathParamRegex));
    const std::string paramMatcherStr =
      std::regex_replace(path, paramMatcherRegex, "/(" + std::string(pathSegmentRegex) + ")");

    routes_[method].emplace_back(path, std::regex(paramMatcherStr), callback);
  }
  else
  {
    ::sen::throwRuntimeError("Invalid route path");
  }
}

void BaseRouter::releaseAll() { routes_.clear(); }

std::optional<MatchedRoute> BaseRouter::matchPath(HttpMethod method, const std::string& path) const noexcept
{
  std::smatch matches;
  const auto routes = routes_.find(method);

  if (routes == routes_.cend())
  {
    return std::nullopt;
  }

  for (const auto& route: routes->second)
  {
    if (std::regex_match(path, route.matcher) && std::regex_search(path, matches, route.matcher))
    {
      UrlParams params {matches.cbegin() + 1, matches.cend()};
      return MatchedRoute {params, route.callback};
    }
  }

  return std::nullopt;
}

}  // namespace sen::components::rest
