// === http_response.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "http_response.h"

// component
#include "http_header.h"

// asio
#include <asio/buffer.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>  // NOLINT(misc-include-cleaner)

// std
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

const std::unordered_map<uint32_t, std::string>& getStatusMap()
{
  static const std::unordered_map<uint32_t, std::string> statusMap = {{100, "Continue"},
                                                                      {101, "Switching Protocols"},
                                                                      {102, "Processing"},
                                                                      {200, "OK"},
                                                                      {201, "Created"},
                                                                      {202, "Accepted"},
                                                                      {203, "Non-Authoritative Information"},
                                                                      {204, "No Content"},
                                                                      {205, "Reset Content"},
                                                                      {206, "Partial Content"},
                                                                      {300, "Multiple Choices"},
                                                                      {301, "Moved Permanently"},
                                                                      {302, "Found"},
                                                                      {303, "See Other"},
                                                                      {304, "Not Modified"},
                                                                      {307, "Temporary Redirect"},
                                                                      {308, "Permanent Redirect"},
                                                                      {400, "Bad Request"},
                                                                      {401, "Unauthorized"},
                                                                      {403, "Forbidden"},
                                                                      {404, "Not Found"},
                                                                      {405, "Method Not Allowed"},
                                                                      {406, "Not Acceptable"},
                                                                      {408, "Request Timeout"},
                                                                      {409, "Conflict"},
                                                                      {410, "Gone"},
                                                                      {500, "Internal Server Error"},
                                                                      {501, "Not Implemented"},
                                                                      {502, "Bad Gateway"},
                                                                      {503, "Service Unavailable"},
                                                                      {504, "Gateway Timeout"}};
  return statusMap;
}

namespace sen::components::rest
{

//--------------------------------------------------------------------------------------------------------------
// HttpResponse
//--------------------------------------------------------------------------------------------------------------

HttpResponse::HttpResponse() { insertAllowCorsHeaders(); }

HttpResponse::HttpResponse(uint32_t statusCode,
                           const std::vector<HttpHeader>& headers,
                           const std::string& body,
                           bool allowCors)
  : statusCode_(statusCode), headers_(std::move(headers)), body_(std::move(body))
{
  if (allowCors)
  {
    insertAllowCorsHeaders();
  }
}

std::optional<std::string> HttpResponse::getReasonPhrase() const
{
  if (auto it = getStatusMap().find(statusCode_); it != getStatusMap().end())
  {
    return it->second;
  }
  return std::nullopt;
}

uint32_t HttpResponse::getStatusCode() const { return statusCode_; }

std::string HttpResponse::toBuffer() const
{
  std::string buffer;
  buffer.reserve(maxHeaderSize + body_.size());

  buffer.append("HTTP/1.1 ");
  buffer.append(std::to_string(statusCode_));

  const auto reasonPhrase = getReasonPhrase();
  if (reasonPhrase.has_value())
  {
    buffer.append(" ");
    buffer.append(reasonPhrase.value());
    buffer.append("\r\n");
  }
  else
  {
    buffer.append(" Bad Gateway\r\n");
  }

  bool isStream = false;
  for (const auto& header: headers_)
  {
    buffer.append(header.field().data());
    buffer.append(": ");
    buffer.append(header.value().data());
    buffer.append("\r\n");

    if (std::strncmp(header.value().data(), "text/event-stream", header.value().size()) == 0)
    {
      isStream = true;
    }
  }

  if (!isStream)
  {
    buffer.append("Content-Length: ");
    buffer.append(std::to_string(body_.size()));
    buffer.append("\r\n");
  }

  buffer.append("\r\n");
  buffer.append(body_);

  return buffer;
}

std::error_code HttpResponse::socketWrite(asio::ip::tcp::socket& socket) const
{
  std::error_code ec;

  asio::write(socket, asio::buffer(toBuffer()), ec);  // NOLINT(misc-include-cleaner)

  return ec;
}

void HttpResponse::insertAllowCorsHeaders()
{
  headers_.insert(headers_.end(),
                  {
                    HttpHeader("Access-Control-Allow-Origin", "*"),
                    HttpHeader("Access-Control-Allow-Headers", "*"),
                    HttpHeader("Access-Control-Allow-Methods", "GET, POST, DELETE, PUT, OPTIONS"),
                  });
}

}  // namespace sen::components::rest
