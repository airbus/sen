// === http_server.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REST_SRC_HTTP_SERVER_H
#define SEN_COMPONENTS_REST_SRC_HTTP_SERVER_H

// component
#include "base_router.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/kernel/component_api.h"

// asio
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/thread_pool.hpp>

// std
#include <chrono>
#include <memory>

namespace sen::components::rest
{

/// HttpServer class handles incoming HTTP requests and route requests.
/// This class provides functionality to start, stop the server and
/// dispatch requests to a `BaseRouter` implementation.
class HttpServer
{
  SEN_NOCOPY_NOMOVE(HttpServer)

public:
  ~HttpServer();

public:
  template <class Router>
  static std::unique_ptr<HttpServer> create(kernel::RunApi& runApi)
  {
    auto router = std::make_unique<Router>(runApi);
    return std::unique_ptr<HttpServer>(new HttpServer(std::move(router)));
  }

  /// The `start` method initializes the server, binding it to the specified address and port.
  /// It sets up the request routing mechanism using the provided `router` object
  void start(const asio::ip::tcp::endpoint& endpoint);

  /// Stops the HTTP server blocking until all workers have finished.
  void stop();

  /// Run until the given relative time is reached.
  void runUntil(const std::chrono::nanoseconds& relTime);

private:
  template <class Router>
  explicit HttpServer(std::unique_ptr<Router> router): router_(std::move(router)), acceptor_(context_)
  {
    // Left blank intentionally
  }

  void accept(std::shared_ptr<BaseRouter> router);

private:
  asio::io_context context_;
  std::shared_ptr<BaseRouter> router_;
  asio::ip::tcp::acceptor acceptor_;
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_HTTP_SERVER_H
