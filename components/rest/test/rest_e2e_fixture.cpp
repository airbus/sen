// === rest_e2e_fixture.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "rest_e2e_fixture.h"

// auto-generated code
#include "stl/types.stl.h"

// asio
#include <asio/buffer.hpp>
#include <asio/connect.hpp>  // NOLINT(misc-include-cleaner)
#include <asio/error.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/system_error.hpp>
#include <asio/write.hpp>  // NOLINT(misc-include-cleaner)

// json
#include <nlohmann/json.hpp>

// google test
#include <gtest/gtest.h>

// std
#include <atomic>
#include <cstring>
#include <functional>
#include <iostream>  //NOLINT
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

using Json = nlohmann::json;
using sen::components::rest::HttpMethod;

constexpr std::string_view eventHeader = "event: ";

std::string buildHTTPRequest(const HttpMethod& method,
                             const std::string& host,
                             const std::string& port,
                             const std::string& path,
                             const std::optional<Json> data,
                             const std::string& token)
{
  std::string request;
  std::string payload;
  size_t payloadSize = 0;

  switch (method)
  {
    case HttpMethod::httpPut:
      request = "PUT";
      if (data.has_value())
      {
        payload = data->dump();
        payloadSize = payload.size();
      }
      break;
    case HttpMethod::httpPost:
      request = "POST";
      if (data.has_value())
      {
        payload = data->dump();
        payloadSize = payload.size();
      }
      break;
    case HttpMethod::httpDelete:
      request = "DELETE";
      break;
    case HttpMethod::httpGet:
    default:
      request = "GET";
  }

  request.append(" ");
  request.append(path);
  request.append(" HTTP/1.1");
  request.append("\r\nHost: ");
  request.append(host);
  request.append(":");
  request.append(port);
  request.append("\r\nContent-Type: application/json");
  if (!token.empty())
  {
    request.append("\r\nAuthorization: Bearer ");
    request.append(token);
  }
  if (method == HttpMethod::httpPost || method == HttpMethod::httpPut)
  {
    request.append("\r\nContent-Length: ");
    request.append(std::to_string(payloadSize));
  }
  request.append("\r\n\r\n");
  if (method == HttpMethod::httpPost || method == HttpMethod::httpPut)
  {
    request.append(payload);
  }

  return request;
}

HttpResponse RestE2EFixture::request(const HttpMethod& method, const std::string& path, const std::optional<Json>& data)
{
  HttpResponse result {0, ""};

  asio::ip::tcp::socket socket(context_);
  asio::connect(socket, endpoints_);  // NOLINT

  std::string request = buildHTTPRequest(method, "127.0.0.1", "12345", path, data, token_.value());
  asio::write(socket, asio::buffer(request));  // NOLINT

  std::string response;
  asio::error_code error;
  while (true)
  {
    char buf[1024];
    size_t len = socket.read_some(asio::buffer(buf), error);
    if (error == asio::error::eof)
    {
      response.append(buf, len);
      break;
    }
    if (error)
    {
      throw asio::system_error(error);
    }

    response.append(buf, len);
  }

  size_t lineEnd = response.find("\r\n");
  if (lineEnd != std::string::npos)
  {
    std::string line = response.substr(0, lineEnd);

    std::istringstream statusStream(line);
    std::string httpVersion;
    statusStream >> httpVersion >> result.statusCode;
  }

  size_t bodyStart = response.find("\r\n\r\n");
  if (bodyStart != std::string::npos)
  {
    result.body = response.substr(bodyStart + 4);
  }

  return result;
}

bool RestE2EFixture::requestSSE(const std::string& path,
                                const std::atomic<bool>& cancelToken,
                                const std::function<bool(std::string)>& onNotification)
{
  asio::ip::tcp::socket socket(context_);
  asio::connect(socket, endpoints_);  // NOLINT
  socket.non_blocking(true);

  std::string payload;
  std::string request = buildHTTPRequest(HttpMethod::httpGet, "127.0.0.1", "12345", path, std::nullopt, token_.value());
  asio::write(socket, asio::buffer(request));  // NOLINT

  std::string response;
  asio::error_code error;
  bool shallContinue = true;
  while (shallContinue && !cancelToken.load())
  {
    char buf[1024];
    size_t len = socket.read_some(asio::buffer(buf), error);

    if (error == asio::error::eof)
    {
      response.append(buf, len);
      return false;
    }
    if (error && error != asio::error::basic_errors::try_again && error != asio::error::would_block)
    {
      return false;
    }

    response.append(buf, len);
    if (len >= eventHeader.size() && memcmp(buf, eventHeader.data(), eventHeader.size()) == 0)
    {
      shallContinue = onNotification(buf);
    }
  }

  return true;
}

void RestE2EFixture::authenticate()
{
  auto authRet = request(HttpMethod::httpPost, "/api/auth", Json {{"id", "admin"}});
  if (authRet.statusCode != 200)
  {
    token_ = std::nullopt;
  }

  auto authResponse = Json::parse(authRet.body);
  token_ = authResponse["token"].get<std::string>();

  ASSERT_TRUE(token_.has_value());
}
