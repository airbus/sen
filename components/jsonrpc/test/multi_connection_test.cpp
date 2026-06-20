// === multi_connection_test.cpp =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Cross-connection contracts: per-connection state isolation and per-connection type-cache
// dedup. Uses `drainAllFramesOverSteps` instead of per-conn await helpers because frames from
// multiple ConnectionIds interleave.

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
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using sen::components::jsonrpc::ConnectionId;
using sen::components::jsonrpc::test::DispatcherFixture;
using sen::components::jsonrpc::test::drainAllFramesOverSteps;
using sen::components::jsonrpc::test::DrainedFrame;
using sen::components::jsonrpc::test::findDecodedPropertyValue;
using sen::components::jsonrpc::test::request;
using sen::components::jsonrpc::test::wildcardSelector;

/// @test
/// Same interest name on two connections doesn't collide; subscriptions are independent; a
/// disconnect tears down only that connection's state.
TEST(JsonRpc, multipleConnectionsHaveIndependentState)
{
  DispatcherFixture f;
  const ConnectionId conn1 {1U};
  const ConnectionId conn2 {2U};
  f.connect(conn1);
  f.connect(conn2);

  // Both connections create an interest of the same name with the same subscribe block.
  // No collision (interest names are scoped per connection).
  const nlohmann::json sharedParams {{"interestName", "shared"},
                                     {"query", "SELECT * FROM local.fixture"},
                                     {"subscribe", {{"properties", wildcardSelector()}}}};
  for (const auto& [conn, id]: std::vector<std::pair<ConnectionId, int>> {{conn1, 401}, {conn2, 402}})
  {
    f.pushFrame(conn, request("createInterest", id, sharedParams));
  }

  // Drain everything for both connections in one sweep. Each conn should see at least its
  // response and its interestUpdate.added (the latter carrying the snapshot).
  auto setupFrames = drainAllFramesOverSteps(f, 64);
  const auto countOn = [](const std::vector<DrainedFrame>& frames, ConnectionId conn)
  { return std::count_if(frames.begin(), frames.end(), [&](const auto& fr) { return fr.connId == conn; }); };
  EXPECT_GE(countOn(setupFrames, conn1), 2) << "conn1 should see at least response + interestUpdate";
  EXPECT_GE(countOn(setupFrames, conn2), 2) << "conn2 should see at least response + interestUpdate";

  // Mutate the widget; both subscriptions should fire independently.
  f.setWidgetCounter(91);
  const auto countMutationsOn = [](const std::vector<DrainedFrame>& frames, ConnectionId conn, int expectedValue)
  {
    return std::count_if(frames.begin(),
                         frames.end(),
                         [&](const auto& fr)
                         {
                           if (fr.connId != conn || fr.payload.value("method", "") != "propertyChanged")
                           {
                             return false;
                           }
                           const auto decoded = findDecodedPropertyValue(fr.payload["params"]["values"], "counter");
                           return decoded.has_value() && *decoded == expectedValue;
                         });
  };
  const auto mutationFrames = drainAllFramesOverSteps(f, 64);
  EXPECT_EQ(countMutationsOn(mutationFrames, conn1, 91), 1) << "conn1 should receive exactly one propertyChanged";
  EXPECT_EQ(countMutationsOn(mutationFrames, conn2, 91), 1) << "conn2 should receive exactly one propertyChanged";

  // Disconnect conn1 - its state (interest + guards) should be torn down. conn2 keeps working.
  f.disconnect(conn1);
  // Synchronously round-trip the dispatcher so the disconnect lands before we mutate again.
  // Without this, setWidgetCounter would step the kernel and the disconnect would be processed
  // in the same tick *after* the counter write, racing against the propertyChanged emission.
  f.awaitDispatcherDrained(conn2);

  f.setWidgetCounter(92);
  const auto afterDisconnect = drainAllFramesOverSteps(f, 32);
  // Some teardown notifications (interestUpdate.removed for the matched widget) may fire on
  // conn1 as the bus subscription destructs. The check that matters is that conn1 does NOT
  // receive the post-disconnect propertyChanged - its guard should already be unwired.
  EXPECT_EQ(countMutationsOn(afterDisconnect, conn1, 92), 0) << "conn1 should not see mutation 92 after disconnect";
  EXPECT_EQ(countMutationsOn(afterDisconnect, conn2, 92), 1) << "conn2 should still receive its propertyChanged";
}

/// @test
/// `knownTypes` dedup: the second `interestUpdate.added` for an object on the same connection
/// must not re-bundle a spec the first one already shipped.
TEST(JsonRpc, knownTypesAreNotResentOnSameConnection)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  f.connect(conn);

  // attachTo() may fire onAdded synchronously *before* ctx.respond() returns, so the
  // interestUpdate can land ahead of the response in the outbound queue. Drain everything in
  // one sweep to avoid `popResponseFor` consuming the notification on its way to the response.
  f.pushFrame(conn, request("createInterest", 501, {{"interestName", "i1"}, {"query", "SELECT * FROM local.fixture"}}));
  f.pushFrame(conn, request("createInterest", 502, {{"interestName", "i2"}, {"query", "SELECT * FROM local.fixture"}}));

  const auto frames = drainAllFramesOverSteps(f, 64);
  auto findUpdateFor = [&frames](std::string_view interest) -> nlohmann::json
  {
    for (const auto& f: frames)
    {
      if (f.payload.value("method", "") == "interestUpdate" &&
          f.payload["params"].value("interestName", "") == interest)
      {
        return f.payload;
      }
    }
    return {};
  };

  const auto iu1 = findUpdateFor("i1");
  const auto iu2 = findUpdateFor("i2");
  ASSERT_FALSE(iu1.is_null()) << "missing interestUpdate for i1";
  ASSERT_FALSE(iu2.is_null()) << "missing interestUpdate for i2";
  EXPECT_FALSE(iu1["params"]["types"].empty()) << iu1.dump();
  EXPECT_TRUE(iu2["params"]["types"].empty()) << iu2.dump();
}

/// @test
/// Default `createInterest` (no `withSchemas`) leaves `typeSchemas` empty in `interestUpdate`.
TEST(JsonRpc, interestUpdateOmitsTypeSchemasWhenNotRequested)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  f.connect(conn);
  f.pushFrame(conn, request("createInterest", 700, {{"interestName", "i1"}, {"query", "SELECT * FROM local.fixture"}}));

  const auto frames = drainAllFramesOverSteps(f, 64);
  for (const auto& fr: frames)
  {
    if (fr.payload.value("method", "") == "interestUpdate")
    {
      EXPECT_TRUE(fr.payload["params"]["typeSchemas"].get<std::string>().empty()) << fr.payload.dump();
    }
  }
}

/// @test
/// `createInterest` with `withSchemas=true` populates `typeSchemas` for every newly-shipped
/// type in the `interestUpdate.added` batch. Schemas are JSON-encoded as a string field on the
/// wire; decoded form is a map keyed by qualified name.
TEST(JsonRpc, interestUpdateShipsSchemasWhenOptedIn)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  f.connect(conn);
  f.pushFrame(conn,
              request("createInterest",
                      701,
                      {{"interestName", "i1"}, {"query", "SELECT * FROM local.fixture"}, {"withSchemas", true}}));

  const auto frames = drainAllFramesOverSteps(f, 64);
  nlohmann::json iu;
  for (const auto& fr: frames)
  {
    if (fr.payload.value("method", "") == "interestUpdate")
    {
      iu = fr.payload;
      break;
    }
  }
  ASSERT_FALSE(iu.is_null()) << "missing interestUpdate";
  const auto& schemasStr = iu["params"]["typeSchemas"].get<std::string>();
  ASSERT_FALSE(schemasStr.empty()) << iu.dump();

  const auto schemas = nlohmann::json::parse(schemasStr);
  ASSERT_TRUE(schemas.is_object());
  EXPECT_TRUE(schemas.contains("sen.components.jsonrpc.test.FixtureWidget")) << schemasStr;
}

/// @test
/// `knownSchemas` dedup: a second interest with `withSchemas=true` on the same connection that
/// references the same types as the first must not re-ship their schemas.
TEST(JsonRpc, knownSchemasAreNotResentOnSameConnection)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  f.connect(conn);
  f.pushFrame(
    conn,
    request(
      "createInterest", 710, {{"interestName", "a"}, {"query", "SELECT * FROM local.fixture"}, {"withSchemas", true}}));
  f.pushFrame(
    conn,
    request(
      "createInterest", 711, {{"interestName", "b"}, {"query", "SELECT * FROM local.fixture"}, {"withSchemas", true}}));

  const auto frames = drainAllFramesOverSteps(f, 64);
  auto findUpdateFor = [&frames](std::string_view interest) -> nlohmann::json
  {
    for (const auto& fr: frames)
    {
      if (fr.payload.value("method", "") == "interestUpdate" &&
          fr.payload["params"].value("interestName", "") == interest)
      {
        return fr.payload;
      }
    }
    return {};
  };
  const auto a = findUpdateFor("a");
  const auto b = findUpdateFor("b");
  ASSERT_FALSE(a.is_null());
  ASSERT_FALSE(b.is_null());
  EXPECT_FALSE(a["params"]["typeSchemas"].get<std::string>().empty()) << a.dump();
  EXPECT_TRUE(b["params"]["typeSchemas"].get<std::string>().empty()) << b.dump();
}

/// @test
/// `getType(qname, withSchema=true)` returns the schema fragment as a non-empty string.
TEST(JsonRpc, getTypeWithSchemaReturnsFragment)
{
  DispatcherFixture f;
  const ConnectionId conn {1U};
  f.connect(conn);
  f.pushFrame(
    conn,
    request("getType", 720, {{"qualifiedName", "sen.components.jsonrpc.test.FixtureWidget"}, {"withSchema", true}}));
  const auto resp = popResponseFor(f, 720);
  ASSERT_TRUE(resp.contains("result")) << resp.dump();
  const auto& schema = resp["result"]["schema"].get<std::string>();
  EXPECT_FALSE(schema.empty()) << resp.dump();
  EXPECT_NE(schema.find("FixtureWidget"), std::string::npos) << schema;
}

/// @test
/// Reproduces the TS-suite churn pattern: open a connection, declare an interest, expect the
/// existing widget to land in `interestUpdate.added`, disconnect; repeat N times with fresh
/// ConnectionIds. Every iteration must see the widget. Failure shape: at iteration K the
/// snapshot `added` array is empty, matching `objects=0, state=active` in the TS suite.
TEST(JsonRpc, sequentialConnectChurnAlwaysSeesExistingObject)
{
  DispatcherFixture f;
  constexpr std::size_t kIterations = 16U;

  for (std::size_t i = 0; i < kIterations; ++i)
  {
    const ConnectionId conn {static_cast<uint64_t>(100U + i)};
    f.connect(conn);
    f.pushFrame(conn,
                request("createInterest",
                        static_cast<int>(700 + i),
                        {{"interestName", "churn"}, {"query", "SELECT * FROM local.fixture"}}));

    const auto frames = drainAllFramesOverSteps(f, 16);
    bool sawWidget = false;
    for (const auto& fr: frames)
    {
      if (fr.connId != conn)
      {
        continue;
      }
      if (fr.payload.value("method", "") != "interestUpdate")
      {
        continue;
      }
      const auto& added = fr.payload["params"]["added"];
      for (const auto& entry: added)
      {
        if (entry.value("objectName", "") == DispatcherFixture::widgetName)
        {
          sawWidget = true;
          break;
        }
      }
      if (sawWidget)
      {
        break;
      }
    }
    EXPECT_TRUE(sawWidget) << "iteration " << i << " (conn=" << conn.get()
                           << ") did not see the existing widget snapshot";

    f.disconnect(conn);
    // Let the disconnect land before the next iteration's connect.
    f.step(2);
  }
}
