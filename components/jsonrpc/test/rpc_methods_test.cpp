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
#include "dispatcher_fixture.h"
#include "frame_helpers.h"
#include "messages.h"

// nlohmann
#include "nlohmann/json.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

using sen::components::jsonrpc::ConnectionId;
using sen::components::jsonrpc::test::awaitNotification;
using sen::components::jsonrpc::test::DispatcherFixture;
using sen::components::jsonrpc::test::encoded;
using sen::components::jsonrpc::test::popResponseFor;
using sen::components::jsonrpc::test::primeFixtureInterest;
using sen::components::jsonrpc::test::request;

/// @test
/// Returns the methodNotFound error when invoke names a method that does not exist on a known
/// object.
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
/// Invokes the widget's doubled method with argument 21 and returns a JSON-encoded result that
/// decodes to 42.
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
/// Converts an exception thrown by the invoked method into an internalError response that
/// carries a stable handler-threw message and the exception's own text in the data field.
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
/// Rejects an invoke argument that fails string-to-number coercion with invalidParams instead
/// of terminating the dispatcher, placing the coercion detail in the error data field and a
/// fixed invalid-args text in the message.
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
/// Rejects an invoke whose argsJson string does not parse as JSON with invalidParams and a
/// dedicated message saying argsJson is not valid JSON, distinct from the post-parse coercion
/// error.
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
/// Returns the kernel's custom-type registry from getTypes as a JSON array of qualified names
/// that includes the FixtureWidget type loaded into the test kernel.
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
/// Returns the registered type's spec from getType, echoing the qualified name with non-empty
/// spec data, and leaves the schema field empty when withSchema is not set.
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
/// Answers getType for an unregistered qualified name with the unknownType error.
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
/// Enumerates the interest's current match set in listObjects as a snapshot, returning the one
/// matched widget with its object name and qualified class name.
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
/// Answers listObjects for an interest name the connection never opened with the
/// unknownInterest error.
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
/// Returns the property's current value from getProperty, a counter set to 123 on the kernel
/// side reads back as a JSON-encoded 123.
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
/// Encodes a variant-typed property on the wire as an object whose type field holds the active
/// arm's qualified name and whose value field holds its payload, not the binary protocol's
/// numeric arm key.
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
/// Answers getProperty for a property name that is not a member of the class with the
/// unknownMember error.
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
/// Writes a property value through setProperty so that a follow-up getProperty on the same
/// connection reads back the 777 just written, proving the write landed on the kernel side.
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
/// Rejects a setProperty value that does not fit the property's type with invalidParams, a
/// string written into the integer counter fails through the same coercion path invoke uses.
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
/// Rejects a setProperty whose value field is a bare JSON number instead of the declared
/// JSON-encoded string with invalidParams rather than silently coercing the legacy shape.
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
/// Rejects a setProperty whose value string does not parse as JSON with invalidParams and a
/// message naming the invalid value field.
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
/// Answers setProperty on a read-only property with the notWritable error, gating up front
/// instead of reaching the invalid setter handle.
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
/// Answers getProperty with the objectNotInInterest error once the object has left the match
/// set, failing early instead of serving a stale value.
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
/// Returns every property of every matched object in one getObjectsBatchState round-trip when
/// no filters are given, each value JSON-encoded in the same form getProperty uses, with an
/// empty per-object errors list.
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
/// Restricts getObjectsBatchState to the requested propertyNames and reports an unknown name
/// in the per-object errors list while the known property still returns its value, keeping
/// successful reads and failures in separate lists.
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
/// Filters getObjectsBatchState by objectNames and silently drops a requested name that is not
/// in the match set, returning only the matched object with no error entry for the absent one.
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
/// Answers getObjectsBatchState for an interest the connection never opened with the
/// unknownInterest error.
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
/// Returns the full type spec from every getType call, a repeated lookup on the same
/// connection is not suppressed by the per-connection type cache.
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
