// === ws_server_test.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// local
#include "auth.h"
#include "messages.h"
#include "ws_client.h"
#include "ws_server.h"

// sen
#include "sen/core/base/result.h"

// generated
#include "stl/configuration.stl.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <string>
#include <thread>
#include <utility>
#include <variant>

namespace sen::components::jsonrpc::test
{

namespace
{

constexpr auto pollInterval = std::chrono::milliseconds(10);
constexpr auto pollTimeout = std::chrono::seconds(2);

/// Default authenticator used by every test that doesn't substitute a stricter one.
const NoAuth defaultAuthenticator {};

bool waitForMessage(InboundQueue& queue, InboundMessage& out)
{
  const auto deadline = std::chrono::steady_clock::now() + pollTimeout;
  while (std::chrono::steady_clock::now() < deadline)
  {
    if (queue.try_dequeue(out))
    {
      return true;
    }
    std::this_thread::sleep_for(pollInterval);
  }
  return false;
}

/// Drains until a non-`ClientConnected` message appears, so legacy text-frame / disconnect
/// assertions can skip past the upgrade-time identity message.
bool waitForNonConnectMessage(InboundQueue& queue, InboundMessage& out)
{
  while (waitForMessage(queue, out))
  {
    if (!std::holds_alternative<ClientConnected>(out.payload))
    {
      return true;
    }
  }
  return false;
}

}  // namespace

/// @test
/// A text frame received by the server lands on the inbound queue, tagged with a non-zero
/// connection id and the unmodified payload.
TEST(WebSocketServer, pushesInboundOnTextFrame)
{
  InboundQueue inboundQueue;
  OutboundQueue outboundQueue;
  auto result = WebSocketServer::make("127.0.0.1", 0, inboundQueue, outboundQueue, defaultAuthenticator);
  ASSERT_TRUE(result);
  auto server = std::move(result).getValue();

  WsClient client("127.0.0.1", server->getPort());
  client.connect();
  client.sendText("hello");

  InboundMessage msg;
  ASSERT_TRUE(waitForNonConnectMessage(inboundQueue, msg));
  ASSERT_TRUE(std::holds_alternative<std::string>(msg.payload));
  EXPECT_EQ(std::get<std::string>(msg.payload), "hello");
  EXPECT_NE(msg.connectionId, 0U);

  client.close();
}

/// @test
/// Sending a binary frame closes the connection (1003 Unsupported Data) and produces a
/// `ClientDisconnected` on the inbound queue. No `std::string` payload reaches the dispatcher.
TEST(WebSocketServer, binaryFrameClosesConnection)
{
  InboundQueue inboundQueue;
  OutboundQueue outboundQueue;
  auto result = WebSocketServer::make("127.0.0.1", 0, inboundQueue, outboundQueue, defaultAuthenticator);
  ASSERT_TRUE(result);
  auto server = std::move(result).getValue();

  WsClient client("127.0.0.1", server->getPort());
  client.connect();
  client.sendBinary("not allowed");

  // Drain past ClientConnected and assert the next non-connect message is ClientDisconnected.
  // No `std::string` (text) payload should appear.
  InboundMessage msg;
  ASSERT_TRUE(waitForNonConnectMessage(inboundQueue, msg));
  EXPECT_TRUE(std::holds_alternative<ClientDisconnected>(msg.payload)) << "variant index " << msg.payload.index();
}

/// @test
/// Closing a connection produces a ClientDisconnected on the inbound queue with the same
/// connection id that was tagged on the open.
TEST(WebSocketServer, pushesClientDisconnectedOnClose)
{
  InboundQueue inboundQueue;
  OutboundQueue outboundQueue;
  auto result = WebSocketServer::make("127.0.0.1", 0, inboundQueue, outboundQueue, defaultAuthenticator);
  ASSERT_TRUE(result);
  auto server = std::move(result).getValue();

  WsClient client("127.0.0.1", server->getPort());
  client.connect();
  client.sendText("ping");

  InboundMessage first;
  ASSERT_TRUE(waitForNonConnectMessage(inboundQueue, first));
  const auto id = first.connectionId;

  client.close();

  InboundMessage second;
  ASSERT_TRUE(waitForMessage(inboundQueue, second));
  EXPECT_EQ(second.connectionId, id);
  EXPECT_TRUE(std::holds_alternative<ClientDisconnected>(second.payload));
}

/// @test
/// A message pushed to the outbound queue is delivered to the right client after notifyOutbound().
TEST(WebSocketServer, deliversOutboundOnNotify)
{
  InboundQueue inboundQueue;
  OutboundQueue outboundQueue;
  auto result = WebSocketServer::make("127.0.0.1", 0, inboundQueue, outboundQueue, defaultAuthenticator);
  ASSERT_TRUE(result);
  auto server = std::move(result).getValue();

  WsClient client("127.0.0.1", server->getPort());
  client.connect();
  client.sendText("hi");

  // Learn the connection id from the inbound side.
  InboundMessage inbound;
  ASSERT_TRUE(waitForMessage(inboundQueue, inbound));
  const auto id = inbound.connectionId;

  outboundQueue.enqueue(OutboundMessage {id, "from-server"});
  server->notifyOutbound();

  EXPECT_EQ(client.receiveText(), "from-server");
  client.close();
}

/// @test
/// Outbound messages addressed to a connection that has already gone away are dropped silently.
TEST(WebSocketServer, dropsOutboundForUnknownConnection)
{
  InboundQueue inboundQueue;
  OutboundQueue outboundQueue;
  auto result = WebSocketServer::make("127.0.0.1", 0, inboundQueue, outboundQueue, defaultAuthenticator);
  ASSERT_TRUE(result);
  auto server = std::move(result).getValue();

  outboundQueue.enqueue(OutboundMessage {99999U, "stray"});
  server->notifyOutbound();
  // Nothing observable on the client side; the test passes if the server does not crash.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

/// @test
/// Destruction returns promptly even when a client is still connected.
TEST(WebSocketServer, destroyClosesLiveConnections)
{
  InboundQueue inboundQueue;
  OutboundQueue outboundQueue;
  auto result = WebSocketServer::make("127.0.0.1", 0, inboundQueue, outboundQueue, defaultAuthenticator);
  ASSERT_TRUE(result);
  auto server = std::move(result).getValue();

  WsClient client("127.0.0.1", server->getPort());
  client.connect();

  server.reset();
}

/// @test
/// `make()` rejects a `ConnectionLimits` where `highBackpressureBytes >= maxBackpressureBytes`:
/// the soft trigger must fire before the hard ceiling or the application-level signal is dead.
TEST(WebSocketServer, makeRejectsBackpressureLimitsInversion)
{
  InboundQueue inboundQueue;
  OutboundQueue outboundQueue;
  ConnectionLimits limits {};
  limits.maxBackpressureBytes.emplace(32U * 1024U);
  limits.highBackpressureBytes.emplace(32U * 1024U);  // equal is still bad
  auto result = WebSocketServer::make("127.0.0.1", 0, inboundQueue, outboundQueue, defaultAuthenticator, limits);
  ASSERT_FALSE(result);
  EXPECT_NE(result.getError().find("highBackpressureBytes"), std::string::npos);
}

/// @test
/// Binding a port that is already in use returns an Err describing the failure.
TEST(WebSocketServer, makeReportsBindFailure)
{
  InboundQueue inboundQueue;
  OutboundQueue outboundQueue;
  auto firstResult = WebSocketServer::make("127.0.0.1", 0, inboundQueue, outboundQueue, defaultAuthenticator);
  ASSERT_TRUE(firstResult);
  auto first = std::move(firstResult).getValue();

  auto secondResult =
    WebSocketServer::make("127.0.0.1", first->getPort(), inboundQueue, outboundQueue, defaultAuthenticator);
  ASSERT_FALSE(secondResult);
  EXPECT_NE(secondResult.getError().find("failed to bind"), std::string::npos);
}

/// @test
/// `NoAuth` produces `ClientConnected{identity:"anonymous"}` on every accepted upgrade. The
/// upgrade callback runs the authenticator and the resulting Identity rides the inbound queue.
TEST(WebSocketServer, emitsClientConnectedWithAnonymousIdentityUnderNoAuth)
{
  InboundQueue inboundQueue;
  OutboundQueue outboundQueue;
  auto result = WebSocketServer::make("127.0.0.1", 0, inboundQueue, outboundQueue, defaultAuthenticator);
  ASSERT_TRUE(result);
  auto server = std::move(result).getValue();

  WsClient client("127.0.0.1", server->getPort());
  client.connect();

  InboundMessage msg;
  ASSERT_TRUE(waitForMessage(inboundQueue, msg));
  ASSERT_TRUE(std::holds_alternative<ClientConnected>(msg.payload));
  EXPECT_EQ(std::get<ClientConnected>(msg.payload).identity.subject, "anonymous");
  EXPECT_NE(msg.connectionId, 0U);

  client.close();
}

namespace
{

/// Authenticator that rejects every connection.
class DenyAllAuthenticator final: public Authenticator
{
public:
  [[nodiscard]] sen::Result<Identity, std::string> verify(std::string_view /*authorizationHeader*/) const override
  {
    return sen::Err(std::string {"denied"});
  }
};

}  // namespace

/// @test
/// A denying authenticator blocks the upgrade: nothing lands on the inbound queue. uWS responds
/// 401 without promoting the connection to a WebSocket.
TEST(WebSocketServer, rejectingAuthenticatorBlocksUpgrade)
{
  InboundQueue inboundQueue;
  OutboundQueue outboundQueue;
  DenyAllAuthenticator deny;
  auto result = WebSocketServer::make("127.0.0.1", 0, inboundQueue, outboundQueue, deny);
  ASSERT_TRUE(result);
  auto server = std::move(result).getValue();

  WsClient client("127.0.0.1", server->getPort());
  // Best-effort: WsClient doesn't surface the HTTP-level outcome; the contract is "nothing on
  // inbound", asserted below.
  try
  {
    client.connect();
    client.sendText("nope");
  }
  catch (...)
  {
  }

  InboundMessage msg;
  EXPECT_FALSE(waitForMessage(inboundQueue, msg))
    << "expected nothing on inbound; got variant index " << msg.payload.index();
}

}  // namespace sen::components::jsonrpc::test
