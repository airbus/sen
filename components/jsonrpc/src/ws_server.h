// === ws_server.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_JSONRPC_WS_SERVER_H
#define SEN_COMPONENTS_JSONRPC_WS_SERVER_H

// local
#include "auth.h"
#include "messages.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/result.h"

// generated code
#include "stl/configuration.stl.h"

// uwebsockets
#include <uWebSockets/App.h>

// std
#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <thread>

namespace sen::components::jsonrpc
{

/// HTTP GET handler invoked on the uWS loop thread. Receives raw uWS types so handlers can
/// stream responses, set headers, etc. without going through an abstraction layer.
using HttpGetHandler = std::function<void(uWS::HttpResponse<false>* res, uWS::HttpRequest* req)>;

/// uWebSockets server running on its own thread, constructed through `make()`.
class WebSocketServer
{
  SEN_NOCOPY_NOMOVE(WebSocketServer)
  SEN_PRIVATE_TAG

public:
  WebSocketServer(std::string address,
                  uint16_t port,
                  InboundQueue& inboundQueue,
                  OutboundQueue& outboundQueue,
                  const Authenticator& authenticator,
                  ConnectionLimits limits,
                  Private notUsable);

  ~WebSocketServer();

public:
  /// Starts a server bound to `address:port`.
  [[nodiscard]] static sen::Result<std::unique_ptr<WebSocketServer>, std::string> make(
    std::string address,
    uint16_t port,
    InboundQueue& inboundQueue,
    OutboundQueue& outboundQueue,
    const Authenticator& authenticator,
    ConnectionLimits limits = {});

  /// Wakes the uWS loop to drain the outbound queue.
  void notifyOutbound();

  /// Adds a `GET` route. Thread-safe; the registration is deferred to the uWS loop, so the route
  /// becomes effective on the next loop iteration.
  void registerHttpGet(std::string pattern, HttpGetHandler handler);

  /// Bound port.
  [[nodiscard]] uint16_t getPort() const noexcept;

private:
  [[nodiscard]] sen::Result<void, std::string> start();
  void runUwsLoop(std::promise<bool> readyPromise);

private:
  std::string address_;
  uint16_t requestedPort_ {0};
  InboundQueue& inboundQueue_;
  OutboundQueue& outboundQueue_;
  const Authenticator& authenticator_;
  ConnectionLimits limits_ {};
  uint16_t boundPort_ {0};
  std::atomic<uWS::Loop*> loop_ {nullptr};
  std::function<void()> shutdownAction_;
  std::function<void()> drainAction_;
  std::function<void(std::string, HttpGetHandler)> httpGetRegistrar_;
  std::thread thread_;
};

}  // namespace sen::components::jsonrpc

#endif  // SEN_COMPONENTS_JSONRPC_WS_SERVER_H
