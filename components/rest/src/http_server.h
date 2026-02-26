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

// asio
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/thread_pool.hpp>

// std
#include <cstdint>
#include <functional>
#include <memory>
#include <thread>

namespace sen::components::rest
{

/// HttpServer class handles incoming HTTP requests and route requests.
/// This class provides functionality to start, stop the server and
/// dispatch requests to a `BaseRouter` implementation.
class HttpServer
{
public:
  SEN_NOCOPY_NOMOVE(HttpServer)

public:
  HttpServer();
  ~HttpServer();

public:
  /// The `start` method initializes the server, binding it to the specified address and port.
  /// It sets up the request routing mechanism using the provided `router` object
  /// Optional `callback` can be provided to be executed once the server has successfully started.
  void start(std::unique_ptr<BaseRouter> router,
             const asio::ip::tcp::endpoint& endpoint,
             uint16_t threadPoolSize,
             std::function<void()> callback);

  /// Stops the HTTP server blocking until all workers have finished.
  void stop();

private:
  void accept(std::shared_ptr<BaseRouter> router);

private:
  asio::io_context context_;
  std::shared_ptr<asio::thread_pool> threadPool_;
  asio::ip::tcp::acceptor acceptor_;
  std::thread executor_;
};

}  // namespace sen::components::rest

#endif  // SEN_COMPONENTS_REST_SRC_HTTP_SERVER_H
