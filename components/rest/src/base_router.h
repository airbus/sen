// === base_router.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_BASE_ROUTER_H
#define SEN_COMPONENTS_REST_SRC_BASE_ROUTER_H

// component
#include "http_response.h"
#include "http_session.h"
#include "json_response.h"

// generated code
#include "stl/types.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/result.h"

// asio
#include <asio/ip/tcp.hpp>

// std
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace sen::components::rest
{

using UrlParams = std::vector<std::string>;
using QueryParams = std::map<std::string, std::string>;
using RouteCallback = std::function<HttpResponse(HttpSession&, const UrlParams&, const QueryParams&)>;
using StreamRouteCallback = std::function<
  HttpResponse(std::shared_ptr<HttpSession>, const UrlParams&, const QueryParams&, asio::ip::tcp::socket)>;

// Template helpers bind member functions instead lambdas
template <typename T, typename MemberFn>
RouteCallback bindRouteCallback(T* obj, MemberFn memberFn)
{
  return [obj, memberFn](HttpSession& session, const UrlParams& urlParams, const QueryParams& queryParams)
  { return (obj->*memberFn)(session, urlParams, queryParams); };
}

std::optional<HttpMethod> fromString(std::string methodStr);

struct Route
{
  // NOLINTNEXTLINE
  Route(std::string _path, std::regex _matcher, RouteCallback _callback)
    : path(std::move(_path)), matcher(std::move(_matcher)), callback(std::move(_callback))
  {
    // Left blank intentionally
  }
  // NOLINTNEXTLINE
  Route(std::string _path, std::regex _matcher, StreamRouteCallback _callback)
    : path(std::move(_path)), matcher(std::move(_matcher)), callback(std::move(_callback))
  {
    // Left blank intentionally
  }

  std::string path;                                           // NOLINT(misc-non-private-member-variables-in-classes)
  std::regex matcher;                                         // NOLINT(misc-non-private-member-variables-in-classes)
  std::variant<RouteCallback, StreamRouteCallback> callback;  // NOLINT(misc-non-private-member-variables-in-classes)
};

struct MatchedRoute
{
  UrlParams urlParams;
  QueryParams queryParams;
  std::variant<RouteCallback, StreamRouteCallback> callback;
};

struct QueryParamsError
{
  std::string error;
};

/// BaseRouter base class for routing HTTP requests to appropriate handlers.
/// Derived classes can extend this to associate HTTP methods and paths
/// with specific request handlers.
class BaseRouter
{
public:
  BaseRouter() = default;
  SEN_NOCOPY_NOMOVE(BaseRouter)
  virtual ~BaseRouter() = default;

  /// Returns a matching route callback for the given path and HTTP method.
  [[nodiscard]] Result<std::optional<MatchedRoute>, QueryParamsError> matchPath(HttpMethod method,
                                                                                const std::string& path) const noexcept;

  /// Clean up
  virtual void releaseAll();

protected:
  /// Adds a new route callback for the given endpoint path.
  void addRoute(HttpMethod method, const std::string& path, const RouteCallback callback);

  void addStreamRoute(HttpMethod method, const std::string& path, const StreamRouteCallback callback);

private:
  std::pair<std::string, std::regex> parseAndValidateRoute(const std::string& path);
  Result<QueryParams, QueryParamsError> getQueryParams(const std::string& queryParamsString) const noexcept;

  std::unordered_map<HttpMethod, std::vector<Route>> routes_;
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_BASE_ROUTER_H
