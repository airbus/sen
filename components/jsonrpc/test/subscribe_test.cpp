// === subscribe_test.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Subscribe / unsubscribe behavior plus the streaming notifications. Bare interest CRUD lives
// in lifecycle_test.cpp; request/response RPCs in rpc_methods_test.cpp.

// local
#include "dispatcher_fixture.h"
#include "frame_helpers.h"
#include "messages.h"

// nlohmann
#include "nlohmann/json.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <string>
#include <tuple>
#include <utility>

using sen::components::jsonrpc::ConnectionId;
using sen::components::jsonrpc::test::awaitFirstAddedObjectName;
using sen::components::jsonrpc::test::awaitNotification;
using sen::components::jsonrpc::test::awaitPropertyValue;
using sen::components::jsonrpc::test::decodeEventArgs;
using sen::components::jsonrpc::test::DispatcherFixture;
using sen::components::jsonrpc::test::findDecodedPropertyValue;
using sen::components::jsonrpc::test::namedSelector;
using sen::components::jsonrpc::test::noFurtherFrames;
using sen::components::jsonrpc::test::popResponseFor;
using sen::components::jsonrpc::test::primeFixtureInterest;
using sen::components::jsonrpc::test::request;
using sen::components::jsonrpc::test::wildcardSelector;

/// @test
/// `subscribeProperty` against an unknown interest name returns unknownInterest. The lookup
/// chain stops at the very first step.
TEST(JsonRpc, subscribePropertyUnknownInterestFails)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  f.pushFrame(
    connId,
    request("subscribeProperty", 10, {{"interestName", "nope"}, {"objectName", "api"}, {"propertyName", "buildInfo"}}));

  const auto resp = f.popJsonAfterStepping();
  EXPECT_EQ(resp["id"], 10);
  ASSERT_TRUE(resp.contains("error"));
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::unknownInterest));
}

/// @test
/// Subscribing against an object not in the match set succeeds and is recorded for sticky rewire
/// when an object with that name later joins.
TEST(JsonRpc, subscribePropertyOnAbsentObjectIsDeferred)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  f.pushFrame(
    connId,
    request(
      "subscribeProperty", 11, {{"interestName", "i1"}, {"objectName", "noSuchObject"}, {"propertyName", "counter"}}));

  const auto resp = popResponseFor(f, 11);
  ASSERT_TRUE(resp.contains("result")) << resp.dump();
  EXPECT_TRUE(resp["result"].is_null());
}

/// @test
/// `subscribeProperty` end-to-end: create interest, subscribe to a known property on a known
/// object, unsubscribe. Exercises the full lookup chain plus the kernel-side callback
/// registration. Idempotent in both directions.
TEST(JsonRpc, subscribePropertyOnKnownPropertyRoundTrips)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  const nlohmann::json subParams {
    {"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}, {"propertyName", "counter"}};

  f.pushFrame(connId, request("subscribeProperty", 20, subParams));
  const auto subResp = popResponseFor(f, 20);
  ASSERT_TRUE(subResp.contains("result")) << subResp.dump();
  EXPECT_TRUE(subResp["result"].is_null());

  // Idempotent: a second subscribe for the same triple is a no-op success.
  f.pushFrame(connId, request("subscribeProperty", 21, subParams));
  const auto subResp2 = popResponseFor(f, 21);
  ASSERT_TRUE(subResp2.contains("result"));
  EXPECT_TRUE(subResp2["result"].is_null());

  const auto unsubFrame = request("unsubscribeProperty", 22, subParams);
  f.pushFrame(connId, unsubFrame);
  const auto unsubResp = popResponseFor(f, 22);
  ASSERT_TRUE(unsubResp.contains("result"));
  EXPECT_TRUE(unsubResp["result"].is_null());

  // Unsubscribe again is also a no-op success (idempotent).
  f.pushFrame(connId, unsubFrame);
  const auto unsubResp2 = popResponseFor(f, 22);
  ASSERT_TRUE(unsubResp2.contains("result"));
  EXPECT_TRUE(unsubResp2["result"].is_null());
}

/// @test
/// `subscribeProperty` against a real (interest, object) but an unknown property name returns
/// unknownMember. Exercises the property-lookup leg of the chain.
TEST(JsonRpc, subscribePropertyUnknownPropertyFails)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  f.pushFrame(
    connId,
    request("subscribeProperty",
            30,
            {{"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}, {"propertyName", "doesNotExist"}}));

  const auto resp = popResponseFor(f, 30);
  ASSERT_TRUE(resp.contains("error"));
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::unknownMember));
}

/// @test
/// `subscribeEvent` against an unknown interest name returns unknownInterest.
TEST(JsonRpc, subscribeEventUnknownInterestFails)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  f.pushFrame(connId,
              request("subscribeEvent", 40, {{"interestName", "nope"}, {"objectName", "api"}, {"eventName", "ev"}}));

  const auto resp = f.popJsonAfterStepping();
  EXPECT_EQ(resp["id"], 40);
  ASSERT_TRUE(resp.contains("error"));
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::unknownInterest));
}

/// @test
/// Same as the property variant: subscribing to an event on an absent object is recorded as a
/// deferred request rather than rejected. Confirms the sticky semantic applies symmetrically.
TEST(JsonRpc, subscribeEventOnAbsentObjectIsDeferred)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  f.pushFrame(
    connId,
    request("subscribeEvent", 41, {{"interestName", "i1"}, {"objectName", "noSuchObject"}, {"eventName", "ev"}}));

  const auto resp = popResponseFor(f, 41);
  ASSERT_TRUE(resp.contains("result")) << resp.dump();
  EXPECT_TRUE(resp["result"].is_null());
}

/// @test
/// `subscribeEvent` against a real (interest, object) but an unknown event name returns
/// unknownMember. Exercises the event-lookup leg.
TEST(JsonRpc, subscribeEventUnknownEventFails)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  f.pushFrame(
    connId,
    request("subscribeEvent",
            42,
            {{"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}, {"eventName", "doesNotExist"}}));

  const auto resp = popResponseFor(f, 42);
  ASSERT_TRUE(resp.contains("error"));
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::unknownMember));
}

/// @test
/// `unsubscribeEvent` is a silent no-op even when the interest is unknown, mirroring
/// `unsubscribeProperty`.
TEST(JsonRpc, unsubscribeEventOnUnknownIsNoOp)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  f.pushFrame(connId,
              request("unsubscribeEvent", 43, {{"interestName", "nope"}, {"objectName", "api"}, {"eventName", "ev"}}));

  const auto resp = f.popJsonAfterStepping();
  EXPECT_EQ(resp["id"], 43);
  ASSERT_TRUE(resp.contains("result"));
  EXPECT_TRUE(resp["result"].is_null());
}

/// @test
/// `subscribeAll` wires every property and every event on the named object in one call.
TEST(JsonRpc, subscribeAllWiresAllPropertiesAndEvents)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  f.pushFrame(connId,
              request("subscribeAll", 700, {{"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}}));
  ASSERT_TRUE(popResponseFor(f, 700).contains("result"));

  // Confirm the property side wired: counter+label both arrive in the snapshot bundle, then a
  // mutation produces a delta carrying the new counter value.
  std::ignore = awaitPropertyValue(f, "counter", 0);
  f.setWidgetCounter(91);
  std::ignore = awaitPropertyValue(f, "counter", 91);

  // Confirm the event side wired: chimed fires and the payload arrives.
  f.fireWidgetChimed("ding");
  const auto note = awaitNotification(f, "eventTriggered");
  EXPECT_EQ(note["params"]["eventName"], "chimed");
  EXPECT_EQ(decodeEventArgs(note)[0], "ding");
}

/// @test
/// `subscribeAll`'s `maxRateHz` flows through `applySubscribeBlockToObjectSubs` and coalesces
/// rapid mutations the same way `subscribeProperty` does.
TEST(JsonRpc, subscribeAllHonorsMaxRateHz)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  f.pushFrame(
    connId,
    request(
      "subscribeAll",
      710,
      {{"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}, {"maxRateHz", 4}}));  // 250ms window
  ASSERT_TRUE(popResponseFor(f, 710).contains("result"));

  for (int i = 1; i <= 5; ++i)
  {
    // setWidgetCounter is sync: each call runs in its own kernel tick (push + step) so
    // setNextCounter doesn't coalesce same-tick writes the way it would if we batched them.
    f.setWidgetCounter(i);
  }

  // Throttle is on virtual time, so awaitPropertyValue's step loop will eventually advance
  // past the 250ms window and trigger the post-window flush.
  std::ignore = awaitPropertyValue(f, "counter", 5);
  EXPECT_TRUE(noFurtherFrames(f, 32));
}

/// @test
/// `unsubscribeAll` drops every guard in one call and is idempotent.
TEST(JsonRpc, unsubscribeAllDropsBothPropertiesAndEvents)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  const nlohmann::json target {{"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}};

  f.pushFrame(connId, request("subscribeAll", 720, target));
  ASSERT_TRUE(popResponseFor(f, 720).contains("result"));
  std::ignore = awaitPropertyValue(f, "counter", 0);  // drain snapshot

  const auto unsubFrame = request("unsubscribeAll", 721, target);
  f.pushFrame(connId, unsubFrame);
  ASSERT_TRUE(popResponseFor(f, 721).contains("result"));

  // Mutating + firing should now be silent - every notification path was dropped.
  f.setWidgetCounter(7);
  f.fireWidgetChimed("hush");
  EXPECT_TRUE(noFurtherFrames(f, 32));

  // Second unsubscribe is a no-op success - exercises the idempotency contract.
  f.pushFrame(connId, unsubFrame);
  const auto resp2 = popResponseFor(f, 721);
  EXPECT_TRUE(resp2.contains("result"));
}

/// @test
/// `subscribeAll` needs the class to expand the wildcard, so an absent object is
/// objectNotInInterest (vs. `subscribeProperty`, which defers).
TEST(JsonRpc, subscribeAllOnAbsentObjectIsObjectNotInInterest)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");
  f.removeWidget();

  f.pushFrame(connId,
              request("subscribeAll", 730, {{"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}}));
  const auto resp = popResponseFor(f, 730);
  ASSERT_TRUE(resp.contains("error")) << resp.dump();
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::objectNotInInterest));
}

/// @test
/// @test
/// Interest names with JSON-significant characters (quote, backslash, control) must
/// round-trip through `propertyChanged`'s hand-built envelope.
TEST(JsonRpc, propertyChangedEscapesInterestNameSpecialChars)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  const std::string trickyName = R"(name with "quote", backslash \ and newline ()" + std::string("\n") + ")";
  primeFixtureInterest(f, connId, trickyName);

  f.pushFrame(
    connId,
    request(
      "subscribeProperty",
      900,
      {{"interestName", trickyName}, {"objectName", DispatcherFixture::widgetName}, {"propertyName", "counter"}}));
  const auto subResp = popResponseFor(f, 900);
  ASSERT_TRUE(subResp.contains("result")) << subResp.dump();

  f.setWidgetCounter(7);

  const auto note = awaitPropertyValue(f, "counter", 7);
  EXPECT_EQ(note["params"]["interestName"], trickyName);
}

/// @test
/// End-to-end `propertyChanged` delivery, with the (interest, object, property) tuple intact.
TEST(JsonRpc, propertyChangedNotificationRoundTrips)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  f.pushFrame(
    connId,
    request("subscribeProperty",
            100,
            {{"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}, {"propertyName", "counter"}}));
  const auto subResp = popResponseFor(f, 100);
  ASSERT_TRUE(subResp.contains("result")) << subResp.dump();

  f.setWidgetCounter(42);

  // Two propertyChanged frames arrive for this subscription: the initial snapshot (counter=0)
  // and the delta (counter=42). The test cares about the delta, so drain until the value lands.
  const auto note = awaitPropertyValue(f, "counter", 42);
  EXPECT_EQ(note["params"]["interestName"], "i1");
  EXPECT_EQ(note["params"]["objectName"], DispatcherFixture::widgetName);
}

/// @test
/// End-to-end eventTriggered delivery: subscribe to the widget's `chimed` event, fire it from
/// the test thread, confirm the notification carries the string payload through `args`.
TEST(JsonRpc, eventTriggeredNotificationRoundTrips)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  f.pushFrame(
    connId,
    request("subscribeEvent",
            110,
            {{"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}, {"eventName", "chimed"}}));
  const auto subResp = popResponseFor(f, 110);
  ASSERT_TRUE(subResp.contains("result")) << subResp.dump();

  f.fireWidgetChimed("hello");

  const auto note = awaitNotification(f, "eventTriggered");
  EXPECT_EQ(note["params"]["interestName"], "i1");
  EXPECT_EQ(note["params"]["objectName"], DispatcherFixture::widgetName);
  EXPECT_EQ(note["params"]["eventName"], "chimed");
  // STL declares `eventTriggered.args : string` (JSON-encoded array). Decode to inspect contents.
  const auto args = decodeEventArgs(note);
  ASSERT_TRUE(args.is_array());
  ASSERT_EQ(args.size(), 1U);
  EXPECT_EQ(args[0], "hello");
  // Source-stamped emission time, mirroring the propertyChanged.timestamp contract but sourced
  // from EventInfo.creationTime instead of getLastCommitTime - for events that's the right
  // freshness axis (the source attaches the timestamp at emit time).
  ASSERT_TRUE(note["params"].contains("timestamp")) << note.dump();
  ASSERT_TRUE(note["params"]["timestamp"].is_string()) << note.dump();
  EXPECT_FALSE(note["params"]["timestamp"].get<std::string>().empty()) << note.dump();
}

/// @test
/// Sticky end-to-end: on remove the active guards drop but the requested set persists; on re-add
/// the dispatcher rewires the guard and the next mutation reaches the client.
TEST(JsonRpc, subscribeSurvivesObjectRemoveAndReadd)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  f.pushFrame(
    connId,
    request("subscribeProperty",
            130,
            {{"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}, {"propertyName", "counter"}}));
  ASSERT_TRUE(popResponseFor(f, 130).contains("result"));

  // Remove and re-add the widget. The interestUpdate flow tells the client both transitions
  // happened; awaitNotification drains until the second `added` carrying the widget arrives.
  f.removeWidget();
  {
    const auto removed = awaitNotification(f, "interestUpdate");
    ASSERT_TRUE(removed["params"]["removed"].is_array());
    ASSERT_FALSE(removed["params"]["removed"].empty());
    EXPECT_EQ(removed["params"]["removed"][0], DispatcherFixture::widgetName);
  }
  f.addWidget();
  {
    const auto added = awaitNotification(f, "interestUpdate");
    ASSERT_TRUE(added["params"]["added"].is_array());
    ASSERT_FALSE(added["params"]["added"].empty());
    EXPECT_EQ(added["params"]["added"][0]["objectName"], DispatcherFixture::widgetName);
  }

  // Mutation against the rewired guard should produce a propertyChanged with the new value.
  // subscribeProperty seeded a snapshot (counter=0) before the remove/re-add cycle that may
  // still be sitting in the queue, so target counter=7 explicitly rather than taking the first
  // propertyChanged we see.
  f.setWidgetCounter(7);
  const auto note = awaitPropertyValue(f, "counter", 7);
  EXPECT_EQ(note["params"]["objectName"], DispatcherFixture::widgetName);
}

/// @test
/// Deferred-subscribe variant: subscribe while the object is absent, then re-add and confirm
/// the rewire happens against the freshly-arrived instance.
TEST(JsonRpc, subscribeBeforeObjectArrivesThenWiresOnAdd)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  // Take the widget out so the subscribe call records a deferred request rather than wiring
  // immediately. Drain the corresponding `removed` notification.
  f.removeWidget();
  std::ignore = awaitNotification(f, "interestUpdate");

  f.pushFrame(
    connId,
    request("subscribeProperty",
            140,
            {{"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}, {"propertyName", "counter"}}));
  ASSERT_TRUE(popResponseFor(f, 140).contains("result"));

  f.addWidget();
  std::ignore = awaitNotification(f, "interestUpdate");

  f.setWidgetCounter(11);
  const auto note = awaitNotification(f, "propertyChanged");
  EXPECT_EQ(findDecodedPropertyValue(note["params"]["values"], "counter"), 11);
}

/// @test
/// A second `subscribeProperty` without an explicit `maxRateHz` must preserve the per-object
/// rate set by the first call - omitting the field means "use the current rate", not "drop the
/// rate to unlimited". Drives `counter` and `label` rapidly through five distinct kernel ticks
/// after both subs are wired; the second sub leaving the rate intact means the bundle still
/// coalesces to a single propertyChanged.
TEST(JsonRpc, subscribePropertyOmittedRatePreservesPriorRate)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  // First sub establishes a 4Hz (250ms) per-object rate.
  f.pushFrame(connId,
              request("subscribeProperty",
                      300,
                      {{"interestName", "i1"},
                       {"objectName", DispatcherFixture::widgetName},
                       {"propertyName", "counter"},
                       {"maxRateHz", 4}}));
  ASSERT_TRUE(popResponseFor(f, 300).contains("result"));

  // Second sub on a different property without `maxRateHz` MUST NOT retune the per-object rate.
  f.pushFrame(
    connId,
    request("subscribeProperty",
            301,
            {{"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}, {"propertyName", "label"}}));
  ASSERT_TRUE(popResponseFor(f, 301).contains("result"));

  // Drain the snapshot frames the two subs produced.
  std::ignore = awaitPropertyValue(f, "counter", 0);
  std::ignore = awaitPropertyValue(f, "label", "");

  // Five back-to-back mutations should coalesce under the 4Hz throttle. If the bug had returned
  // (`label` sub silently retuning the rate to 0/unlimited), each mutation would emit on its own.
  for (int i = 1; i <= 5; ++i)
  {
    f.setWidgetCounter(i);
  }
  std::ignore = awaitPropertyValue(f, "counter", 5);
  EXPECT_TRUE(noFurtherFrames(f, 32));
}

/// @test
/// `maxRateHz` coalesces rapid changes within a throttle window into a single bundle. 250ms
/// (4Hz) keeps the test robust against virtual-clock jitter while the fixture ticks at 200Hz.
TEST(JsonRpc, subscribePropertyMaxRateHzCoalescesRapidChanges)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  f.pushFrame(connId,
              request("subscribeProperty",
                      200,
                      {{"interestName", "i1"},
                       {"objectName", DispatcherFixture::widgetName},
                       {"propertyName", "counter"},
                       {"maxRateHz", 4}}));  // 250ms interval
  ASSERT_TRUE(popResponseFor(f, 200).contains("result"));

  // setWidgetCounter is sync: each call lands in its own kernel tick (push + step), so we get
  // five distinct change callbacks instead of the same-tick coalesce setNextCounter would do
  // if they were batched. Every mutation here falls inside the throttle window and coalesces
  // at the rate-limiter to the latest value (5), delivered by the next post-window flush tick.
  for (int i = 1; i <= 5; ++i)
  {
    f.setWidgetCounter(i);
  }

  // The throttle reads virtual time from `RunApi::getTime()`, which advances per kernel.step().
  // awaitPropertyValue keeps stepping until the 250ms window has elapsed in virtual time and
  // the post-window flush emits the coalesced bundle.
  std::ignore = awaitPropertyValue(f, "counter", 5);

  // After the coalesced flush there should be no further notifications: nothing else has
  // changed.
  EXPECT_TRUE(noFurtherFrames(f, 32));
}

/// @test
/// Two same-tick writes land in one `propertyChanged` carrying both keys (per-object bundling,
/// not per-property).
TEST(JsonRpc, propertyChangedBundlesSameTickWritesIntoOneFrame)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);

  f.pushFrame(connId,
              request("createInterest",
                      400,
                      {{"interestName", "i1"},
                       {"query", "SELECT * FROM local.fixture"},
                       {"subscribe", {{"properties", wildcardSelector()}}}}));
  ASSERT_TRUE(popResponseFor(f, 400).contains("result"));
  ASSERT_EQ(awaitFirstAddedObjectName(f), DispatcherFixture::widgetName);

  // The createInterest subscribe-block snapshot rides on `interestUpdate.added.currentValues`
  // (drained by awaitFirstAddedObjectName above), not as a separate propertyChanged - so the
  // first propertyChanged we see is the bundled delta from the dual mutation below.
  f.setWidgetCounterAndLabel(42, "ready");

  const auto frame = awaitNotification(f, "propertyChanged");
  const auto& values = frame["params"]["values"];
  EXPECT_EQ(frame["params"]["objectName"], DispatcherFixture::widgetName);
  EXPECT_EQ(findDecodedPropertyValue(values, "counter"), 42);
  EXPECT_EQ(findDecodedPropertyValue(values, "label"), "ready");
}

/// @test
/// Each `propertyChanged` carries a `timestamp` (ISO8601 string sourced from the object's
/// `getLastCommitTime()`); consecutive bundles are non-decreasing.
TEST(JsonRpc, propertyChangedCarriesCommitTimestamp)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  f.pushFrame(
    connId,
    request("subscribeProperty",
            800,
            {{"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}, {"propertyName", "counter"}}));
  ASSERT_TRUE(popResponseFor(f, 800).contains("result"));

  // First bundle: snapshot from subscribeProperty (counter=0). Already carries a commit-time
  // timestamp from the fixture's onInit add.
  const auto first = awaitPropertyValue(f, "counter", 0);
  ASSERT_TRUE(first["params"].contains("timestamp")) << first.dump();
  ASSERT_TRUE(first["params"]["timestamp"].is_string()) << first.dump();
  const auto firstTs = first["params"]["timestamp"].get<std::string>();
  EXPECT_FALSE(firstTs.empty()) << first.dump();

  f.setWidgetCounter(58);
  const auto second = awaitPropertyValue(f, "counter", 58);
  ASSERT_TRUE(second["params"].contains("timestamp")) << second.dump();
  const auto secondTs = second["params"]["timestamp"].get<std::string>();
  EXPECT_FALSE(secondTs.empty()) << second.dump();
  // ISO8601 strings sort lexically iff they share a precision and timezone - Sen's toUtcString
  // gives us both, so >= is meaningful.
  EXPECT_GE(secondTs, firstTs) << "first=" << firstTs << " second=" << secondTs;
}

/// @test
/// `createInterest.subscribe.properties: "*"` auto-wires every property on every matched object's
/// class. The fixture widget exposes writable `counter` and `label`, so a mutation on either
/// drives a propertyChanged without any explicit subscribeProperty call.
TEST(JsonRpc, createInterestSubscribeBlockWildcardWiresAllProperties)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);

  f.pushFrame(connId,
              request("createInterest",
                      300,
                      {{"interestName", "i1"},
                       {"query", "SELECT * FROM local.fixture"},
                       {"subscribe", {{"properties", wildcardSelector()}}}}));
  ASSERT_TRUE(popResponseFor(f, 300).contains("result"));
  ASSERT_EQ(awaitFirstAddedObjectName(f), DispatcherFixture::widgetName);

  f.setWidgetCounter(7);

  const auto note = awaitNotification(f, "propertyChanged");
  EXPECT_EQ(note["params"]["objectName"], DispatcherFixture::widgetName);
  EXPECT_EQ(findDecodedPropertyValue(note["params"]["values"], "counter"), 7);
}

/// @test
/// Named-list variant: missing names are silently skipped (matching sticky-subscription
/// behavior).
TEST(JsonRpc, createInterestSubscribeBlockNamedListSkipsUnknownProperties)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);

  f.pushFrame(connId,
              request("createInterest",
                      310,
                      {{"interestName", "i1"},
                       {"query", "SELECT * FROM local.fixture"},
                       {"subscribe", {{"properties", namedSelector({"counter", "doesNotExist"})}}}}));
  ASSERT_TRUE(popResponseFor(f, 310).contains("result"));
  ASSERT_EQ(awaitFirstAddedObjectName(f), DispatcherFixture::widgetName);

  f.setWidgetCounter(13);
  const auto note = awaitNotification(f, "propertyChanged");
  EXPECT_EQ(findDecodedPropertyValue(note["params"]["values"], "counter"), 13);
}

/// @test
/// `subscribe.events: "*"` auto-wires every event on the class. We fire `chimed` and confirm the
/// payload arrives without any explicit subscribeEvent call.
TEST(JsonRpc, createInterestSubscribeBlockEventsWildcardWiresAllEvents)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);

  f.pushFrame(connId,
              request("createInterest",
                      320,
                      {{"interestName", "i1"},
                       {"query", "SELECT * FROM local.fixture"},
                       {"subscribe", {{"events", wildcardSelector()}}}}));
  ASSERT_TRUE(popResponseFor(f, 320).contains("result"));
  ASSERT_EQ(awaitFirstAddedObjectName(f), DispatcherFixture::widgetName);

  f.fireWidgetChimed("ding");
  const auto note = awaitNotification(f, "eventTriggered");
  EXPECT_EQ(note["params"]["eventName"], "chimed");
  EXPECT_EQ(decodeEventArgs(note)[0], "ding");
}

/// @test
/// `subscribe.maxRateHz` applies to every property the block wires.
TEST(JsonRpc, createInterestSubscribeBlockMaxRateHzAppliesToWiredProperties)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);

  f.pushFrame(connId,
              request("createInterest",
                      330,
                      {{"interestName", "i1"},
                       {"query", "SELECT * FROM local.fixture"},
                       {"subscribe", {{"properties", wildcardSelector()}, {"maxRateHz", 4}}}}));
  ASSERT_TRUE(popResponseFor(f, 330).contains("result"));
  ASSERT_EQ(awaitFirstAddedObjectName(f), DispatcherFixture::widgetName);

  // Subscribe block snapshot was bundled into interestUpdate.added (drained by
  // awaitFirstAddedObjectName) and primed `lastEmitTime`. Each setWidgetCounter is sync (push
  // + step), so the five mutations land in five distinct kernel ticks; they all fall inside
  // the throttle window and coalesce at the rate-limiter to the latest value.
  for (int i = 1; i <= 5; ++i)
  {
    f.setWidgetCounter(i);
  }

  // Throttle is on virtual time; awaitPropertyValue's step loop advances past the 250ms window.
  std::ignore = awaitPropertyValue(f, "counter", 5);
  EXPECT_TRUE(noFurtherFrames(f, 32));
}

/// @test
/// The subscribe block also applies to objects that arrive *after* `createInterest`.
TEST(JsonRpc, createInterestSubscribeBlockAppliesToObjectsArrivingLater)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);

  f.pushFrame(connId,
              request("createInterest",
                      340,
                      {{"interestName", "i1"},
                       {"query", "SELECT * FROM local.fixture"},
                       {"subscribe", {{"properties", wildcardSelector()}}}}));
  ASSERT_TRUE(popResponseFor(f, 340).contains("result"));
  ASSERT_EQ(awaitFirstAddedObjectName(f), DispatcherFixture::widgetName);

  f.removeWidget();
  std::ignore = awaitNotification(f, "interestUpdate");
  f.addWidget();
  std::ignore = awaitNotification(f, "interestUpdate");

  f.setWidgetCounter(99);
  const auto note = awaitNotification(f, "propertyChanged");
  EXPECT_EQ(findDecodedPropertyValue(note["params"]["values"], "counter"), 99);
}

/// @test
/// The subscribe block bundles `currentValues` into `interestUpdate.added` whenever it
/// subscribes to any properties (Sen's notifications are delta-only).
TEST(JsonRpc, createInterestSubscribeBlockSnapshotsInitialPropertyValues)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);

  // Drive a known counter value before the subscribe arrives, so the snapshot has a non-default
  // value that can only have come from reading the live object.
  f.setWidgetCounter(123);

  f.pushFrame(connId,
              request("createInterest",
                      360,
                      {{"interestName", "i1"},
                       {"query", "SELECT * FROM local.fixture"},
                       {"subscribe", {{"properties", wildcardSelector()}}}}));  // currentValues comes for free

  // Don't use popResponseFor here - it would consume the interestUpdate frame on its way to
  // finding the response. awaitNotification skips non-interestUpdate frames (including the
  // response itself) until it finds the snapshot.
  const auto note = awaitNotification(f, "interestUpdate");
  const auto& added = note["params"]["added"];
  ASSERT_FALSE(added.empty()) << note.dump();
  const auto& widgetEntry = added[0];
  EXPECT_EQ(widgetEntry["objectName"], DispatcherFixture::widgetName);
  ASSERT_TRUE(widgetEntry.contains("currentValues")) << widgetEntry.dump();
  EXPECT_EQ(findDecodedPropertyValue(widgetEntry["currentValues"], "counter"), 123);
}

/// @test
/// A malformed `properties` value is invalidParams - validation surfaces shape errors as
/// `createInterest` failures rather than silently degrading.
TEST(JsonRpc, createInterestSubscribeBlockBadSelectorIsInvalidParams)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);

  f.pushFrame(connId,
              request("createInterest",
                      350,
                      {{"interestName", "i1"},
                       {"query", "SELECT * FROM local.fixture"},
                       {"subscribe", {{"properties", 42}}}}));  // number is not a valid selector

  const auto resp = popResponseFor(f, 350);
  ASSERT_TRUE(resp.contains("error"));
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::invalidParams));
}

/// @test
/// `subscribeProperty` against a present object emits a one-shot snapshot with the current
/// value so the client sees initial state without waiting for the first delta.
TEST(JsonRpc, subscribePropertyDeliversInitialSnapshot)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  f.setWidgetCounter(77);

  f.pushFrame(
    connId,
    request("subscribeProperty",
            220,
            {{"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}, {"propertyName", "counter"}}));

  // Don't drain via popResponseFor - it would silently consume the snapshot frame on the way to
  // the response. awaitNotification finds the propertyChanged that subscribeProperty pushes out
  // synchronously after wiring the guard.
  const auto snapshot = awaitNotification(f, "propertyChanged");
  EXPECT_EQ(snapshot["params"]["objectName"], DispatcherFixture::widgetName);
  EXPECT_EQ(findDecodedPropertyValue(snapshot["params"]["values"], "counter"), 77);
}

/// @test
/// `maxRateHz` zero is invalidParams (and so are negatives / non-numerics; same code path).
TEST(JsonRpc, subscribePropertyMaxRateHzZeroIsInvalidParams)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  f.pushFrame(connId,
              request("subscribeProperty",
                      210,
                      {{"interestName", "i1"},
                       {"objectName", DispatcherFixture::widgetName},
                       {"propertyName", "counter"},
                       {"maxRateHz", 0}}));

  const auto resp = popResponseFor(f, 210);
  ASSERT_TRUE(resp.contains("error"));
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::invalidParams));
}

/// @test
/// The block re-applies on every onAdded, so a manual `unsubscribeProperty` against a block-
/// subscribed property is "for the current incarnation only". A permanent opt-out needs the
/// interest released and recreated without the block.
TEST(JsonRpc, subscribeBlockReappliesOverManualUnsubscribeAfterReadd)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  f.connect(conn);

  f.pushFrame(conn,
              request("createInterest",
                      410,
                      {{"interestName", "i1"},
                       {"query", "SELECT * FROM local.fixture"},
                       {"subscribe", {{"properties", wildcardSelector()}}}}));
  ASSERT_TRUE(popResponseFor(f, 410).contains("result"));
  ASSERT_EQ(awaitFirstAddedObjectName(f), DispatcherFixture::widgetName);

  // Manually unsubscribe the auto-wired counter sub.
  f.pushFrame(
    conn,
    request("unsubscribeProperty",
            411,
            {{"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}, {"propertyName", "counter"}}));
  ASSERT_TRUE(popResponseFor(f, 411).contains("result"));

  // Mutate - should now be silent (request and guard both gone).
  f.setWidgetCounter(50);
  EXPECT_TRUE(noFurtherFrames(f, 32));

  // Remove and re-add the widget. The block fires on add, re-creates the request, re-wires the
  // guard. The next mutation should then deliver.
  f.removeWidget();
  std::ignore = awaitNotification(f, "interestUpdate");
  f.addWidget();
  std::ignore = awaitNotification(f, "interestUpdate");

  f.setWidgetCounter(60);
  const auto note = awaitNotification(f, "propertyChanged");
  EXPECT_EQ(findDecodedPropertyValue(note["params"]["values"], "counter"), 60);
}
