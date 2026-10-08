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
/// Pushes a received text frame onto the inbound queue tagged with a non-zero connection id
/// and the unmodified payload.
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
/// Closes the connection when a client sends a binary frame, producing a ClientDisconnected on
/// the inbound queue with no text payload ever reaching the dispatcher.
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
/// Pushes a ClientDisconnected onto the inbound queue when the client closes, carrying the
/// same connection id the client's earlier traffic was tagged with.
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
/// Delivers a message enqueued on the outbound queue to the client addressed by its connection
/// id once notifyOutbound is called.
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
/// Drops an outbound message addressed to a connection id that does not exist, surviving the
/// notify without a crash.
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
/// Returns promptly from destruction even while a client is still connected.
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
/// Refuses to construct when the connection limits set highBackpressureBytes at or above
/// maxBackpressureBytes, naming the offending field in the error, since the soft trigger must
/// fire before the hard ceiling.
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
/// Reports an error naming the failed bind when the requested port is already in use.
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
/// Emits a ClientConnected carrying the anonymous identity and a non-zero connection id on an
/// upgrade accepted under the NoAuth authenticator.
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
/// Blocks the upgrade when the authenticator denies the connection, so nothing from that
/// client ever lands on the inbound queue.
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
