// === e2e_test.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "request.h"

// sen
#include "sen/kernel/test_kernel.h"

// google test
#include <gtest/gtest.h>

// json
#include <jwt.h>
#include <nlohmann/json.hpp>

// std
#include <chrono>
#include <iostream>  //NOLINT
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
      threadPoolSize: 5
  )";

void runKernelSteps(unsigned int seconds, sen::kernel::TestKernel& kernel)
{
  for (unsigned int step = 0; step <= seconds * 2; step++)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    kernel.step();
  }
}

/// @test
/// End-to-end test for sessions retrieval
/// @requirements(SEN-1061)
TEST(Rest, e2e_sessions)
{
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

  auto ret = request(HttpMethod::httpGet, "127.0.0.1", "12345", "/api/sessions");
  ASSERT_EQ(ret.statusCode, 200);
  ASSERT_EQ(Json::parse(ret.body).dump(), R"(["local"])");
}

/// @test
/// End-to-end test for client authentication
/// @requirements(SEN-1061)
TEST(Rest, e2e_auth)
{
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

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
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

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
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

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
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

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
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

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
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

  auto token = authenticate();
  ASSERT_TRUE(token.has_value());

  auto createRet = request(HttpMethod::httpPost,
                           "127.0.0.1",
                           "12345",
                           "/api/interests",
                           Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}},
                           token.value());
  ASSERT_EQ(createRet.statusCode, 200);

  auto ret = request(HttpMethod::httpGet, "127.0.0.1", "12345", "/api/interests/test_interest", Json(), token.value());
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
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

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
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

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
  ret = request(HttpMethod::httpGet, "127.0.0.1", "12345", "/api/interests/test_interest", Json(), token.value());
  ASSERT_EQ(ret.statusCode, 200);
  auto response = Json::parse(ret.body);
  ASSERT_TRUE(response.contains("name"));
  ASSERT_EQ(response["name"].get<std::string>(), "test_interest");

  // Delete the interest
  ret = request(HttpMethod::httpDelete, "127.0.0.1", "12345", "/api/interests/test_interest", Json(), token.value());
  ASSERT_EQ(ret.statusCode, 200);

  // Check there is no active interests
  ret = request(HttpMethod::httpGet, "127.0.0.1", "12345", "/api/interests", Json(), token.value());
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
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

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
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

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

  runKernelSteps(2, kernel);

  // Get objects
  auto ret =
    request(HttpMethod::httpGet, "127.0.0.1", "12345", "/api/interests/test_interest/objects", Json(), token.value());
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
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

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

  runKernelSteps(2, kernel);

  // Get object
  auto ret = request(
    HttpMethod::httpGet, "127.0.0.1", "12345", "/api/interests/test_interest/objects/api", Json(), token.value());
  ASSERT_EQ(ret.statusCode, 200);

  std::cout << ret.body << std::endl;

  auto interests = Json::parse(ret.body);
  ASSERT_TRUE(interests.is_object());
  ASSERT_EQ(interests["localname"], "rest.local.kernel.api");
}

/// @test
/// End-to-end test for getting an non-existing object
/// @requirements(SEN-1061)
// TODO(SEN-1377): Disabled for now
// TEST(Rest, e2e_get_non_existing_object)
// {
//   auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
//   kernel.step();
//
//   // Authenticate
//   auto token = authenticate();
//   ASSERT_TRUE(token.has_value());
//
//   // Create interest
//   auto createRet = request(HttpMethod::httpPost,
//                            "127.0.0.1",
//                            "12345",
//                            "/api/interests",
//                            Json {{"name", "test_interest"}, {"query", "SELECT * FROM local.kernel"}},
//                            token.value());
//   ASSERT_EQ(createRet.statusCode, 200);
//
//   // Try to get an non-existent object
//   auto ret = request(HttpMethod::httpGet,
//                      "127.0.0.1",
//                      "12345",
//                      "/api/interests/test_interest/objects/test_object",
//                      Json(),
//                      token.value());
//   ASSERT_EQ(ret.statusCode, 404);
// }

/// @test
/// End-to-end test for getting a method definition
/// @requirements(SEN-1061)
TEST(Rest, e2e_get_method_definition)
{
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

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

  runKernelSteps(2, kernel);

  // Get method definition
  auto ret = request(HttpMethod::httpGet,
                     "127.0.0.1",
                     "12345",
                     "/api/interests/test_interest/objects/api/methods/shutdown",
                     Json(),
                     token.value());
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
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

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

  runKernelSteps(2, kernel);

  // Get method definition
  auto ret = request(HttpMethod::httpGet,
                     "127.0.0.1",
                     "12345",
                     "/api/interests/test_interest/objects/api/properties/buildInfo",
                     Json(),
                     token.value());
  ASSERT_EQ(ret.statusCode, 200);

  auto res = Json::parse(ret.body);
  ASSERT_TRUE(res.is_object());
}

/// @test
/// End-to-end test for subscribing and unsubscribing to property updates
/// @requirements(SEN-1061)
TEST(Rest, e2e_property_subscription)
{
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

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

  runKernelSteps(2, kernel);

  auto ret = request(HttpMethod::httpPost,
                     "127.0.0.1",
                     "12345",
                     "/api/interests/test_interest/objects/api/properties/buildInfo/subscribe",
                     Json(),
                     token.value());
  ASSERT_TRUE(ret.statusCode == 200);

  ret = request(HttpMethod::httpPost,
                "127.0.0.1",
                "12345",
                "/api/interests/test_interest/objects/api/properties/buildInfo/unsubscribe",
                Json(),
                token.value());
  ASSERT_TRUE(ret.statusCode == 200);
}

/// @test
/// End-to-end test for invoking a method
/// @requirements(SEN-1061)
TEST(Rest, e2e_invoke_method)
{
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

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

  runKernelSteps(2, kernel);

  // Invoke method
  auto ret = request(HttpMethod::httpPost,
                     "127.0.0.1",
                     "12345",
                     "/api/interests/test_interest/objects/api/methods/getUnits/invoke",
                     Json::array(),
                     token.value());
  ASSERT_EQ(ret.statusCode, 200);

  auto res = Json::parse(ret.body);
  ASSERT_TRUE(res.is_object());
  ASSERT_EQ(res["status"], "pending");

  // Get invocation status
  runKernelSteps(2, kernel);

  ret = request(HttpMethod::httpGet,
                "127.0.0.1",
                "12345",
                "/api/interests/test_interest/objects/api/methods/getUnits/invoke/" + res["id"].dump(),
                Json(),
                token.value());
  ASSERT_EQ(ret.statusCode, 200);

  res = Json::parse(ret.body);
  ASSERT_TRUE(res.is_object());
  ASSERT_EQ(res["status"], "finished");
}

/// @test
/// End-to-end test for notification subscription
/// @requirements(SEN-1061)
TEST(Rest, e2e_notification_subscription)
{
  auto kernel = sen::kernel::TestKernel::fromYamlString(std::string(configString));
  kernel.step();

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

  runKernelSteps(2, kernel);

  // Query notifications from a different thread
  std::thread t(
    [token]()
    {
      ;
      auto ret = request(HttpMethod::httpGet, "127.0.0.1", "12345", "/api/sse", Json(), token.value(), true);
      ASSERT_EQ(ret.statusCode, 200);
    });

  // Invoke a method
  ret = request(HttpMethod::httpPost,
                "127.0.0.1",
                "12345",
                "/api/interests/test_interest/objects/api/methods/getUnits/invoke",
                Json::array(),
                token.value());
  ASSERT_EQ(ret.statusCode, 200);

  runKernelSteps(2, kernel);
  t.join();
}
