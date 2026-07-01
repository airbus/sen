// === rest_e2e_fixture.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_TEST_REST_E2E_FIXTURE_H
#define SEN_COMPONENTS_REST_TEST_REST_E2E_FIXTURE_H

// auto-generated code
#include "stl/types.stl.h"

// asio
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

// google test
#include <gtest/gtest.h>

// json
#include <nlohmann/json.hpp>

// std
#include <atomic>
#include <functional>
#include <optional>
#include <string>

using HttpMethod = sen::components::rest::HttpMethod;
using Json = nlohmann::json;

struct HttpResponse
{
  int statusCode;
  std::string body;
};

class RestE2EFixture: public testing::Test
{
protected:
  void SetUp() override { endpoints_ = resolver_.resolve("127.0.0.1", "12345"); }

  void authenticate();

  HttpResponse request(const HttpMethod& method, const std::string& path, const std::optional<Json>& data = Json());

  bool requestSSE(const std::string& path,
                  const std::atomic<bool>& cancelToken,
                  const std::function<bool(std::string)>& onNotification);

private:
  std::optional<std::string> token_ = "";
  asio::io_context context_;
  asio::ip::tcp::resolver resolver_ {context_};
  asio::ip::tcp::resolver::results_type endpoints_;
};

#endif  // SEN_COMPONENTS_REST_TEST_REST_E2E_FIXTURE_H
