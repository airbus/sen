// === lifecycle_test.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// local
#include "dispatcher_fixture.h"
#include "frame_helpers.h"
#include "messages.h"

// sen
#include "sen/kernel/test_kernel.h"

// nlohmann
#include "nlohmann/json.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <string>
#include <tuple>

using sen::components::jsonrpc::ConnectionId;
using sen::components::jsonrpc::test::awaitNotification;
using sen::components::jsonrpc::test::DispatcherFixture;
using sen::components::jsonrpc::test::popResponseFor;
using sen::components::jsonrpc::test::primeFixtureInterest;
using sen::components::jsonrpc::test::request;

/// @test
/// Exercises the `dlopen`/SEN_COMPONENT path. Requires `LD_LIBRARY_PATH` to include the build's
/// `bin/` so `libjsonrpc.so` is found.
TEST(JsonRpc, loadsViaTestKernel)
{
  const std::string configString = R"(
    load:
    - name: jsonrpc
      group: 3
      address: "127.0.0.1"
      port: 9090
  )";

  auto kernel = sen::kernel::TestKernel::fromYamlString(configString);
  auto context = kernel.getComponentContext("jsonrpc");
  ASSERT_TRUE(context.has_value());
  ASSERT_NE(context.value()->instance, nullptr);
}

/// @test
/// `ping` round-trips via the dispatcher: inbound queue -> method dispatch -> outbound queue.
TEST(JsonRpc, pingRoundTrips)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  f.pushFrame(connId, request("ping", 1));

  const auto envelope = f.popJsonAfterStepping();
  EXPECT_EQ(envelope["jsonrpc"], "2.0");
  EXPECT_EQ(envelope["result"], "pong");
  EXPECT_EQ(envelope["id"], 1);
}

/// @test
/// `listTopology` returns a JSON array of `{name, buses}` entries; pins the wiring through
/// the dispatcher-level `TopologyService` and the kernel's session/bus discovery surface.
TEST(JsonRpc, listTopologyRoundTrips)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  f.pushFrame(connId, request("listTopology", 2));

  const auto envelope = f.popJsonAfterStepping();
  EXPECT_EQ(envelope["jsonrpc"], "2.0");
  EXPECT_EQ(envelope["id"], 2);
  ASSERT_TRUE(envelope.contains("result"));
  ASSERT_TRUE(envelope["result"].is_array());
  for (const auto& entry: envelope["result"])
  {
    EXPECT_TRUE(entry.contains("name"));
    EXPECT_TRUE(entry.contains("buses"));
    EXPECT_TRUE(entry["buses"].is_array());
  }
}

/// @test
/// `subscribeTopology` pushes an initial `topologyChanged` carrying the current snapshot,
/// then leaves the subscription open for the connection's lifetime.
TEST(JsonRpc, subscribeTopologyPushesInitialSnapshot)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  f.pushFrame(connId, request("subscribeTopology", 3));

  const auto frame = awaitNotification(f, "topologyChanged");
  ASSERT_TRUE(frame["params"].contains("sessions"));
  EXPECT_TRUE(frame["params"]["sessions"].is_array());
}

/// @test
/// `createInterest` returns null and pushes at least one `interestUpdate` carrying objects
/// already on the bus and a `types` array with each matched class's spec.
TEST(JsonRpc, createInterestPushesUpdate)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  f.pushFrame(connId, request("createInterest", 3, {{"interestName", "i1"}, {"query", "SELECT * FROM local.fixture"}}));

  const auto response = popResponseFor(f, 3);
  EXPECT_EQ(response["jsonrpc"], "2.0");
  ASSERT_TRUE(response.contains("result"));
  EXPECT_TRUE(response["result"].is_null());

  const auto frame = awaitNotification(f, "interestUpdate");
  const auto& params = frame["params"];
  ASSERT_TRUE(params.contains("interestName"));
  EXPECT_EQ(params["interestName"].get<std::string>(), "i1");
  ASSERT_FALSE(params["added"].empty());
  const auto& firstAdded = params["added"][0];
  ASSERT_TRUE(firstAdded.contains("objectName"));
  ASSERT_TRUE(firstAdded.contains("qualifiedClassName"));
  EXPECT_FALSE(firstAdded["qualifiedClassName"].get<std::string>().empty());

  // `types` is a CustomTypeSpecList (sequence of CustomTypeSpec). Look up by `qualifiedName`
  // via a linear scan rather than the previous map-style `contains(qname)` accessor.
  ASSERT_TRUE(params.contains("types"));
  ASSERT_TRUE(params["types"].is_array());
  const auto qname = firstAdded["qualifiedClassName"].get<std::string>();
  bool foundSpec = false;
  for (const auto& spec: params["types"])
  {
    if (spec.value("qualifiedName", "") == qname)
    {
      foundSpec = true;
      break;
    }
  }
  EXPECT_TRUE(foundSpec) << params["types"].dump();
}

/// @test
/// `createInterest` rejects empty `name` with invalidParams (early gate before any kernel work).
TEST(JsonRpc, createInterestEmptyNameFails)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  f.pushFrame(connId, request("createInterest", 4, {{"interestName", ""}, {"query", "SELECT * FROM local.fixture"}}));

  const auto resp = popResponseFor(f, 4);
  ASSERT_TRUE(resp.contains("error"));
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::invalidParams));
}

/// @test
/// Two `createInterest` calls with the same name on the same connection: the second is rejected
/// with invalidParams. Forces the client to releaseInterest first if they want to redefine.
TEST(JsonRpc, createInterestDuplicateNameFails)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  const nlohmann::json dupParams {{"interestName", "dup"}, {"query", "SELECT * FROM local.fixture"}};
  f.pushFrame(connId, request("createInterest", 1, dupParams));
  const auto firstResp = popResponseFor(f, 1);
  ASSERT_TRUE(firstResp.contains("result"));

  f.pushFrame(connId, request("createInterest", 2, dupParams));
  const auto secondResp = popResponseFor(f, 2);
  ASSERT_TRUE(secondResp.contains("error"));
  EXPECT_EQ(secondResp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::invalidParams));
}

/// @test
/// `createInterest` against a bus nothing has published on yet succeeds (kernel `getSource` is
/// get-or-create), and the interest resolves via a live `interestUpdate` once a publisher joins
/// the address. Reconnecting clients that re-declare before the server's domain has rebuilt
/// depend on this; pinned here so a future "validate the bus" refactor fails loudly.
TEST(JsonRpc, createInterestOnNotYetExistingBusResolvesWhenBusAppears)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  f.pushFrame(connId,
              request("createInterest", 1, {{"interestName", "late"}, {"query", "SELECT * FROM local.latebus"}}));
  const auto response = popResponseFor(f, 1);
  ASSERT_TRUE(response.contains("result"));

  std::ignore = f.drainNow();
  f.publishWidgetOn("local.latebus", "lateWidget");

  const auto frame = awaitNotification(f, "interestUpdate");
  EXPECT_EQ(frame["params"]["interestName"].get<std::string>(), "late");
  ASSERT_FALSE(frame["params"]["added"].empty());
  EXPECT_EQ(frame["params"]["added"][0]["objectName"].get<std::string>(), "lateWidget");
}

/// @test
/// The per-connection interest ceiling (256): the creation that would exceed it fails with an
/// actionable invalidParams and existing interests stay usable.
TEST(JsonRpc, createInterestBeyondPerConnectionCapFails)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  for (int i = 0; i < 256; ++i)
  {
    f.pushFrame(connId,
                request("createInterest",
                        i + 1,
                        {{"interestName", "cap_" + std::to_string(i)}, {"query", "SELECT * FROM local.capbus"}}));
    const auto resp = popResponseFor(f, i + 1);
    ASSERT_TRUE(resp.contains("result")) << "creation " << i << " unexpectedly failed";
  }

  f.pushFrame(connId,
              request("createInterest", 999, {{"interestName", "overflow"}, {"query", "SELECT * FROM local.capbus"}}));
  const auto resp = popResponseFor(f, 999);
  ASSERT_TRUE(resp.contains("error"));
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::invalidParams));
  EXPECT_NE(resp["error"]["message"].get<std::string>().find("limit"), std::string::npos);

  // Existing interests are unaffected: releasing one frees room for a new one.
  f.pushFrame(connId, request("releaseInterest", 1000, {{"interestName", "cap_0"}}));
  ASSERT_TRUE(popResponseFor(f, 1000).contains("result"));
  f.pushFrame(connId,
              request("createInterest", 1001, {{"interestName", "overflow"}, {"query", "SELECT * FROM local.capbus"}}));
  ASSERT_TRUE(popResponseFor(f, 1001).contains("result"));
}

/// @test
/// `releaseInterest` returns null and actually erases the entry (a follow-up `invoke` against
/// the released name fails with unknownInterest).
TEST(JsonRpc, releaseInterestRemovesSubscription)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  primeFixtureInterest(f, connId, "i1");

  f.pushFrame(connId, request("releaseInterest", 2, {{"interestName", "i1"}}));
  const auto releaseResp = popResponseFor(f, 2);
  ASSERT_TRUE(releaseResp.contains("result"));
  EXPECT_TRUE(releaseResp["result"].is_null());

  f.pushFrame(
    connId,
    request(
      "invoke", 3, {{"interestName", "i1"}, {"objectName", DispatcherFixture::widgetName}, {"methodName", "doubled"}}));
  const auto invokeResp = popResponseFor(f, 3);
  ASSERT_TRUE(invokeResp.contains("error"));
  EXPECT_EQ(invokeResp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::unknownInterest));
}

/// @test
/// `releaseInterest` on an unknown name returns unknownInterest (lookup, not shape gate).
TEST(JsonRpc, releaseInterestUnknownNameFails)
{
  DispatcherFixture f;
  const ConnectionId connId {1U};
  f.connect(connId);
  f.pushFrame(connId, request("releaseInterest", 7, {{"interestName", "nope"}}));

  const auto resp = f.popJsonAfterStepping();
  EXPECT_EQ(resp["id"], 7);
  ASSERT_TRUE(resp.contains("error"));
  EXPECT_EQ(resp["error"]["code"].get<int>(),
            static_cast<int>(sen::components::jsonrpc::JsonRpcErrorCode::unknownInterest));
}

/// @test
/// `ClientConnected` publishes a `client-<id>` server on `local.jsonrpc`; `ClientDisconnected`
/// removes it. Observed end-to-end through a watcher's interest on `local.jsonrpc` (the watcher
/// itself appears in its own initial snapshot, hence the second `client-2` add/remove on top).
TEST(JsonRpc, serverLifecycleTracksConnectAndDisconnect)
{
  DispatcherFixture f;
  const ConnectionId watcher {1U};
  const ConnectionId client {2U};

  f.connect(watcher);
  f.pushFrame(watcher,
              request("createInterest", 1, {{"interestName", "servers"}, {"query", "SELECT * FROM local.jsonrpc"}}));
  ASSERT_TRUE(popResponseFor(f, 1).contains("result"));

  // Initial snapshot carries the watcher itself - it's already on the bus by the time
  // `createInterest` attaches. Drain it before observing the next add.
  const auto initial = awaitNotification(f, "interestUpdate");
  EXPECT_EQ(initial["params"]["interestName"].get<std::string>(), "servers");
  ASSERT_FALSE(initial["params"]["added"].empty());
  EXPECT_EQ(initial["params"]["added"][0]["objectName"].get<std::string>(), "client-1");

  f.connect(client, "bob");
  const auto added = awaitNotification(f, "interestUpdate");
  EXPECT_EQ(added["params"]["interestName"].get<std::string>(), "servers");
  ASSERT_FALSE(added["params"]["added"].empty());
  EXPECT_EQ(added["params"]["added"][0]["objectName"].get<std::string>(), "client-2");
  EXPECT_EQ(added["params"]["added"][0]["qualifiedClassName"].get<std::string>(),
            "sen.components.jsonrpc.JsonRpcServer");

  f.disconnect(client);
  const auto removed = awaitNotification(f, "interestUpdate");
  EXPECT_EQ(removed["params"]["interestName"].get<std::string>(), "servers");
  ASSERT_FALSE(removed["params"]["removed"].empty());
  EXPECT_EQ(removed["params"]["removed"][0].get<std::string>(), "client-2");
}
