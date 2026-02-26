// === http_request.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_HTTP_REQUEST_H
#define SEN_COMPONENTS_REST_SRC_HTTP_REQUEST_H

// component
#include "http_header.h"
#include "jwt.h"

// std
#include <optional>
#include <string>
#include <unordered_map>

namespace sen::components::rest
{

using HttpHeaders = std::unordered_map<std::string, HttpHeader>;

/// HttpRequest class stores all the resources related to a HTTP request and optionally the client
/// session which it belongs to.
class HttpRequest
{
public:
  /// Returns request URL path.
  const std::string& path() const { return path_; }

  /// Returns the request HTTP method.
  const std::string& method() const { return method_; }

  /// Returns the request raw body.
  const std::string& body() const { return body_; }

  /// Returns all the HTTP headers for this request.
  const HttpHeaders& headers() const { return headers_; }

  /// Returns the session token if it exists.
  std::optional<JWT> sessionToken() const;

private:
  friend class HttpSession;

  std::string path_;
  std::string method_;
  std::string body_;
  HttpHeaders headers_;
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_HTTP_REQUEST_H
