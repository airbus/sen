// === router_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "base_router.h"
#include "http_response.h"
#include "http_session.h"

// sen
#include "sen/core/base/compiler_macros.h"

// generated code
#include "stl/types.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <exception>
#include <string>
#include <vector>

using sen::components::rest::HttpMethod;
using sen::components::rest::HttpResponse;
using sen::components::rest::HttpSession;
using sen::components::rest::UrlParams;

class ExampleRouter: public sen::components::rest::BaseRouter
{
public:
  ExampleRouter() = default;
  SEN_NOCOPY_NOMOVE(ExampleRouter)
  ~ExampleRouter() override = default;

  void onGet(const std::string& path, sen::components::rest::RouteCallback callback)
  {
    addRoute(HttpMethod::httpGet, path, callback);
  }
  void onPost(const std::string& path, sen::components::rest::RouteCallback callback)
  {
    addRoute(HttpMethod::httpPost, path, callback);
  }
  void onPut(const std::string& path, sen::components::rest::RouteCallback callback)
  {
    addRoute(HttpMethod::httpPut, path, callback);
  }
  void onDelete(const std::string& path, sen::components::rest::RouteCallback callback)
  {
    addRoute(HttpMethod::httpDelete, path, callback);
  }
};

/// @test
/// Check REST API valid syntax routes
/// @requirements(SEN-1061)
TEST(Rest, simple_routes)
{
  std::vector<std::string> routes {
    "/api",
    "/api/test",
    "/api/test/:id",
    "/api/test/:id/test2/:id2",
    "/api/test_test/:id",
  };

  ExampleRouter router;
  for (const auto& route: routes)
  {
    router.onGet(route, [](const HttpSession&, const UrlParams&) { return HttpResponse {}; });
  }

  std::vector<std::string> paths {
    "/api", "/api/test", "/api/test/123", "/api/test/123/test2/456", "/api/test_test/123"};
  for (const auto& path: paths)
  {
    ASSERT_TRUE(router.matchPath(HttpMethod::httpGet, path).has_value());
  }
}

/// @test
/// Check REST API invalid syntax routes
/// @requirements(SEN-1061)
TEST(Rest, invalid_routes)
{
  std::vector<std::string> routes {
    "api",
    "/api#",
    "/api/#test",
    "/api/:#id",
  };

  ExampleRouter router;
  for (const auto& route: routes)
  {
    try
    {
      router.onGet(route, [](const HttpSession&, const UrlParams&) { return HttpResponse {}; });
    }
    catch (const std::exception& e)
    {
      ASSERT_TRUE(std::string(e.what()).find("Invalid route path") != std::string::npos);
    }
  }
}

/// @test
/// Check REST API failing routes
/// @requirements(SEN-1061)
TEST(Rest, failing_routes)
{
  std::vector<std::string> routes {
    "/api", "/api/test", "/api/test/:id", "/api/test/:id/test2/:id2", "/api/test_test/:id"};

  ExampleRouter router;
  for (const auto& route: routes)
  {
    try
    {
      router.onGet(route, [](const HttpSession&, const UrlParams&) { return HttpResponse {}; });
    }
    catch (const std::exception& e)
    {
      ASSERT_TRUE(std::string(e.what()).find("Invalid route path") != std::string::npos);
    }
  }

  std::vector<std::string> paths {"/test", "/api/none", "/api/none/123", "/api/none/123/none2/456"};
  for (const auto& path: paths)
  {
    ASSERT_FALSE(router.matchPath(HttpMethod::httpGet, path).has_value());
  }
}

/// @test
/// Check REST API route with parameters
/// @requirements(SEN-1061)
TEST(Rest, router_params)
{
  std::vector<std::string> paths {
    "/test",
    "/test/:id",
    "/test/:id/test2/:subid",
    "/test_test3/:id/test_test2/:subid",
  };

  ExampleRouter router;
  for (const auto& path: paths)
  {
    router.onGet(path, [](const HttpSession&, const UrlParams&) { return HttpResponse {}; });
  }

  auto match1 = router.matchPath(HttpMethod::httpGet, "/test");
  ASSERT_TRUE(match1.value().params.empty());

  auto match2 = router.matchPath(HttpMethod::httpGet, "/test/1234");
  ASSERT_EQ(match2.value().params.size(), 1);
  ASSERT_EQ(match2.value().params[0], "1234");

  auto match3 = router.matchPath(HttpMethod::httpGet, "/test/1234/test2/5678");
  ASSERT_EQ(match3.value().params.size(), 2);
  ASSERT_EQ(match3.value().params[0], "1234");
  ASSERT_EQ(match3.value().params[1], "5678");
}

/// @test
/// Check REST API route with special chars in path segments
/// @requirements(SEN-1061)
TEST(Rest, router_params_special_path_segments)
{
  std::vector<std::string> paths {
    "/abcd",
    "/ab_cd",
    "/ab__cd",
    "/ab-cd",
    "/ab--cd",
    "/ab~~cd",
    "/ab==cd",
    "/ab;;cd",
    "/ab,,cd",
    "/ab@@cd",
  };

  ExampleRouter router;
  for (const auto& path: paths)
  {
    router.onGet(path, [](const HttpSession&, const UrlParams&) { return HttpResponse {}; });
  }

  for (const auto& path: paths)
  {
    auto match = router.matchPath(HttpMethod::httpGet, path);
    ASSERT_TRUE(match.value().params.empty());
  }
}
