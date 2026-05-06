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
#include "sen/core/base/result.h"
#include "sen/core/io/util.h"

// std
#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

namespace sen::components::rest
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

constexpr std::string_view pathUrlParamRegex = ":[a-zA-Z0-9]+";
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

std::pair<std::string, std::regex> BaseRouter::parseAndValidateRoute(const std::string& path)
{
  const std::regex pattern("^(/(" + std::string(pathUrlParamRegex) + "|" + std::string(pathSegmentRegex) + "))+/?$");
  if (!std::regex_match(path, pattern))
  {
    ::sen::throwRuntimeError("Invalid route path");
  }

  const std::regex paramMatcherRegex("/" + std::string(pathUrlParamRegex));
  std::string paramMatcherStr = std::regex_replace(path, paramMatcherRegex, "/(" + std::string(pathSegmentRegex) + ")");

  return {paramMatcherStr, std::regex(paramMatcherStr)};
}

void BaseRouter::addRoute(HttpMethod method, const std::string& path, const RouteCallback callback)
{
  auto [matcherStr, matcherRegex] = parseAndValidateRoute(path);
  routes_[method].emplace_back(path, std::move(matcherRegex), callback);
}

void BaseRouter::addStreamRoute(HttpMethod method, const std::string& path, const StreamRouteCallback callback)
{
  auto [matcherStr, matcherRegex] = parseAndValidateRoute(path);
  routes_[method].emplace_back(path, std::move(matcherRegex), callback);
}

void BaseRouter::releaseAll() { routes_.clear(); }

Result<QueryParams, QueryParamsError> BaseRouter::getQueryParams(const std::string& queryParamsString) const noexcept
{
  QueryParams queryParams;

  if (queryParamsString.empty())
  {
    return Ok(queryParams);
  }

  for (const auto& queryParam: impl::split(queryParamsString, '&'))
  {
    const auto splitEqual = impl::split(queryParam, '=');

    if (splitEqual.size() == 2 && !splitEqual[0].empty() && !splitEqual[1].empty())
    {
      queryParams[splitEqual[0]] = splitEqual[1];
    }
    else
    {
      return Err(QueryParamsError {std::string("Invalid query params")});
    }
  }

  return Ok(queryParams);
}

Result<std::optional<MatchedRoute>, QueryParamsError> BaseRouter::matchPath(HttpMethod method,
                                                                            const std::string& path) const noexcept
{
  const auto routes = routes_.find(method);
  if (routes == routes_.cend())
  {
    return Ok(std::optional<MatchedRoute>(std::nullopt));
  }

  const auto pathParamsSplit = impl::split(path, '?');
  QueryParams queryParams;

  if (pathParamsSplit.size() == 2)
  {
    const auto queryParamsResult = getQueryParams(pathParamsSplit[1]);
    if (queryParamsResult.isError())
    {
      return Err(QueryParamsError {std::string("Invalid query params")});
    }

    queryParams = std::move(queryParamsResult).getValue();
  }

  std::smatch matches;
  for (const auto& route: routes->second)
  {
    if (std::regex_match(pathParamsSplit[0], route.matcher) &&
        std::regex_search(pathParamsSplit[0], matches, route.matcher))
    {
      const UrlParams urlParams {matches.cbegin() + 1, matches.cend()};

      if (std::holds_alternative<RouteCallback>(route.callback))
      {
        return Ok(std::make_optional(MatchedRoute {urlParams, queryParams, std::get<RouteCallback>(route.callback)}));
      }
      if (std::holds_alternative<StreamRouteCallback>(route.callback))
      {
        return Ok(
          std::make_optional(MatchedRoute {urlParams, queryParams, std::get<StreamRouteCallback>(route.callback)}));
      }
    }
  }

  return Ok(std::optional<MatchedRoute>(std::nullopt));
}

}  // namespace sen::components::rest
