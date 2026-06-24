// === e2e_test.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "rest_e2e_fixture.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/base/version.h"
#include "sen/kernel/test_kernel.h"

// google test
#include <gtest/gtest.h>

// json
#include <jwt.h>
#include <nlohmann/json.hpp>

// std
#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <mutex>
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

        {
          std::lock_guard lock(mutex_);

          kernel.step();

          serverStarted_ = true;
          cv_.notify_all();
        }

        while (!cancelFlag_)
        {
          kernel.step();
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      });

    std::unique_lock lock(mutex_);
    cv_.wait_for(lock, std::chrono::milliseconds(500), [this] { return serverStarted_; });
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
  std::thread th_;
  bool serverStarted_ {false};

  std::atomic<bool> cancelFlag_;
  std::mutex mutex_;
  std::condition_variable cv_;
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
/// End-to-end test for the version endpoint
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, version)
{
  Server server;

  auto ret = request(HttpMethod::httpGet, "/api/version");
  ASSERT_EQ(ret.statusCode, 200);

  auto response = Json::parse(ret.body);
  ASSERT_TRUE(response.contains("version"));
  ASSERT_EQ(response["version"].get<std::string>(), SEN_VERSION_STRING);
}

/// @test
/// End-to-end test for client authentication
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, auth)
{
  Server server;

  auto ret = request(HttpMethod::httpPost, "/api/auth", Json {{"id", "admin"}});
  ASSERT_EQ(ret.statusCode, 200);

  auto response = Json::parse(ret.body);
  auto jwt = sen::components::rest::decodeJWT(response["token"].get<std::string>());
  ASSERT_TRUE(jwt.valid);
  ASSERT_EQ(jwt.error, sen::components::rest::JWTError::noError);
}

/// @test
/// End-to-end test for sessions retrieval
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, sessions)
{
  Server server;

  authenticate();

  auto ret = request(HttpMethod::httpGet, "/api/sessions", Json());
  ASSERT_EQ(ret.statusCode, 200);

  auto response = Json::parse(ret.body);
  ASSERT_TRUE(response.is_array());

  auto sessionIt = std::find(response.begin(), response.end(), "local");
  ASSERT_NE(sessionIt, response.end());
}

/// @test
/// End-to-end test for type introspection endpoint
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, type_introspection)
{
  Server server;

  authenticate();

  auto ret = request(HttpMethod::httpGet, "/api/types/string", Json());
  ASSERT_EQ(ret.statusCode, 200);

  auto typeInfo = Json::parse(ret.body);
  ASSERT_EQ(typeInfo["name"].get<std::string>(), "string");
}

/// End-to-end test for interests retrieval (empty list)
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, get_interests)
{
  Server server;

  authenticate();

  auto ret = request(HttpMethod::httpGet, "/api/interests", Json());

  ASSERT_EQ(ret.statusCode, 200);
  auto interests = Json::parse(ret.body);
  ASSERT_TRUE(interests.is_array());
  ASSERT_EQ(interests.size(), 0);
}

/// @test
/// End-to-end test for successful interest creation
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, create_interest)
{
  Server server;

  authenticate();

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
    auto ret = request(
      HttpMethod::httpPost, "/api/interests", Json {{"name", interestName}, {"query", "SELECT * FROM local.kernel"}});

    ASSERT_EQ(ret.statusCode, 200);
    auto response = Json::parse(ret.body);
    ASSERT_TRUE(response.contains("name"));
    ASSERT_EQ(response["name"], interestName);
  }
}

/// @test
/// End-to-end test for invalid query on interest creation
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, create_interest_invalid_query)
{
  Server server;

  authenticate();

  auto ret =
    request(HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "INVALID QUERY"}});

  ASSERT_EQ(ret.statusCode, 400);
}

/// @test
/// End-to-end test for malformed request to create interest
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, create_interest_malformed_request)
{
  Server server;

  authenticate();

  auto ret = request(HttpMethod::httpPost, "/api/interests", std::nullopt);
  ASSERT_EQ(ret.statusCode, 400);
}

/// @test
/// End-to-end test for getting an existing interest
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, get_interest_success)
{
  Server server;

  authenticate();

  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  auto ret = retryUntil(200, [this]() { return request(HttpMethod::httpGet, "/api/interests/test_interest", Json()); });

  ASSERT_EQ(ret.statusCode, 200);

  auto response = Json::parse(ret.body);
  ASSERT_TRUE(response.contains("name"));
  ASSERT_EQ(response["name"].get<std::string>(), "test_interest");
}

/// @test
/// End-to-end test for getting an unknown interest
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, get_interest_unknown_interest)
{
  Server server;

  authenticate();

  auto ret = request(HttpMethod::httpGet, "/api/interests/test_interest", Json());
  ASSERT_EQ(ret.statusCode, 404);
}

/// @test
/// End-to-end test for removing an interest
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, remove_interest)
{
  Server server;

  authenticate();

  // Create an interest
  auto ret = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(ret.statusCode, 200);

  // Check interest was created
  ret = retryUntil(200, [this]() { return request(HttpMethod::httpGet, "/api/interests/test_interest", Json()); });

  ASSERT_EQ(ret.statusCode, 200);
  auto response = Json::parse(ret.body);
  ASSERT_TRUE(response.contains("name"));
  ASSERT_EQ(response["name"].get<std::string>(), "test_interest");

  // Delete the interest
  ret = request(HttpMethod::httpDelete, "/api/interests/test_interest", Json());
  ASSERT_EQ(ret.statusCode, 200);

  // Check there is no active interests
  ret = retryUntil(200, [this]() { return request(HttpMethod::httpGet, "/api/interests", Json()); });

  ASSERT_EQ(ret.statusCode, 200);
  auto interests = Json::parse(ret.body);
  ASSERT_TRUE(interests.is_array());
  ASSERT_EQ(interests.size(), 0);
}

/// @test
/// End-to-end test for getting session details when client has not created any interest on session's buses
TEST_F(RestE2EFixture, get_session_no_buses)
{
  Server server;

  authenticate();

  // Get session
  auto ret = retryUntil(200, [this]() { return request(HttpMethod::httpGet, "/api/sessions/local", Json()); });

  ASSERT_EQ(ret.statusCode, 200);

  auto response = Json::parse(ret.body);
  ASSERT_TRUE(response.contains("name"));
  ASSERT_EQ(response["name"].get<std::string>(), "local");
  ASSERT_TRUE(response.contains("buses"));
  ASSERT_TRUE(response["buses"].is_array());
  ASSERT_TRUE(response["buses"].empty());
}

/// @test
/// End-to-end test for getting session details when client has created an interest on session's bus
TEST_F(RestE2EFixture, get_session_with_buses)
{
  Server server;

  authenticate();

  // Create an interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  // Get session
  auto ret = retryUntil(200, [this]() { return request(HttpMethod::httpGet, "/api/sessions/local", Json()); });

  ASSERT_EQ(ret.statusCode, 200);

  auto response = Json::parse(ret.body);
  ASSERT_TRUE(response.contains("name"));
  ASSERT_EQ(response["name"].get<std::string>(), "local");
  ASSERT_TRUE(response.contains("buses"));
  ASSERT_TRUE(response["buses"].is_array());

  auto busIt = std::find(response["buses"].begin(), response["buses"].end(), "kernel");
  ASSERT_NE(busIt, response["buses"].end());
}

/// @test
/// End-to-end test for getting non-existing session
TEST_F(RestE2EFixture, get_non_existing_session)
{
  Server server;

  authenticate();

  auto ret = request(HttpMethod::httpPost, "/api/sessions/nonExistingSession", Json());
  ASSERT_EQ(ret.statusCode, 404);
}

/// @test
/// End-to-end test for getting objects in an interest
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, get_objects)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  // Get objects
  auto ret = request(HttpMethod::httpGet, "/api/interests/test_interest/objects", Json());
  ASSERT_EQ(ret.statusCode, 200);

  auto response = Json::parse(ret.body);
  ASSERT_TRUE(response.is_array());
}

/// @test
/// End-to-end test for getting existing objects
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, get_existing_objects)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Get objects
  auto ret =
    retryUntil(200, [this]() { return request(HttpMethod::httpGet, "/api/interests/test_interest/objects", Json()); });
  ASSERT_EQ(ret.statusCode, 200);

  auto interests = Json::parse(ret.body);
  ASSERT_TRUE(interests.is_array());
  ASSERT_GT(interests.size(), 0);
}

/// @test
/// End-to-end test for getting existing object
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, get_existing_object)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Get object
  auto ret = retryUntil(
    200, [this]() { return request(HttpMethod::httpGet, "/api/interests/test_interest/objects/api", Json()); });

  ASSERT_EQ(ret.statusCode, 200);

  auto interests = Json::parse(ret.body);
  ASSERT_TRUE(interests.is_object());
  ASSERT_EQ(interests["localName"], "rest.local.kernel.api");
}

/// @test
/// End-to-end test for getting a non-existing object
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, get_non_existing_object)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  // Try to get a non-existent object
  auto ret = request(HttpMethod::httpGet, "/api/interests/test_interest/objects/test_object", Json());
  ASSERT_EQ(ret.statusCode, 404);
}

/// @test
/// End-to-end test for getting an object property and event subscriptions
TEST_F(RestE2EFixture, get_subcriptions)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Get object subscriptions
  auto ret = retryUntil(
    200,
    [this]() { return request(HttpMethod::httpGet, "/api/interests/test_interest/objects/api/subscription", Json()); });

  ASSERT_EQ(ret.statusCode, 200);

  auto response = Json::parse(ret.body);
  ASSERT_TRUE(response.contains("properties"));
  ASSERT_TRUE(response.contains("events"));
  ASSERT_TRUE(response["properties"].is_array());
  ASSERT_TRUE(response["events"].is_array());
}

/// @test
/// End-to-end test for updating object subscriptions when body is empty
TEST_F(RestE2EFixture, update_subscriptions_empty)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Update object subscriptions
  auto updateRet = retryUntil(
    200,
    [this]() { return request(HttpMethod::httpPut, "/api/interests/test_interest/objects/api/subscription", Json()); });

  ASSERT_EQ(updateRet.statusCode, 200);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Get object subscriptions
  auto getRet = retryUntil(
    200,
    [this]() { return request(HttpMethod::httpGet, "/api/interests/test_interest/objects/api/subscription", Json()); });

  ASSERT_EQ(getRet.statusCode, 200);

  auto response = Json::parse(getRet.body);
  ASSERT_TRUE(response["properties"].empty());
  ASSERT_TRUE(response["events"].empty());
}

/// @test
/// End-to-end test for updating object property subscriptions
TEST_F(RestE2EFixture, update_subscriptions_property)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Update object subscriptions
  auto updateRet = retryUntil(200,
                              [this]()
                              {
                                return request(HttpMethod::httpPut,
                                               "/api/interests/test_interest/objects/api/subscription",
                                               Json {{"properties", {"buildInfo"}}});
                              });

  ASSERT_EQ(updateRet.statusCode, 200);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Get object subscriptions
  auto getRet = retryUntil(
    200,
    [this]() { return request(HttpMethod::httpGet, "/api/interests/test_interest/objects/api/subscription", Json()); });

  ASSERT_EQ(getRet.statusCode, 200);

  auto response = Json::parse(getRet.body);
  auto propIt = find(response["properties"].begin(), response["properties"].end(), "buildInfo");
  ASSERT_NE(propIt, response["properties"].end());
  ASSERT_TRUE(response["events"].empty());
}

/// @test
/// End-to-end test for updating object subscriptions of non-existing members
TEST_F(RestE2EFixture, update_subscriptions_non_existing_members)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  auto updateRet = request(HttpMethod::httpPut,
                           "/api/interests/test_interest/objects/api/subscription",
                           Json {{"properties", {"nonExistingProperty"}}});
  ASSERT_EQ(updateRet.statusCode, 404);
}

/// @test
/// End-to-end test getting existing object with its properties when optional query param is true
TEST_F(RestE2EFixture, get_existing_object_including_properties)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Try to get an object properties
  auto ret = request(HttpMethod::httpGet, "/api/interests/test_interest/objects/api?includeValues=true", Json());
  ASSERT_EQ(ret.statusCode, 200);

  auto interests = Json::parse(ret.body);
  ASSERT_TRUE(interests.is_object());
  ASSERT_TRUE(interests.contains("properties"));
  ASSERT_TRUE(interests["properties"].is_object());
}

/// @test
/// End-to-end test getting existing object without its properties when optional query param is not true
TEST_F(RestE2EFixture, get_existing_object_not_including_properties)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Try to not get an object properties
  auto ret = request(HttpMethod::httpGet, "/api/interests/test_interest/objects/api?includeValues=false", Json());
  ASSERT_EQ(ret.statusCode, 200);

  auto interests = Json::parse(ret.body);
  ASSERT_TRUE(interests.is_object());
  ASSERT_FALSE(interests.contains("properties"));
}

/// @test
/// End-to-end test for successful interest creation including auto-subscription
TEST_F(RestE2EFixture, create_interest_autosubscription)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost,
    "/api/interests",
    Json {
      {"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}, {"autoSubscribe", {{"properties", true}}}});
  ASSERT_EQ(createRet.statusCode, 200);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Get object subscriptions
  auto ret = retryUntil(
    200,
    [this]() { return request(HttpMethod::httpGet, "/api/interests/test_interest/objects/api/subscription", Json()); });

  ASSERT_EQ(ret.statusCode, 200);

  auto response = Json::parse(ret.body);
  ASSERT_FALSE(response["properties"].empty());
}

/// @test
/// End-to-end test for successful interest creation without auto-subscription
TEST_F(RestE2EFixture, create_interest_no_autosubscription)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(HttpMethod::httpPost,
                           "/api/interests",
                           Json {{"name", "test_interest"},
                                 {"query", "SELECT * FROM local.kernel"},
                                 {"autoSubscribe", {{"properties", false}, {"events", false}}}});
  ASSERT_EQ(createRet.statusCode, 200);

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Get object subscriptions
  auto ret = retryUntil(
    200,
    [this]() { return request(HttpMethod::httpGet, "/api/interests/test_interest/objects/api/subscription", Json()); });

  ASSERT_EQ(ret.statusCode, 200);

  auto response = Json::parse(ret.body);
  ASSERT_TRUE(response["properties"].empty());
  ASSERT_TRUE(response["events"].empty());
}

/// @test
/// End-to-end test for malformed request to create interest with auto-subscription
TEST_F(RestE2EFixture, create_interest_malformed_request_autosubscribe)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(HttpMethod::httpPost,
                           "/api/interests",
                           Json {{"name", "test_interest"},
                                 {"query", "SELECT * FROM local.kernel"},
                                 {"autoSubscribe", {{"properties", "nonValidRequest"}}}});
  ASSERT_EQ(createRet.statusCode, 400);
}

/// @test
/// End-to-end test for getting a method definition
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, get_method_definition)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  // Get method definition
  HttpResponse ret = retryUntil(
    200,
    [this]()
    { return request(HttpMethod::httpGet, "/api/interests/test_interest/objects/api/methods/shutdown", Json()); });
  ASSERT_EQ(ret.statusCode, 200);

  auto res = Json::parse(ret.body);
  ASSERT_TRUE(res.is_object());
  ASSERT_EQ(res["name"], "shutdown");
}

/// @test
/// End-to-end test to verify all returned definition links are accessible
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, get_all_method_definitions)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  // Retrieve object definition
  HttpResponse ret = retryUntil(
    200, [this]() { return request(HttpMethod::httpGet, "/api/interests/test_interest/objects/api", Json()); });
  ASSERT_EQ(ret.statusCode, 200);

  auto res = Json::parse(ret.body);
  ASSERT_TRUE(res.is_object());

  // Walk all definition links
  for (const auto& link: res["links"])
  {
    if (link["rel"] != "def")
    {
      continue;
    }
    auto defRet = request(HttpMethod::httpGet, link["href"], Json());
    ASSERT_EQ(defRet.statusCode, 200);
  }
}

/// @test
/// End-to-end test for getting property definition
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, get_property_definition)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  // Get method definition
  auto ret = retryUntil(
    200,
    [this]()
    { return request(HttpMethod::httpGet, "/api/interests/test_interest/objects/api/properties/buildInfo", Json()); });
  ASSERT_EQ(ret.statusCode, 200);

  auto res = Json::parse(ret.body);
  ASSERT_TRUE(res.is_object());
}

/// @test
/// End-to-end test for subscribing and unsubscribing to property updates
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, property_subscription)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  auto ret = retryUntil(200,
                        [this]()
                        {
                          return request(HttpMethod::httpPost,
                                         "/api/interests/test_interest/objects/api/properties/buildInfo/subscribe",
                                         Json());
                        });
  ASSERT_TRUE(ret.statusCode == 200);

  ret = retryUntil(200,
                   [this]()
                   {
                     return request(HttpMethod::httpPost,
                                    "/api/interests/test_interest/objects/api/properties/buildInfo/unsubscribe",
                                    Json());
                   });
  ASSERT_TRUE(ret.statusCode == 200);
}

/// @test
/// Notification immediately received with current value after creating a property subscription
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, property_subscription_notification)
{
  Server server;

  authenticate();

  std::atomic<sen::TimeStamp> timestamp = sen::TimeStamp(std::chrono::system_clock::now().time_since_epoch());
  std::atomic<bool> cancelToken = false;

  // Query notifications from a different thread
  auto future = std::async(
    std::launch::async,
    [this, &cancelToken, &timestamp]()
    {
      auto retVal = requestSSE(
        "/api/sse", cancelToken, [&timestamp](std::string value) { return value.rfind("event: property", 0); });
      ASSERT_TRUE(retVal);
    });

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  timestamp = sen::TimeStamp(std::chrono::system_clock::now().time_since_epoch());
  auto ret = retryUntil(200,
                        [this]()
                        {
                          return request(HttpMethod::httpPost,
                                         "/api/interests/test_interest/objects/api/properties/buildInfo/subscribe",
                                         Json());
                        });
  ASSERT_TRUE(ret.statusCode == 200);

  EXPECT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
  cancelToken = true;
}

/// @test
/// End-to-end test for invoking a method
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, invoke_method)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  // Invoke method
  auto ret = retryUntil(200,
                        [this]()
                        {
                          return request(HttpMethod::httpPost,
                                         "/api/interests/test_interest/objects/api/methods/getUnits/invoke",
                                         Json::array());
                        });
  ASSERT_EQ(ret.statusCode, 200);

  auto res = Json::parse(ret.body);
  ASSERT_TRUE(res.is_object());
  ASSERT_TRUE(res["status"] == "finished" || res["status"] == "pending");

  ret =
    retryUntil(200,
               [this, &res]()
               {
                 return request(HttpMethod::httpGet,
                                "/api/interests/test_interest/objects/api/methods/getUnits/invoke/" + res["id"].dump(),
                                Json());
               });
  ASSERT_EQ(ret.statusCode, 200);

  res = Json::parse(ret.body);
  ASSERT_TRUE(res.is_object());
  ASSERT_TRUE(res["status"] == "finished" || res["status"] == "pending");
}

/// @test
/// End-to-end test for notification subscription
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, notification_subscription)
{
  Server server;

  authenticate();

  // Create interest
  auto ret = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(ret.statusCode, 200);

  // Query notifications from a different thread
  std::atomic<bool> cancelToken = false;
  auto future = std::async(std::launch::async,
                           [this, &cancelToken]()
                           {
                             auto retVal = requestSSE("/api/sse", cancelToken, [](std::string) { return false; });
                             ASSERT_TRUE(retVal);
                           });

  // Invoke a method
  ret = retryUntil(200,
                   [this]()
                   {
                     return request(HttpMethod::httpPost,
                                    "/api/interests/test_interest/objects/api/methods/getUnits/invoke",
                                    Json::array());
                   });
  ASSERT_EQ(ret.statusCode, 200);

  EXPECT_EQ(future.wait_for(std::chrono::seconds(5)), std::future_status::ready);
  cancelToken = true;
}
