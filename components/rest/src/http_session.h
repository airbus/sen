// === http_session.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_HTTP_SESSION_H
#define SEN_COMPONENTS_REST_SRC_HTTP_SESSION_H

// component
#include "http_request.h"

// sen
#include "sen/core/base/compiler_macros.h"

// asio
#include <asio/ip/tcp.hpp>

// llhttp
extern "C"
{
#include "llhttp.h"
}

// asio
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/thread_pool.hpp>

// std
#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace sen::components::rest
{

constexpr size_t maxReadLength = 1500;

// Forward declarations
class BaseRouter;

/// HttpSession class represents a single HTTP connection with a client,
/// handling the parsing of incoming request and routing them to the
/// appropiate handler.
class HttpSession: public std::enable_shared_from_this<HttpSession>
{
  SEN_NOCOPY_NOMOVE(HttpSession)

public:
  HttpSession(asio::io_context& ctx, asio::ip::tcp::socket socket, std::shared_ptr<BaseRouter> router);
  ~HttpSession();

public:
  /// Asynchronously starts a HTTP session, reading and parsing HTTP client request and dispatching it to the router.
  void start();

  /// Returns HTTP request of this session.
  inline const HttpRequest& getRequest() const { return request_; }

  /// Returns the IO context of this sessions
  inline asio::io_context& getIOContext() const { return ctx_; }

private:
  static int onMethod(llhttp_t* parser, const char* path, size_t nbytes);
  static int onMessageComplete(llhttp_t* parser);
  static int onURL(llhttp_t* parser, const char* path, size_t nbytes);
  static int onBody(llhttp_t* parser, const char* data, size_t nbytes);
  static int onHeaderField(llhttp_t* parser, const char* data, size_t nbytes);
  static int onHeaderValue(llhttp_t* parser, const char* data, size_t nbytes);

  /// Take ownership of the socket
  inline asio::ip::tcp::socket takeSocket() { return std::move(socket_); }

  asio::io_context& ctx_;
  asio::ip::tcp::socket socket_;

  llhttp_t parser_;
  llhttp_settings_t settings_;
  HttpRequest request_;
  std::string currentHttpField_;
  std::shared_ptr<BaseRouter> router_;
  std::array<char, maxReadLength> data_ = {0};
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_HTTP_SESSION_H
