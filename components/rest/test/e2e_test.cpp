// === e2e_test.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "component.h"
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
/// Serves GET /api/version over a live HTTP connection with 200 and a JSON body whose version field equals
/// the compiled-in Sen version string.
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
/// Issues a decodable, valid JWT on POST /api/auth with a client id, answering 200.
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
/// Lists the known sessions on GET /api/sessions as a JSON array that includes the local session.
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
/// Serves type introspection on GET /api/types/string, the returned JSON naming the string type.
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

/// @test
/// Returns an empty JSON array from GET /api/interests while the client has created no interest.
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
/// Creates an interest on POST /api/interests and echoes its name back with 200, accepting names that mix
/// underscores, dashes, semicolons, commas, and at characters.
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
/// Rejects interest creation with 400 when the submitted query does not parse.
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
/// Rejects interest creation with 400 when the POST carries no JSON body.
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, create_interest_malformed_request)
{
  Server server;

  authenticate();

  auto ret = request(HttpMethod::httpPost, "/api/interests", std::nullopt);
  ASSERT_EQ(ret.statusCode, 400);
}

/// @test
/// Serves a created interest on GET /api/interests/test_interest with 200 and a body echoing that name.
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
/// Answers 404 on GET /api/interests/name when no interest of that name has been created.
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, get_interest_unknown_interest)
{
  Server server;

  authenticate();

  auto ret = request(HttpMethod::httpGet, "/api/interests/test_interest", Json());
  ASSERT_EQ(ret.statusCode, 404);
}

/// @test
/// Deletes a created interest on DELETE /api/interests/name with 200, after which GET /api/interests
/// serves an empty list.
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
/// Serves session details on GET /api/sessions/local with an empty buses array while the client holds no
/// interest on any of the session's buses.
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
/// Lists the kernel bus in the session details served by GET /api/sessions/local once the client has an
/// interest on local.kernel.
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
/// Answers 404 when a request names a session that does not exist.
TEST_F(RestE2EFixture, get_non_existing_session)
{
  Server server;

  authenticate();

  auto ret = request(HttpMethod::httpPost, "/api/sessions/nonExistingSession", Json());
  ASSERT_EQ(ret.statusCode, 404);
}

/// @test
/// Serves the object list of a created interest on GET /api/interests/name/objects as a JSON array.
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
/// Fills the interest's object list once its query resolves against local.kernel, GET .../objects then
/// serving a non-empty JSON array.
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
/// Serves a single resolved object on GET .../objects/api, the body carrying the fully qualified localName
/// rest.local.kernel.api.
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
/// Answers 404 on GET for an object name the interest's query did not match.
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
/// Serves an object's subscription state on GET .../objects/api/subscription as JSON with properties and
/// events arrays.
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
/// Accepts a subscription update with an empty body on PUT .../subscription, a subsequent GET showing both
/// the properties and events lists empty.
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
/// Subscribes an object property through PUT .../subscription naming buildInfo, a subsequent GET listing
/// buildInfo under properties and no events.
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
/// Rejects with 404 a subscription update naming a property the object does not have.
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
/// Includes a properties object in the body served for an object when the includeValues query parameter is
/// true.
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
/// Omits the properties object from the body served for an object when the includeValues query parameter
/// is false.
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
/// Subscribes object properties automatically when the interest is created with autoSubscribe properties
/// true, the subscription list arriving non-empty without any explicit subscribe call.
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
/// Leaves both subscription lists empty when the interest is created with autoSubscribe properties and
/// events false.
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
/// Rejects interest creation with 400 when the autoSubscribe block carries a non-boolean value.
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
/// Serves a method definition on GET .../objects/api/methods/shutdown, the JSON body naming the method.
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
/// Fetches an object body and follows every link it carries with rel def, each definition href answering
/// 200.
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
/// Serves a property definition on GET .../objects/api/properties/buildInfo as a JSON object.
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
/// Accepts a subscribe and then an unsubscribe POST for an object property, each answering 200.
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
/// Pushes a property event over the /api/sse stream within five seconds of a property subscription being
/// created, delivering the current value without waiting for a change.
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
/// Invokes an object method through POST .../methods/getUnits/invoke, receiving an invocation id with
/// status pending or finished that can then be polled through GET on the invocation URL.
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
/// Rejects a method invocation with 400 when the argument array carries too few or too many entries.
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, invoke_method_wrong_argument_count)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  const std::string url = "/api/interests/test_interest/objects/clock/methods/processNoFlush/invoke";

  auto ret = retryUntil(400, [this, &url]() { return request(HttpMethod::httpPost, url, Json::array()); });
  ASSERT_EQ(ret.statusCode, 400);

  ret = retryUntil(400, [this, &url]() { return request(HttpMethod::httpPost, url, Json::array({1000, 2000})); });
  ASSERT_EQ(ret.statusCode, 400);
}

/// @test
/// Rejects a method invocation with 400 when an argument cannot be adapted to the parameter type, such as
/// a string or boolean offered for a Duration, and records no invocation for the rejected request.
/// @requirements(SEN-1061)
TEST_F(RestE2EFixture, invoke_method_wrong_argument_type)
{
  Server server;

  authenticate();

  // Create interest
  auto createRet = request(
    HttpMethod::httpPost, "/api/interests", Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}});
  ASSERT_EQ(createRet.statusCode, 200);

  const std::string url = "/api/interests/test_interest/objects/clock/methods/processNoFlush/invoke";

  // Non-numeric string not adaptable to a Duration
  auto ret =
    retryUntil(400, [this, &url]() { return request(HttpMethod::httpPost, url, Json::array({"not a duration"})); });
  ASSERT_EQ(ret.statusCode, 400);

  auto error = Json::parse(ret.body)["error"].get<std::string>();

  // Boolean not adaptable to a Duration
  ret = retryUntil(400, [this, &url]() { return request(HttpMethod::httpPost, url, Json::array({true})); });
  ASSERT_EQ(ret.statusCode, 400);

  // Rejected request must not create interests
  auto invokeRet = request(HttpMethod::httpGet, url + "/0", Json());
  EXPECT_EQ(invokeRet.statusCode, 404);
}

/// @test
/// Streams a notification over the /api/sse channel when a method is invoked, the listening client
/// receiving its first event within five seconds.
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

/// @test
/// Loads the rest component from a YAML kernel config and exposes the configured listen address 127.0.0.1
/// and port 12345 on the component instance.
/// @requirements(SEN-1061)
TEST(Rest, success_default_config)
{
  std::string configString = R"(
    load:
    - name: rest
      group: 3
      address: "127.0.0.1"
      port: 12345
  )";

  auto kernel = sen::kernel::TestKernel::fromYamlString(configString);
  auto context = kernel.getComponentContext("rest");
  ASSERT_TRUE(context.has_value());

  auto component = dynamic_cast<const sen::components::rest::RestAPIComponent*>(context.value()->instance);

  ASSERT_EQ(component->getListenAddress(), "127.0.0.1");
  ASSERT_EQ(component->getListenPort(), 12345);
}
