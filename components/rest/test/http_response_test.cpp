// === http_response_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "http_header.h"
#include "http_response.h"

// google test
#include <gtest/gtest.h>

// std
#include <string>
#include <vector>

using sen::components::rest::HttpHeader;
using sen::components::rest::HttpResponse;

std::string getAllowCorsHeaders()
{
  return "Access-Control-Allow-Origin: *\r\n"
         "Access-Control-Allow-Headers: *\r\n"
         "Access-Control-Allow-Methods: GET, POST, DELETE, PUT, OPTIONS\r\n";
}

/// @test
/// Check default HTTP message response
/// @requirements(SEN-1061)
TEST(Rest, default_response)
{
  HttpResponse response;
  ASSERT_EQ(response.getStatusCode(), 404);
  ASSERT_EQ(response.getReasonPhrase(), "Not Found");
  ASSERT_EQ(response.toBuffer(), "HTTP/1.1 404 Not Found\r\n" + getAllowCorsHeaders() + "Content-Length: 0\r\n\r\n");
}

/// @test
/// Check valid HTTP reason phrases for common status codes
/// @requirements(SEN-1061)
TEST(Rest, status_code)
{
  {
    HttpResponse response(200);
    ASSERT_EQ(response.getStatusCode(), 200);
    ASSERT_EQ(response.getReasonPhrase(), "OK");
  }
  {
    HttpResponse response(403);
    ASSERT_EQ(response.getStatusCode(), 403);
    ASSERT_EQ(response.getReasonPhrase(), "Forbidden");
  }
  {
    HttpResponse response(502);
    ASSERT_EQ(response.getStatusCode(), 502);
    ASSERT_EQ(response.getReasonPhrase(), "Bad Gateway");
  }
}

/// @test
/// Check HTTP message headers serialization
/// @requirements(SEN-1061)
TEST(Rest, serialization_headers)
{
  {
    HttpResponse response(200);
    ASSERT_EQ(response.toBuffer(), "HTTP/1.1 200 OK\r\n" + getAllowCorsHeaders() + "Content-Length: 0\r\n\r\n");
  }
  {
    HttpResponse response(200, std::vector<HttpHeader> {{"Content-Type", "text/javascript; charset=utf-8"}});
    ASSERT_EQ(response.toBuffer(),
              "HTTP/1.1 200 OK\r\n"
              "Content-Type: text/javascript; charset=utf-8\r\n" +
                getAllowCorsHeaders() + "Content-Length: 0\r\n\r\n");
  }
  {
    HttpResponse response(200,
                          std::vector<HttpHeader> {{"Cache-Control", "max-age=604800"},
                                                   {"Content-Type", "text/javascript; charset=utf-8"}});
    ASSERT_EQ(response.toBuffer(),
              "HTTP/1.1 200 OK\r\n"
              "Cache-Control: max-age=604800\r\nContent-Type: text/javascript; "
              "charset=utf-8\r\n" +
                getAllowCorsHeaders() + "Content-Length: 0\r\n\r\n");
  }
}

/// @test
/// Check HTTP message body serialization
/// @requirements(SEN-1061)
TEST(Rest, body)
{
  std::string body = "hello world";
  HttpResponse response(200, std::vector<HttpHeader> {{"Content-Type", "text/plain; charset=utf-8"}}, "hello world");
  ASSERT_EQ(response.toBuffer(),
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain; charset=utf-8\r\n" +
              getAllowCorsHeaders() + "Content-Length: " + std::to_string(body.length()) + "\r\n\r\n" + body);
}

/// @test
/// Check HTTP message serialization with CORS disabled
/// @requirements(SEN-1061)
TEST(Rest, no_cors_headers)
{
  HttpResponse response(200, {}, "", false);
  ASSERT_EQ(response.toBuffer(),
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 0\r\n\r\n");
}
