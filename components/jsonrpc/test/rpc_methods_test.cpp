// === rpc_methods_test.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Request/response RPC surface: invoke, get/setProperty, listObjects, getType(s), and the
// per-connection type-cache contract. Streaming lives in subscribe_test.cpp; CRUD in
// lifecycle_test.cpp.

// local
#include "dispatcher.h"
#include "dispatcher_fixture.h"
#include "frame_helpers.h"

// nlohmann
#include "nlohmann/json.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

using sen::components::jsonrpc::ConnectionId;
using sen::components::jsonrpc::test::awaitNotification;
using sen::components::jsonrpc::test::DispatcherFixture;
using sen::components::jsonrpc::test::drainAllFramesOverSteps;
using sen::components::jsonrpc::test::encoded;
using sen::components::jsonrpc::test::popResponseFor;
using sen::components::jsonrpc::test::primeFixtureInterest;
using sen::components::jsonrpc::test::request;

/// @test
/// `invoke` with an unknown method name on a known object returns Method-not-found.
TEST(JsonRpc, invokeUnknownMethodReturnsMethodNotFound)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  f.pushFrame(
    connId,
    request("invoke",
            5,
            {{"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}, {"methodName", "doesNotExist"}}));

  const auto envelope = popResponseFor(f, 5);
  EXPECT_EQ(envelope["jsonrpc"], "2.0");
  ASSERT_TRUE(envelope.contains("error"));
  EXPECT_EQ(envelope["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::methodNotFound));
}

/// @test
/// `invoke` happy path through the WorkQueue: `widget.doubled(21)` returns 42.
TEST(JsonRpc, invokeReturnsValueFromKernelMethod)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  f.pushFrame(connId,
              request("invoke",
                      120,
                      {{"interestName", "i1"},
                       {"objectName", DispatcherFixture::widgetName},
                       {"methodName", "doubled"},
                       {"argsJson", encoded(nlohmann::json::array({21}))}}));

  const auto resp = popResponseFor(f, 120);
  ASSERT_TRUE(resp.contains("result")) << resp.dump();
  ASSERT_TRUE(resp["result"].is_string()) << resp.dump();
  EXPECT_EQ(nlohmann::json::parse(resp["result"].get<std::string>()), 42);
}

/// @test
/// A throwing target method becomes an `internalError` (-32603) with the exception's `what()`
/// in `data` and a stable `message`.
TEST(JsonRpc, invokeReturnsErrorWhenHandlerThrows)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  primeFixtureInterest(f, connId, "i1");

  f.pushFrame(connId,
              request("invoke",
                      900,
                      {{"interestName", "i1"},
                       {"objectName", "widget"},
                       {"methodName", "boom"},
                       {"argsJson", encoded(nlohmann::json::array())}}));
  const auto resp = popResponseFor(f, 900);
  ASSERT_TRUE(resp.contains("error")) << resp.dump();
  const auto& err = resp["error"];
  EXPECT_EQ(err["code"].get<int>(), static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::internalError));
  EXPECT_EQ(err["message"].get<std::string>(), "invoke: handler threw an exception");
  ASSERT_TRUE(err.contains("data")) << resp.dump();
  EXPECT_EQ(err["data"].get<std::string>(), "boom");
}

/// @test
/// An arg that fails string-to-number coercion is invalidParams (`parseInvokeArgs` catches the
/// thrown `std::invalid_argument` so it can't terminate the dispatcher). The coercion text rides
/// in `data`, not `message`.
TEST(JsonRpc, invokeArgTypeMismatchIsInvalidParams)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  primeFixtureInterest(f, conn, "i1");

  f.pushFrame(conn,
              request("invoke",
                      1131,
                      {{"interestName", "i1"},
                       {"objectName", "widget"},
                       {"methodName", "doubled"},
                       {"argsJson", encoded(nlohmann::json::array({"not-a-number"}))}}));
  const auto resp = popResponseFor(f, 1131);
  ASSERT_TRUE(resp.contains("error")) << resp.dump();
  const auto& err = resp["error"];
  EXPECT_EQ(err["code"].get<int>(), static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::invalidParams));
  EXPECT_EQ(err["message"].get<std::string>(), "invoke: invalid args");
  ASSERT_TRUE(err.contains("data")) << resp.dump();
  EXPECT_TRUE(err["data"].is_string()) << resp.dump();
  EXPECT_FALSE(err["data"].get<std::string>().empty()) << resp.dump();
}

/// @test
/// `invoke` rejects an `argsJson` that is a string but doesn't parse as JSON, with the dedicated
/// "is not valid JSON" message (separate from the post-parse coercion error).
TEST(JsonRpc, invokeMalformedArgsJsonIsInvalidParams)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  primeFixtureInterest(f, conn, "i1");

  f.pushFrame(
    conn,
    request("invoke",
            1132,
            {{"interestName", "i1"}, {"objectName", "widget"}, {"methodName", "doubled"}, {"argsJson", "garbage{"}}));
  const auto resp = popResponseFor(f, 1132);
  ASSERT_TRUE(resp.contains("error")) << resp.dump();
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::invalidParams));
  EXPECT_EQ(resp["error"]["message"].get<std::string>(), "invoke: 'argsJson' is not valid JSON");
}

/// @test
/// `getTypes` returns the kernel's custom-type registry as a JSON array of qualified names.
/// FixtureWidget is loaded into this kernel via `sen_generate_cpp`, so it must be present.
TEST(JsonRpc, getTypesReturnsRegisteredCustomTypes)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  f.pushFrame(connId, request("getTypes", 1000));
  const auto resp = popResponseFor(f, 1000);
  ASSERT_TRUE(resp.contains("result")) << resp.dump();
  ASSERT_TRUE(resp["result"].is_array());
  const auto names = resp["result"].get<std::vector<std::string>>();
  EXPECT_NE(std::find(names.begin(), names.end(), "sen.components.jsonrpc.test.FixtureWidget"), names.end())
    << resp.dump();
}

/// @test
/// `getType` returns a `TypeLookupResult` wrapping the `CustomTypeSpec`. `schema` is empty
/// when `withSchema` is not set.
TEST(JsonRpc, getTypeReturnsSpecForRegisteredType)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  f.pushFrame(connId, request("getType", 1001, {{"qualifiedName", "sen.components.jsonrpc.test.FixtureWidget"}}));
  const auto resp = popResponseFor(f, 1001);
  ASSERT_TRUE(resp.contains("result")) << resp.dump();
  const auto& result = resp["result"];
  const auto& spec = result["spec"];
  EXPECT_EQ(spec["qualifiedName"].get<std::string>(), "sen.components.jsonrpc.test.FixtureWidget");
  EXPECT_FALSE(spec["data"].empty());
  EXPECT_TRUE(result["schema"].get<std::string>().empty());
}

/// @test
/// `getType` with an unknown qualified name fails with unknownType.
TEST(JsonRpc, getTypeUnknownNameIsUnknownType)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  f.pushFrame(connId, request("getType", 1002, {{"qualifiedName", "no.such.Type"}}));
  const auto resp = popResponseFor(f, 1002);
  ASSERT_TRUE(resp.contains("error")) << resp.dump();
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::unknownType));
}

/// @test
/// `listObjects` enumerates the current match set (a snapshot, not a replay of
/// `interestUpdate` history).
TEST(JsonRpc, listObjectsReturnsCurrentMatchSet)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  primeFixtureInterest(f, conn, "i1");

  f.pushFrame(conn, request("listObjects", 1100, {{"interestName", "i1"}}));
  const auto resp = popResponseFor(f, 1100);
  ASSERT_TRUE(resp.contains("result")) << resp.dump();
  ASSERT_TRUE(resp["result"].is_array());
  ASSERT_EQ(resp["result"].size(), 1U);
  EXPECT_EQ(resp["result"][0]["objectName"].get<std::string>(), DispatcherFixture::widgetName);
  EXPECT_EQ(resp["result"][0]["qualifiedClassName"].get<std::string>(), "sen.components.jsonrpc.test.FixtureWidget");
}

/// @test
/// `listObjects` for an interest the connection didn't open returns unknownInterest. Mirrors
/// the invoke / subscribe / release behavior for unknown interest names.
TEST(JsonRpc, listObjectsUnknownInterestIsUnknownInterest)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  f.pushFrame(connId, request("listObjects", 1101, {{"interestName", "nope"}}));
  const auto resp = popResponseFor(f, 1101);
  ASSERT_TRUE(resp.contains("error")) << resp.dump();
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::unknownInterest));
}

/// @test
/// `getProperty` returns the current value synchronously. Pre-set the counter via the fixture's
/// kernel-thread mutator, then read it back over JSON-RPC.
TEST(JsonRpc, getPropertyReturnsCurrentValue)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  primeFixtureInterest(f, conn, "i1");
  f.setWidgetCounter(123);

  f.pushFrame(
    conn,
    request("getProperty", 1110, {{"interestName", "i1"}, {"objectName", "widget"}, {"propertyName", "counter"}}));
  const auto resp = popResponseFor(f, 1110);
  ASSERT_TRUE(resp.contains("result")) << resp.dump();
  // `result` is a JSON-encoded string per the STL signature; parse it to inspect the typed value.
  ASSERT_TRUE(resp["result"].is_string()) << resp.dump();
  EXPECT_EQ(nlohmann::json::parse(resp["result"].get<std::string>()), 123);
}

/// @test
/// `getProperty` against a variant-typed property must encode the variant on the wire as
/// `{type: "<qualified-name>", value: ...}`, not the binary-protocol's numeric arm key. The
/// qualified-name form is the documented wire contract and matches what `adaptVariant`
/// (`useStrings=true`) produces.
TEST(JsonRpc, getPropertyOnVariantEmitsQualifiedNameWireShape)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  primeFixtureInterest(f, conn, "i1");
  f.setWidgetModeBusy("compiling");

  f.pushFrame(
    conn, request("getProperty", 1120, {{"interestName", "i1"}, {"objectName", "widget"}, {"propertyName", "mode"}}));
  const auto resp = popResponseFor(f, 1120);
  ASSERT_TRUE(resp.contains("result")) << resp.dump();
  ASSERT_TRUE(resp["result"].is_string()) << resp.dump();
  const auto decoded = nlohmann::json::parse(resp["result"].get<std::string>());
  ASSERT_TRUE(decoded.is_object()) << decoded.dump();
  ASSERT_TRUE(decoded.contains("type")) << decoded.dump();
  ASSERT_TRUE(decoded["type"].is_string()) << decoded.dump();
  EXPECT_EQ(decoded["type"].get<std::string>(), "sen.components.jsonrpc.test.ModeBusy");
  ASSERT_TRUE(decoded.contains("value")) << decoded.dump();
  EXPECT_EQ(decoded["value"]["taskName"].get<std::string>(), "compiling");
}

/// @test
/// `getProperty` against an unknown property name (typo, not a member of the class) is
/// unknownMember.
TEST(JsonRpc, getPropertyUnknownPropertyIsUnknownMember)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  primeFixtureInterest(f, conn, "i1");

  f.pushFrame(
    conn,
    request(
      "getProperty", 1111, {{"interestName", "i1"}, {"objectName", "widget"}, {"propertyName", "no_such_field"}}));
  const auto resp = popResponseFor(f, 1111);
  ASSERT_TRUE(resp.contains("error")) << resp.dump();
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::unknownMember));
}

/// @test
/// `setProperty` writes via the setter method (async through `invokeUntyped` + WorkQueue);
/// `getProperty` round-trip confirms the new value landed on the kernel side.
TEST(JsonRpc, setPropertyWritesAndIsObservableViaGetProperty)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  primeFixtureInterest(f, conn, "i1");

  f.pushFrame(
    conn,
    request("setProperty",
            1120,
            {{"interestName", "i1"}, {"objectName", "widget"}, {"propertyName", "counter"}, {"value", encoded(777)}}));
  const auto setResp = popResponseFor(f, 1120);
  ASSERT_TRUE(setResp.contains("result")) << setResp.dump();

  f.pushFrame(
    conn,
    request("getProperty", 1121, {{"interestName", "i1"}, {"objectName", "widget"}, {"propertyName", "counter"}}));
  const auto getResp = popResponseFor(f, 1121);
  ASSERT_TRUE(getResp.contains("result")) << getResp.dump();
  ASSERT_TRUE(getResp["result"].is_string()) << getResp.dump();
  EXPECT_EQ(nlohmann::json::parse(getResp["result"].get<std::string>()), 777);
}

/// @test
/// `setProperty` with a value that doesn't fit the property's type (here, a string into an i32
/// counter) is invalidParams via the same arg-coercion path invoke uses. Pins that the
/// setProperty / invoke type-check stories don't drift apart.
TEST(JsonRpc, setPropertyTypeMismatchIsInvalidParams)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  primeFixtureInterest(f, conn, "i1");

  f.pushFrame(conn,
              request("setProperty",
                      1122,
                      {{"interestName", "i1"},
                       {"objectName", "widget"},
                       {"propertyName", "counter"},
                       {"value", encoded("not-a-number")}}));
  const auto resp = popResponseFor(f, 1122);
  ASSERT_TRUE(resp.contains("error")) << resp.dump();
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::invalidParams));
}

/// @test
/// `setProperty` rejects a non-string `value` with invalidParams. Locks in the wire-shape change:
/// the STL declares `value : string`, so a JSON number like `{"value": 777}` (the legacy shape)
/// must be rejected, not silently coerced.
TEST(JsonRpc, setPropertyNonStringValueIsInvalidParams)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  primeFixtureInterest(f, conn, "i1");

  f.pushFrame(conn,
              request("setProperty",
                      1123,
                      {{"interestName", "i1"}, {"objectName", "widget"}, {"propertyName", "counter"}, {"value", 777}}));
  const auto resp = popResponseFor(f, 1123);
  ASSERT_TRUE(resp.contains("error")) << resp.dump();
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::invalidParams));
}

/// @test
/// `setProperty` rejects a `value` that is a string but doesn't parse as JSON.
TEST(JsonRpc, setPropertyMalformedEncodedValueIsInvalidParams)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  primeFixtureInterest(f, conn, "i1");

  f.pushFrame(
    conn,
    request(
      "setProperty",
      1124,
      {{"interestName", "i1"}, {"objectName", "widget"}, {"propertyName", "counter"}, {"value", "not valid {{json"}}));
  const auto resp = popResponseFor(f, 1124);
  ASSERT_TRUE(resp.contains("error")) << resp.dump();
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::invalidParams));
  EXPECT_EQ(resp["error"]["message"].get<std::string>(), "setProperty: 'value' is not valid JSON");
}

/// @test
/// `setProperty` on a read-only property is notWritable. Gated up front because invokeUntyped
/// on the invalid setter handle would `std::terminate`.
TEST(JsonRpc, setPropertyOnReadOnlyPropertyIsNotWritable)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  primeFixtureInterest(f, conn, "i1");

  f.pushFrame(
    conn,
    request(
      "setProperty",
      1130,
      {{"interestName", "i1"}, {"objectName", "widget"}, {"propertyName", "readonlyId"}, {"value", encoded(42)}}));
  const auto resp = popResponseFor(f, 1130);
  ASSERT_TRUE(resp.contains("error")) << resp.dump();
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::notWritable));
}

/// @test
/// `getProperty` after the object leaves the match set is objectNotInInterest. Sticky
/// subscriptions persist the name in book-keeping, but the read fails early rather than serve
/// a stale value.
TEST(JsonRpc, getPropertyAfterObjectRemovedIsObjectNotInInterest)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  primeFixtureInterest(f, conn, "i1");

  f.removeWidget();
  std::ignore = awaitNotification(f, "interestUpdate");  // the synchronous onRemoved fanout

  f.pushFrame(
    conn,
    request("getProperty", 1132, {{"interestName", "i1"}, {"objectName", "widget"}, {"propertyName", "counter"}}));
  const auto resp = popResponseFor(f, 1132);
  ASSERT_TRUE(resp.contains("error")) << resp.dump();
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::objectNotInInterest));
}

/// @test
/// `getObjectsBatchState` with no filters returns every property of every matched object in
/// one round-trip. Property values are JSON-encoded SenValue strings, matching `getProperty`'s
/// encoding so callers don't need a second decoding path.
TEST(JsonRpc, getObjectsBatchStateReturnsAllPropertiesForAllObjects)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  primeFixtureInterest(f, conn, "i1");
  // Initialize every confirmed property so the read can't trip on an unset value and surface in
  // `errors` instead of `properties` (this test pins the all-success shape).
  f.setWidgetCounterAndLabel(7, "ready");
  f.setWidgetModeIdle("startup");

  f.pushFrame(conn, request("getObjectsBatchState", 1140, {{"interestName", "i1"}}));
  const auto resp = popResponseFor(f, 1140);
  ASSERT_TRUE(resp.contains("result")) << resp.dump();
  ASSERT_TRUE(resp["result"].is_array()) << resp.dump();
  ASSERT_EQ(resp["result"].size(), 1U);

  const auto& entry = resp["result"][0];
  EXPECT_EQ(entry["objectName"].get<std::string>(), DispatcherFixture::widgetName);
  EXPECT_EQ(entry["qualifiedClassName"].get<std::string>(), "sen.components.jsonrpc.test.FixtureWidget");
  ASSERT_TRUE(entry["errors"].is_array()) << entry.dump();
  EXPECT_EQ(entry["errors"].size(), 0U) << "expected `errors` to be empty when nothing failed: " << entry.dump();

  std::unordered_map<std::string, std::string> propMap;
  for (const auto& pv: entry["properties"])
  {
    propMap.emplace(pv["propertyName"].get<std::string>(), pv["value"].get<std::string>());
  }
  EXPECT_EQ(nlohmann::json::parse(propMap.at("counter")), 7);
  EXPECT_EQ(nlohmann::json::parse(propMap.at("label")), "ready");
  // Every declared property of FixtureWidget surfaces; the exact set is locked to the fixture STL.
  for (const char* name: {"counter", "label", "readonlyId", "mode"})
  {
    EXPECT_TRUE(propMap.count(name) == 1U) << "missing property: " << name << " in " << entry.dump();
  }
}

/// @test
/// `propertyNames` restricts the per-object property set; unknown names land in `errors` so the
/// rest of the batch still returns useful data. Successful reads and failures are split into
/// separate lists so a property literally named `error` can never collide with the failure
/// envelope.
TEST(JsonRpc, getObjectsBatchStatePropertyFilterAndUnknownNameError)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  primeFixtureInterest(f, conn, "i1");
  f.setWidgetCounter(42);

  f.pushFrame(
    conn,
    request("getObjectsBatchState",
            1141,
            {{"interestName", "i1"}, {"propertyNames", nlohmann::json::array({"counter", "no_such_field"})}}));
  const auto resp = popResponseFor(f, 1141);
  ASSERT_TRUE(resp.contains("result")) << resp.dump();
  ASSERT_EQ(resp["result"].size(), 1U);

  const auto& entry = resp["result"][0];
  ASSERT_EQ(entry["properties"].size(), 1U);
  EXPECT_EQ(entry["properties"][0]["propertyName"].get<std::string>(), "counter");
  EXPECT_EQ(nlohmann::json::parse(entry["properties"][0]["value"].get<std::string>()), 42);
  ASSERT_EQ(entry["errors"].size(), 1U) << entry.dump();
  EXPECT_EQ(entry["errors"][0]["propertyName"].get<std::string>(), "no_such_field");
  EXPECT_EQ(entry["errors"][0]["error"].get<std::string>(), "unknown property");
}

/// @test
/// `objectNames` filters the match set; names that aren't currently matched are silently
/// dropped so the caller diffs requested vs returned names rather than parsing per-object
/// error envelopes.
TEST(JsonRpc, getObjectsBatchStateSilentlySkipsUnmatchedObjectNames)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  primeFixtureInterest(f, conn, "i1");

  f.pushFrame(conn,
              request("getObjectsBatchState",
                      1142,
                      {{"interestName", "i1"},
                       {"objectNames", nlohmann::json::array({DispatcherFixture::widgetName, "not_in_match_set"})},
                       {"propertyNames", nlohmann::json::array({"counter"})}}));
  const auto resp = popResponseFor(f, 1142);
  ASSERT_TRUE(resp.contains("result")) << resp.dump();
  ASSERT_EQ(resp["result"].size(), 1U);
  EXPECT_EQ(resp["result"][0]["objectName"].get<std::string>(), DispatcherFixture::widgetName);
}

/// @test
/// `getObjectsBatchState` against an interest the connection didn't open is unknownInterest.
TEST(JsonRpc, getObjectsBatchStateUnknownInterestIsUnknownInterest)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  f.pushFrame(connId, request("getObjectsBatchState", 1143, {{"interestName", "nope"}}));
  const auto resp = popResponseFor(f, 1143);
  ASSERT_TRUE(resp.contains("error")) << resp.dump();
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::unknownInterest));
}

/// @test
/// `getType` is idempotent: every call returns the spec, ignoring the per-connection cache.
/// The cache-suppression on the implicit `interestUpdate` path lives in the multi-connection
/// suite.
TEST(JsonRpc, getTypeIsIdempotentRegardlessOfCache)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  f.connect(conn);

  const nlohmann::json widgetTypeParams {{"qualifiedName", "sen.components.jsonrpc.test.FixtureWidget"}};

  f.pushFrame(conn, request("getType", 1010, widgetTypeParams));
  const auto first = popResponseFor(f, 1010);
  ASSERT_TRUE(first.contains("result"));
  EXPECT_EQ(first["result"]["spec"]["qualifiedName"].get<std::string>(), "sen.components.jsonrpc.test.FixtureWidget");

  f.pushFrame(conn, request("getType", 1011, widgetTypeParams));
  const auto second = popResponseFor(f, 1011);
  ASSERT_TRUE(second.contains("result"));
  EXPECT_EQ(second["result"]["spec"]["qualifiedName"].get<std::string>(), "sen.components.jsonrpc.test.FixtureWidget");
}
