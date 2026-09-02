// === ws_client.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_JSONRPC_TEST_WS_CLIENT_H
#define SEN_COMPONENTS_JSONRPC_TEST_WS_CLIENT_H

// sen
#include "sen/core/base/compiler_macros.h"

// asio
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>

// std
#include <cstdint>
#include <string>
#include <string_view>

namespace sen::components::jsonrpc::test
{

/// Minimal RFC 6455 WebSocket client built directly on standalone asio.
///
/// Only what the test harness needs: connect, send a single text frame, receive a single text frame, close.
/// Payloads must fit in a single unfragmented frame and be < 126 bytes (no extended-length encoding).
/// Not for production use.
class WsClient
{
  SEN_NOCOPY_NOMOVE(WsClient)

public:
  WsClient(std::string host, uint16_t port);
  ~WsClient() = default;

  /// Performs the HTTP/1.1 Upgrade handshake. Throws on transport / handshake failure.
  void connect(std::string_view path = "/");

  /// Sends a single masked text frame.
  void sendText(std::string_view payload);

  /// Sends a single masked binary frame. Used to exercise the server's "binary not supported"
  /// path, which must close the connection rather than silently drop.
  void sendBinary(std::string_view payload);

  /// Receives a single text frame. Returns its unmasked payload.
  [[nodiscard]] std::string receiveText();

  /// Sends a close frame and closes the underlying socket.
  void close();

private:
  asio::io_context ctx_;
  asio::ip::tcp::socket socket_;
  std::string host_;
  uint16_t port_;
};

}  // namespace sen::components::jsonrpc::test

#endif  // SEN_COMPONENTS_JSONRPC_TEST_WS_CLIENT_H
