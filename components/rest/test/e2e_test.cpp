// === e2e_test.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "request.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/kernel/test_kernel.h"

// google test
#include <gtest/gtest.h>

// json
#include <jwt.h>
#include <nlohmann/json.hpp>

// std
#include <atomic>
#include <chrono>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using Json = nlohmann::json;

constexpr std::string_view configString = R"(
    load:
    - name: rest
      group: 3
      address: "127.0.0.1"
      port: 12345
  )";

class Server
{
  SEN_NOCOPY_NOMOVE(Server)

public:
  Server(): cancelFlag_(false)
  {
    th_ = std::thread(
      [this]()
      {
        auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
        while (!cancelFlag_)
        {
          kernel.step();
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  ~Server()
  {
    cancel();
    join();
  }

  void cancel() { cancelFlag_ = true; }
  void join()
  {
    if (th_.joinable())
    {
      th_.join();
    }
  }

private:
  std::atomic<bool> cancelFlag_;
  std::thread th_;
};

HttpResponse retryUntil(int statusCode, std::function<HttpResponse()> callback)
{
  HttpResponse ret {404};
  for (auto retry = 0; retry < 10; ++retry)
  {
    ret = callback();
    if (ret.statusCode == statusCode)
    {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return ret;
}

/// @test
/// End-to-end test for sessions retrieval
/// @requirements(SEN-1061)
TEST(Rest, e2e_sessions)
{
  Server server;

  auto ret = request(HttpMethod::httpGet, "127.0.0.1", "12345", "/api/sessions");
  ASSERT_EQ(ret.statusCode, 200);
  ASSERT_EQ(Json::parse(ret.body).dump(), R"(["local"])");
}

/// @test
/// End-to-end test for client authentication
/// @requirements(SEN-1061)
TEST(Rest, e2e_auth)
{
  Server server;

  auto ret = request(HttpMethod::httpPost, "127.0.0.1", "12345", "/api/auth", Json {{"id", "admin"}});
  ASSERT_EQ(ret.statusCode, 200);

  auto response = Json::parse(ret.body);
  auto jwt = sen::components::rest::decodeJWT(response["token"].get<std::string>());
  ASSERT_TRUE(jwt.valid);
  ASSERT_EQ(jwt.error, sen::components::rest::JWTError::noError);
}

/// @test
/// End-to-end test for type introspection endpoint
/// @requirements(SEN-1061)
TEST(Rest, e2e_type_introspection)
{
  Server server;

  auto authRet = request(HttpMethod::httpPost, "127.0.0.1", "12345", "/api/auth", Json {{"id", "admin"}});
  ASSERT_EQ(authRet.statusCode, 200);

  auto authResponse = Json::parse(authRet.body);
  auto token = authResponse["token"].get<std::string>();

  auto ret = request(HttpMethod::httpGet, "127.0.0.1", "12345", "/api/types/string", Json(), token);
  ASSERT_EQ(ret.statusCode, 200);

  auto typeInfo = Json::parse(ret.body);
  ASSERT_EQ(typeInfo["name"].get<std::string>(), "string");
}

/// End-to-end test for interests retrieval (empty list)
/// @requirements(SEN-1061)
TEST(Rest, e2e_get_interests)
{
  Server server;

  auto token = authenticate();
  ASSERT_TRUE(token.has_value());

  auto ret = request(HttpMethod::httpGet, "127.0.0.1", "12345", "/api/interests", Json(), token.value());

  ASSERT_EQ(ret.statusCode, 200);
  auto interests = Json::parse(ret.body);
  ASSERT_TRUE(interests.is_array());
  ASSERT_EQ(interests.size(), 0);
}

/// @test
/// End-to-end test for successful interest creation
/// @requirements(SEN-1061)
TEST(Rest, e2e_create_interest)
{
  Server server;

  auto token = authenticate();
  ASSERT_TRUE(token.has_value());

  std::vector<std::string> interestNames = {
    "abcd",
    "ab_cd",
    "ab__cd",
    "ab-cd",
    "ab--cd",
    "ab;;cd",
    "ab,,cd",
    "ab@@cd",
    "ab@@cd,,ef;;gh--ij__kl",
  };

  for (const auto& interestName: interestNames)
  {
    auto ret = request(HttpMethod::httpPost,
                       "127.0.0.1",
                       "12345",
                       "/api/interests",
                       Json {{"name", interestName}, {"query", "SELECT * FROM local.kernel"}},
                       token.value());

    ASSERT_EQ(ret.statusCode, 200);
    auto response = Json::parse(ret.body);
    ASSERT_TRUE(response.contains("name"));
    ASSERT_EQ(response["name"], interestName);
  }
}

/// @test
/// End-to-end test for invalid query on interest creation
/// @requirements(SEN-1061)
TEST(Rest, e2e_create_interest_invalid_query)
{
  Server server;

  auto token = authenticate();
  ASSERT_TRUE(token.has_value());

  auto ret = request(HttpMethod::httpPost,
                     "127.0.0.1",
                     "12345",
                     "/api/interests",
                     Json {{"name", "test_interest"}, {"query", "INVALID QUERY"}},
                     token.value());

  ASSERT_EQ(ret.statusCode, 400);
}

/// @test
/// End-to-end test for getting an existing interest
/// @requirements(SEN-1061)
TEST(Rest, e2e_get_interest_success)
{
  Server server;

  auto token = authenticate();
  ASSERT_TRUE(token.has_value());

  auto createRet = request(HttpMethod::httpPost,
                           "127.0.0.1",
                           "12345",
                           "/api/interests",
                           Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}},
                           token.value());
  ASSERT_EQ(createRet.statusCode, 200);

  auto ret = retryUntil(
    200,
    [token]()
    {
      return request(HttpMethod::httpGet, "127.0.0.1", "12345", "/api/interests/test_interest", Json(), token.value());
    });

  ASSERT_EQ(ret.statusCode, 200);

  auto response = Json::parse(ret.body);
  ASSERT_TRUE(response.contains("name"));
  ASSERT_EQ(response["name"].get<std::string>(), "test_interest");
}

/// @test
/// End-to-end test for getting an unknown interest
/// @requirements(SEN-1061)
TEST(Rest, e2e_get_interest_unknown_interest)
{
  Server server;

  auto token = authenticate();
  ASSERT_TRUE(token.has_value());

  auto ret = request(HttpMethod::httpGet, "127.0.0.1", "12345", "/api/interests/test_interest", Json(), token.value());
  ASSERT_EQ(ret.statusCode, 404);
}

/// @test
/// End-to-end test for removing an interest
/// @requirements(SEN-1061)
TEST(Rest, e2e_remove_interest)
{
  Server server;

  // Authenticate
  auto token = authenticate();
  ASSERT_TRUE(token.has_value());

  // Create an interest
  auto ret = request(HttpMethod::httpPost,
                     "127.0.0.1",
                     "12345",
                     "/api/interests",
                     Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}},
                     token.value());
  ASSERT_EQ(ret.statusCode, 200);

  // Check interest was created
  ret = retryUntil(
    200,
    [token]()
    {
      return request(HttpMethod::httpGet, "127.0.0.1", "12345", "/api/interests/test_interest", Json(), token.value());
    });

  ASSERT_EQ(ret.statusCode, 200);
  auto response = Json::parse(ret.body);
  ASSERT_TRUE(response.contains("name"));
  ASSERT_EQ(response["name"].get<std::string>(), "test_interest");

  // Delete the interest
  ret = request(HttpMethod::httpDelete, "127.0.0.1", "12345", "/api/interests/test_interest", Json(), token.value());
  ASSERT_EQ(ret.statusCode, 200);

  // Check there is no active interests
  ret = retryUntil(
    200,
    [token]() { return request(HttpMethod::httpGet, "127.0.0.1", "12345", "/api/interests", Json(), token.value()); });

  ASSERT_EQ(ret.statusCode, 200);
  auto interests = Json::parse(ret.body);
  ASSERT_TRUE(interests.is_array());
  ASSERT_EQ(interests.size(), 0);
}

/// @test
/// End-to-end test for getting objects in an interest
/// @requirements(SEN-1061)
TEST(Rest, e2e_get_objects)
{
  Server server;

  // Authenticate
  auto token = authenticate();
  ASSERT_TRUE(token.has_value());

  // Create interest
  auto createRet = request(HttpMethod::httpPost,
                           "127.0.0.1",
                           "12345",
                           "/api/interests",
                           Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}},
                           token.value());
  ASSERT_EQ(createRet.statusCode, 200);

  // Get objects
  auto ret =
    request(HttpMethod::httpGet, "127.0.0.1", "12345", "/api/interests/test_interest/objects", Json(), token.value());
  ASSERT_EQ(ret.statusCode, 200);

  auto response = Json::parse(ret.body);
  ASSERT_TRUE(response.is_array());
}

/// @test
/// End-to-end test for getting existing objects
/// @requirements(SEN-1061)
TEST(Rest, e2e_get_existing_objects)
{
  Server server;

  // Authenticate
  auto token = authenticate();
  ASSERT_TRUE(token.has_value());

  // Create interest
  auto createRet = request(HttpMethod::httpPost,
                           "127.0.0.1",
                           "12345",
                           "/api/interests",
                           Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}},
                           token.value());
  ASSERT_EQ(createRet.statusCode, 200);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Get objects
  auto ret = retryUntil(
    200,
    [token]()
    {
      return request(
        HttpMethod::httpGet, "127.0.0.1", "12345", "/api/interests/test_interest/objects", Json(), token.value());
    });
  ASSERT_EQ(ret.statusCode, 200);

  auto interests = Json::parse(ret.body);
  ASSERT_TRUE(interests.is_array());
  ASSERT_GT(interests.size(), 0);
}

/// @test
/// End-to-end test for getting existing object
/// @requirements(SEN-1061)
TEST(Rest, e2e_get_existing_object)
{
  Server server;

  // Authenticate
  auto token = authenticate();
  ASSERT_TRUE(token.has_value());

  // Create interest
  auto createRet = request(HttpMethod::httpPost,
                           "127.0.0.1",
                           "12345",
                           "/api/interests",
                           Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}},
                           token.value());
  ASSERT_EQ(createRet.statusCode, 200);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Get object
  auto ret = retryUntil(
    200,
    [token]()
    {
      return request(
        HttpMethod::httpGet, "127.0.0.1", "12345", "/api/interests/test_interest/objects/api", Json(), token.value());
    });

  ASSERT_EQ(ret.statusCode, 200);

  auto interests = Json::parse(ret.body);
  ASSERT_TRUE(interests.is_object());
  ASSERT_EQ(interests["localname"], "rest.local.kernel.api");
}

/// @test
/// End-to-end test for getting an non-existing object
/// @requirements(SEN-1061)
TEST(Rest, e2e_get_non_existing_object)
{
  Server server;

  // Authenticate
  auto token = authenticate();
  ASSERT_TRUE(token.has_value());

  // Create interest
  auto createRet = request(HttpMethod::httpPost,
                           "127.0.0.1",
                           "12345",
                           "/api/interests",
                           Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}},
                           token.value());
  ASSERT_EQ(createRet.statusCode, 200);

  // Try to get an non-existent object
  auto ret = request(HttpMethod::httpGet,
                     "127.0.0.1",
                     "12345",
                     "/api/interests/test_interest/objects/test_object",
                     Json(),
                     token.value());
  ASSERT_EQ(ret.statusCode, 404);
}

/// @test
/// End-to-end test for getting a method definition
/// @requirements(SEN-1061)
TEST(Rest, e2e_get_method_definition)
{
  Server server;

  // Authenticate
  auto token = authenticate();
  ASSERT_TRUE(token.has_value());

  // Create interest
  auto createRet = request(HttpMethod::httpPost,
                           "127.0.0.1",
                           "12345",
                           "/api/interests",
                           Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}},
                           token.value());
  ASSERT_EQ(createRet.statusCode, 200);

  // Get method definition
  HttpResponse ret = retryUntil(200,
                                [&token]()
                                {
                                  return request(HttpMethod::httpGet,
                                                 "127.0.0.1",
                                                 "12345",
                                                 "/api/interests/test_interest/objects/api/methods/shutdown",
                                                 Json(),
                                                 token.value());
                                });
  ASSERT_EQ(ret.statusCode, 200);

  auto res = Json::parse(ret.body);
  ASSERT_TRUE(res.is_object());
  ASSERT_EQ(res["name"], "shutdown");
}

/// @test
/// End-to-end test for getting property definition
/// @requirements(SEN-1061)
TEST(Rest, e2e_get_property_definition)
{
  Server server;

  // Authenticate
  auto token = authenticate();
  ASSERT_TRUE(token.has_value());

  // Create interest
  auto createRet = request(HttpMethod::httpPost,
                           "127.0.0.1",
                           "12345",
                           "/api/interests",
                           Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}},
                           token.value());
  ASSERT_EQ(createRet.statusCode, 200);

  // Get method definition
  auto ret = retryUntil(200,
                        [token]()
                        {
                          return request(HttpMethod::httpGet,
                                         "127.0.0.1",
                                         "12345",
                                         "/api/interests/test_interest/objects/api/properties/buildInfo",
                                         Json(),
                                         token.value());
                        });
  ASSERT_EQ(ret.statusCode, 200);

  auto res = Json::parse(ret.body);
  ASSERT_TRUE(res.is_object());
}

/// @test
/// End-to-end test for subscribing and unsubscribing to property updates
/// @requirements(SEN-1061)
TEST(Rest, e2e_property_subscription)
{
  Server server;

  // Authenticate
  auto token = authenticate();
  ASSERT_TRUE(token.has_value());

  // Create interest
  auto createRet = request(HttpMethod::httpPost,
                           "127.0.0.1",
                           "12345",
                           "/api/interests",
                           Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}},
                           token.value());
  ASSERT_EQ(createRet.statusCode, 200);

  auto ret = retryUntil(200,
                        [token]()
                        {
                          return request(HttpMethod::httpPost,
                                         "127.0.0.1",
                                         "12345",
                                         "/api/interests/test_interest/objects/api/properties/buildInfo/subscribe",
                                         Json(),
                                         token.value());
                        });
  ASSERT_TRUE(ret.statusCode == 200);

  ret = retryUntil(200,
                   [token]()
                   {
                     return request(HttpMethod::httpPost,
                                    "127.0.0.1",
                                    "12345",
                                    "/api/interests/test_interest/objects/api/properties/buildInfo/unsubscribe",
                                    Json(),
                                    token.value());
                   });
  ASSERT_TRUE(ret.statusCode == 200);
}

/// @test
/// End-to-end test for invoking a method
/// @requirements(SEN-1061)
TEST(Rest, e2e_invoke_method)
{
  Server server;

  // Authenticate
  auto token = authenticate();
  ASSERT_TRUE(token.has_value());

  // Create interest
  auto createRet = request(HttpMethod::httpPost,
                           "127.0.0.1",
                           "12345",
                           "/api/interests",
                           Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}},
                           token.value());
  ASSERT_EQ(createRet.statusCode, 200);

  // Invoke method
  auto ret = retryUntil(200,
                        [token]()
                        {
                          return request(HttpMethod::httpPost,
                                         "127.0.0.1",
                                         "12345",
                                         "/api/interests/test_interest/objects/api/methods/getUnits/invoke",
                                         Json::array(),
                                         token.value());
                        });
  ASSERT_EQ(ret.statusCode, 200);

  auto res = Json::parse(ret.body);
  ASSERT_TRUE(res.is_object());
  ASSERT_TRUE(res["status"] == "finished" || res["status"] == "pending");

  ret =
    retryUntil(200,
               [token, &res]()
               {
                 return request(HttpMethod::httpGet,
                                "127.0.0.1",
                                "12345",
                                "/api/interests/test_interest/objects/api/methods/getUnits/invoke/" + res["id"].dump(),
                                Json(),
                                token.value());
               });
  ASSERT_EQ(ret.statusCode, 200);

  res = Json::parse(ret.body);
  ASSERT_TRUE(res.is_object());
  ASSERT_TRUE(res["status"] == "finished" || res["status"] == "pending");
}

/// @test
/// End-to-end test for notification subscription
/// @requirements(SEN-1061)
TEST(Rest, e2e_notification_subscription)
{
  Server server;

  // Authenticate
  auto token = authenticate();
  ASSERT_TRUE(token.has_value());

  // Create interest
  auto ret = request(HttpMethod::httpPost,
                     "127.0.0.1",
                     "12345",
                     "/api/interests",
                     Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}},
                     token.value());
  ASSERT_EQ(ret.statusCode, 200);

  // Query notifications from a different thread
  std::thread t(
    [token]()
    {
      auto ret = request(HttpMethod::httpGet, "127.0.0.1", "12345", "/api/sse", Json(), token.value(), true);
      ASSERT_EQ(ret.statusCode, 200);
    });

  // Invoke a method
  ret = retryUntil(200,
                   [token]()
                   {
                     return request(HttpMethod::httpPost,
                                    "127.0.0.1",
                                    "12345",
                                    "/api/interests/test_interest/objects/api/methods/getUnits/invoke",
                                    Json::array(),
                                    token.value());
                   });
  ASSERT_EQ(ret.statusCode, 200);

  t.join();
}
