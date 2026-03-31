// === request.cpp =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "request.h"

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

// std
#include <cstddef>
#include <cstring>
#include <iostream>  //NOLINT
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

using Json = nlohmann::json;
using sen::components::rest::HttpMethod;

constexpr std::string_view eventHeader = "event: ";

HttpResponse request(const HttpMethod& method,
                     const std::string& host,
                     const std::string& port,
                     const std::string& path,
                     const Json& data,
                     const std::string& token,
                     bool isSSE)
{
  HttpResponse result {0, ""};

  asio::io_context context;
  asio::ip::tcp::resolver resolver(context);
  asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(host, port);

  asio::ip::tcp::socket socket(context);
  asio::connect(socket, endpoints);  // NOLINT

  std::string payload;
  size_t payloadSize = 0;
  std::string request;
  switch (method)
  {
    case HttpMethod::httpPost:
      request = "POST";
      payload = data.dump();
      payloadSize = payload.size();
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
  if (method == HttpMethod::httpPost)
  {
    request.append("\r\nContent-Length: ");
    request.append(std::to_string(payloadSize));
  }
  request.append("\r\n\r\n");
  if (method == HttpMethod::httpPost)
  {
    request.append(payload);
  }

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
    if (isSSE && len >= eventHeader.size() && memcmp(buf, eventHeader.data(), eventHeader.size()) == 0)
    {
      std::cout << buf << std::endl;
      return HttpResponse {200, response};
    }
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

std::optional<std::string> authenticate()
{
  auto authRet = request(HttpMethod::httpPost, "127.0.0.1", "12345", "/api/auth", Json {{"id", "admin"}});
  if (authRet.statusCode != 200)
  {
    return std::nullopt;
  }

  auto authResponse = Json::parse(authRet.body);
  return authResponse["token"].get<std::string>();
}
