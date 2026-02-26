// === http_response.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_HTTP_RESPONSE_H
#define SEN_COMPONENTS_REST_SRC_HTTP_RESPONSE_H

#include "http_header.h"

// asio
#include <asio/ip/tcp.hpp>

// std
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace sen::components::rest
{

constexpr size_t maxHeaderSize = 4096;

constexpr int32_t httpBadRequestError = 400;
constexpr int32_t httpNotFoundError = 404;
constexpr int32_t httpUnauthorizedError = 401;
constexpr int32_t httpInternalServerError = 500;
constexpr int32_t httpSuccess = 200;

/// HttpResponse class abstracts the response of a HTTP route callback
class HttpResponse
{
public:
  HttpResponse();
  explicit HttpResponse(uint32_t statusCode,
                        const std::vector<HttpHeader>& headers = std::vector<HttpHeader>(),
                        const std::string& body = "",
                        bool allowCors = true);

public:
  /// Returns the reason phrases defined in the HTTP specifications if it exists.
  [[nodiscard]] std::optional<std::string> getReasonPhrase() const;

  /// Returns the raw HTTP status code
  [[nodiscard]] uint32_t getStatusCode() const;

  /// Returns the response as a raw buffer.
  [[nodiscard]] std::string toBuffer() const;

  /// Writes the response to a given socket.
  [[nodiscard]] std::error_code socketWrite(asio::ip::tcp::socket& socket) const;

protected:
  void insertAllowCorsHeaders();
  void setBody(const std::string& body) { body_ = body; }

private:
  uint32_t statusCode_ = httpNotFoundError;
  std::vector<HttpHeader> headers_;
  std::string body_;
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_HTTP_RESPONSE_H
