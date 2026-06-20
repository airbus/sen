// === dispatcher_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// local
#include "dispatcher.h"
#include "dispatcher_fixture.h"
#include "messages.h"
#include "server.h"

// nlohmann
#include "nlohmann/json.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <string>

namespace sen::components::jsonrpc::test
{

namespace
{

constexpr ConnectionId clientId {7U};

[[nodiscard]] nlohmann::json popOutboundJson(DispatcherFixture& f)
{
  auto raw = f.tryPopRaw();
  if (!raw.has_value())
  {
    throw std::runtime_error("popOutboundJson: no outbound frame buffered");
  }
  return nlohmann::json::parse(raw->payload);
}

[[nodiscard]] bool outboundIsEmpty(DispatcherFixture& f) { return f.drainNow().empty(); }

}  // namespace

/// @test
/// A well-formed ping request is answered with `result:"pong"` echoing the request id, addressed
/// at the connection that sent it.
TEST(Dispatcher, pingHappyPath)
{
  DispatcherFixture f;
  f.connect(clientId);
  f.pushFrame(clientId, R"({"jsonrpc":"2.0","method":"ping","id":1})");

  const auto envelope = f.popJsonAfterStepping();
  EXPECT_EQ(envelope["jsonrpc"], "2.0");
  EXPECT_EQ(envelope["result"], "pong");
  EXPECT_EQ(envelope["id"], 1);
}

/// @test
/// A request whose `id` field is missing is a notification: the handler runs but no response is
/// emitted on the outbound queue.
TEST(Dispatcher, notificationProducesNoResponse)
{
  DispatcherFixture f;
  f.connect(clientId);
  f.pushFrame(clientId, R"({"jsonrpc":"2.0","method":"ping"})");
  f.step();

  EXPECT_TRUE(outboundIsEmpty(f));
}

/// @test
/// Malformed JSON yields Parse error (-32700) with id null; the parser's diagnostic rides in `data`.
TEST(Dispatcher, parseErrorRespondsWithNullId)
{
  DispatcherFixture f;
  f.connect(clientId);
  f.pushFrame(clientId, "this is not json");

  const auto envelope = f.popJsonAfterStepping();
  const auto& err = envelope["error"];
  EXPECT_EQ(err["code"], static_cast<int>(JsonRpcErrorCode::parseError));
  EXPECT_EQ(err["message"].get<std::string>(), "Parse error");
  ASSERT_TRUE(err.contains("data")) << envelope.dump();
  EXPECT_TRUE(err["data"].is_string()) << envelope.dump();
  EXPECT_FALSE(err["data"].get<std::string>().empty()) << envelope.dump();
  EXPECT_TRUE(envelope["id"].is_null());
}

/// @test
/// A non-object envelope (a bare value or an array, including the batch case we deferred) is
/// rejected with Invalid Request (-32600) and id null.
TEST(Dispatcher, nonObjectEnvelopeIsInvalidRequest)
{
  DispatcherFixture f;
  f.connect(clientId);
  f.pushFrame(clientId, "42");

  const auto envelope = f.popJsonAfterStepping();
  EXPECT_EQ(envelope["error"]["code"], static_cast<int>(JsonRpcErrorCode::invalidRequest));
  EXPECT_TRUE(envelope["id"].is_null());
}

/// @test
/// Batch (array) envelopes fold into the generic non-object rejection - batch support is deferred.
TEST(Dispatcher, batchEnvelopeIsInvalidRequest)
{
  DispatcherFixture f;
  f.connect(clientId);
  f.pushFrame(clientId, R"([{"jsonrpc":"2.0","method":"ping","id":1}])");

  const auto envelope = f.popJsonAfterStepping();
  EXPECT_EQ(envelope["error"]["code"], static_cast<int>(JsonRpcErrorCode::invalidRequest));
}

/// @test
/// An unknown method name produces Method not found (-32601), echoing the request id.
TEST(Dispatcher, methodNotFound)
{
  DispatcherFixture f;
  f.connect(clientId);
  f.pushFrame(clientId, R"({"jsonrpc":"2.0","method":"doesNotExist","id":42})");

  const auto envelope = f.popJsonAfterStepping();
  EXPECT_EQ(envelope["error"]["code"], static_cast<int>(JsonRpcErrorCode::methodNotFound));
  EXPECT_EQ(envelope["id"], 42);
}

/// @test
/// Wrong jsonrpc version field is Invalid Request (-32600) and echoes the request's id.
TEST(Dispatcher, wrongVersionIsInvalidRequest)
{
  DispatcherFixture f;
  f.connect(clientId);
  f.pushFrame(clientId, R"({"jsonrpc":"1.0","method":"ping","id":3})");

  const auto envelope = f.popJsonAfterStepping();
  EXPECT_EQ(envelope["error"]["code"], static_cast<int>(JsonRpcErrorCode::invalidRequest));
  EXPECT_EQ(envelope["id"], 3);
}

/// @test
/// `unsubscribeEvent` shares the param shape `{interest: string, object, event}`. Pins the
/// handler's param-shape gate.
TEST(Dispatcher, unsubscribeEventRejectsBadParams)
{
  DispatcherFixture f;
  f.connect(clientId);
  f.pushFrame(clientId, R"({"jsonrpc":"2.0","method":"unsubscribeEvent","params":{"interestName":"x"},"id":3})");

  const auto envelope = f.popJsonAfterStepping();
  EXPECT_EQ(envelope["error"]["code"], static_cast<int>(JsonRpcErrorCode::invalidParams));
  EXPECT_EQ(envelope["id"], 3);
}

/// @test
/// `unsubscribeProperty` shares the param shape `{interest: string, object, property}`. Pins the
/// handler's param-shape gate.
TEST(Dispatcher, unsubscribePropertyRejectsBadParams)
{
  DispatcherFixture f;
  f.connect(clientId);
  f.pushFrame(clientId, R"({"jsonrpc":"2.0","method":"unsubscribeProperty","params":{"interestName":"x"},"id":2})");

  const auto envelope = f.popJsonAfterStepping();
  EXPECT_EQ(envelope["error"]["code"], static_cast<int>(JsonRpcErrorCode::invalidParams));
  EXPECT_EQ(envelope["id"], 2);
}

/// @test
/// `releaseInterest` with missing or non-string `interest` returns invalidParams. The error fires
/// before any kernel/state lookup.
TEST(Dispatcher, releaseInterestRejectsBadParams)
{
  DispatcherFixture f;
  f.connect(clientId);
  f.pushFrame(clientId, R"({"jsonrpc":"2.0","method":"releaseInterest","params":{},"id":1})");

  const auto envelope = f.popJsonAfterStepping();
  EXPECT_EQ(envelope["error"]["code"], static_cast<int>(JsonRpcErrorCode::invalidParams));
  EXPECT_EQ(envelope["id"], 1);
}

/// @test
/// ClientDisconnected and BackpressureUpdate synthetic messages are absorbed silently: they must
/// not produce protocol traffic, even though the dispatcher tracks per-connection state from them.
TEST(Dispatcher, syntheticMessagesAreAbsorbed)
{
  DispatcherFixture f;
  f.disconnect(clientId);
  f.pushBackpressure(clientId, 1024U);
  f.step();

  EXPECT_TRUE(outboundIsEmpty(f));
}

/// @test
/// `ClientConnected{identity}` is silent on the wire and seeds the per-connection `Server`'s
/// identity (the WS-server tests cover the emission half of the auth seam).
TEST(Dispatcher, clientConnectedSeedsServerIdentity)
{
  DispatcherFixture f;
  f.connect(clientId, "alice");
  f.step();

  EXPECT_TRUE(outboundIsEmpty(f)) << "ClientConnected must be silent on the wire";
  auto* server = f.dispatcher().getServer(clientId);
  ASSERT_NE(server, nullptr);
  EXPECT_EQ(server->getIdentity().subject, "alice");
}

/// @test
/// During the high window, best-effort `pushNotification` drops the message and increments
/// `droppedNotifications`.
TEST(Dispatcher, pushNotificationIsDroppedWhileBackpressureHigh)
{
  DispatcherFixture f;
  f.pushBackpressure(clientId, 32U * 1024U);
  f.step();
  EXPECT_TRUE(f.dispatcher().getOrCreateConnectionState(clientId).backpressureHigh);

  // Two notifications during the high window - both should be dropped.
  f.dispatcher().pushNotification(clientId, "anything", nlohmann::json::object());
  f.dispatcher().pushNotification(clientId, "stillNothing", nlohmann::json::object());

  EXPECT_TRUE(outboundIsEmpty(f)) << "no notifications should escape during high";
  EXPECT_EQ(f.dispatcher().getOrCreateConnectionState(clientId).droppedNotifications, 2U);
}

/// @test
/// On the drained transition, the dispatcher emits one `notificationsDropped {count: N}` and
/// resets the counter. The recovery message itself must not be gated.
TEST(Dispatcher, backpressureRecoveryEmitsNotificationsDropped)
{
  DispatcherFixture f;
  f.pushBackpressure(clientId, 32U * 1024U);
  f.step();
  f.dispatcher().pushNotification(clientId, "drop1", nlohmann::json::object());
  f.dispatcher().pushNotification(clientId, "drop2", nlohmann::json::object());
  f.dispatcher().pushNotification(clientId, "drop3", nlohmann::json::object());
  EXPECT_EQ(f.dispatcher().getOrCreateConnectionState(clientId).droppedNotifications, 3U);

  f.pushBackpressure(clientId, 0U);
  f.step();
  EXPECT_FALSE(f.dispatcher().getOrCreateConnectionState(clientId).backpressureHigh);
  EXPECT_EQ(f.dispatcher().getOrCreateConnectionState(clientId).droppedNotifications, 0U);

  const auto envelope = popOutboundJson(f);
  EXPECT_EQ(envelope["method"], "notificationsDropped");
  EXPECT_EQ(envelope["params"]["count"], 3);
}

/// @test
/// No recovery message when nothing was dropped - the message exists only to signal lost data.
TEST(Dispatcher, backpressureRecoveryIsSilentWhenNothingWasDropped)
{
  DispatcherFixture f;
  f.pushBackpressure(clientId, 32U * 1024U);
  f.pushBackpressure(clientId, 0U);
  f.step();

  EXPECT_TRUE(outboundIsEmpty(f)) << "no recovery notification when nothing was dropped";
}

/// @test
/// Reliable notifications bypass the backpressure gate. The drop counter is not touched by the
/// bypass.
TEST(Dispatcher, reliableNotificationBypassesBackpressureGate)
{
  DispatcherFixture f;
  f.pushBackpressure(clientId, 32U * 1024U);
  f.step();

  f.dispatcher().pushNotification(clientId, "structural", nlohmann::json::object(), /*reliable=*/true);
  f.dispatcher().pushNotification(clientId, "alsoStructural", nlohmann::json::object(), /*reliable=*/true);
  f.dispatcher().pushNotification(clientId, "bestEffort", nlohmann::json::object());

  // Two reliable notifications escape; one best-effort is dropped and counted.
  EXPECT_EQ(popOutboundJson(f)["method"], "structural");
  EXPECT_EQ(popOutboundJson(f)["method"], "alsoStructural");
  EXPECT_TRUE(outboundIsEmpty(f)) << "best-effort must not escape";
  EXPECT_EQ(f.dispatcher().getOrCreateConnectionState(clientId).droppedNotifications, 1U);
}

/// @test
/// Direct-request responses bypass the backpressure gate; clients who asked for a result must
/// always get one.
TEST(Dispatcher, requestResponseBypassesBackpressureGate)
{
  DispatcherFixture f;
  f.connect(clientId);
  f.pushBackpressure(clientId, 32U * 1024U);
  f.pushFrame(clientId, R"({"jsonrpc":"2.0","method":"ping","id":99})");

  const auto envelope = f.popJsonAfterStepping();
  EXPECT_EQ(envelope["result"], "pong");
  EXPECT_EQ(envelope["id"], 99);
}

}  // namespace sen::components::jsonrpc::test
