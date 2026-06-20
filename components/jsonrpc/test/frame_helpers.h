// === frame_helpers.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_JSONRPC_TEST_FRAME_HELPERS_H
#define SEN_COMPONENTS_JSONRPC_TEST_FRAME_HELPERS_H

// component
#include "dispatcher_fixture.h"
#include "messages.h"

// nlohmann
#include "nlohmann/json.hpp"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sen::components::jsonrpc::test
{

/// JSON-RPC 2.0 request envelope serialized for `pushFrame`.
[[nodiscard]] inline std::string request(std::string_view method,
                                         int id,
                                         nlohmann::json params = nlohmann::json::object())
{
  return nlohmann::json {{"jsonrpc", "2.0"}, {"method", method}, {"params", std::move(params)}, {"id", id}}.dump();
}

/// JSON-encodes `value` so it can be embedded in a request field whose STL type is `string`
/// (`setProperty.value`, `invoke.argsJson`). The dispatcher parses the inner string back into a
/// typed value before handing it to the kernel; the wire-level type stays a string per STL.
[[nodiscard]] inline std::string encoded(const nlohmann::json& value) { return value.dump(); }

/// `MemberSelector::WildcardSelection` wire shape.
[[nodiscard]] inline nlohmann::json wildcardSelector()
{
  return {{"type", "WildcardSelection"}, {"value", nlohmann::json::object()}};
}

/// `MemberSelector::NamedSelection` wire shape.
[[nodiscard]] inline nlohmann::json namedSelector(std::initializer_list<std::string> memberNames)
{
  return {{"type", "NamedSelection"}, {"value", {{"memberNames", memberNames}}}};
}

/// Cap on kernel ticks any "wait for the next interesting frame" helper will step. Most paths
/// resolve in one tick; 1024 is slack for multi-tick fan-outs while still failing fast.
inline constexpr std::size_t maxStepsPerAwait = 1024;

/// Pops frames until `matches` returns true; non-matches are stashed and pushed back so later
/// helpers can still see them. The peek buffer is not connection-partitioned, so a test that
/// filters by connection id should drain the buffer between scenarios to avoid cross-talk.
template <typename Predicate>
[[nodiscard]] inline nlohmann::json popMatching(DispatcherFixture& f,
                                                std::string_view contextLabel,
                                                Predicate&& matches)
{
  std::vector<OutboundMessage> stash;
  std::optional<nlohmann::json> hit;
  for (std::size_t i = 0; !hit.has_value() && i <= maxStepsPerAwait; ++i)
  {
    auto raw = f.tryPopRaw();
    if (!raw.has_value())
    {
      if (i < maxStepsPerAwait)
      {
        f.step();
      }
      continue;
    }
    auto frame = nlohmann::json::parse(raw->payload);
    if (matches(frame))
    {
      hit = std::move(frame);
    }
    else
    {
      stash.push_back(std::move(*raw));
    }
  }
  // Put back non-matches even on timeout so the next helper still sees them.
  for (auto& msg: stash)
  {
    f.pushBackRaw(std::move(msg));
  }
  if (!hit.has_value())
  {
    throw std::runtime_error(std::string {contextLabel});
  }
  return std::move(*hit);
}

/// Drains the outbound queue until the response with `expectedId` arrives.
[[nodiscard]] inline nlohmann::json popResponseFor(DispatcherFixture& f, int expectedId)
{
  return popMatching(f,
                     "popResponseFor: timed out waiting for id " + std::to_string(expectedId),
                     [expectedId](const nlohmann::json& frame)
                     { return frame.contains("id") && frame["id"] == expectedId; });
}

/// Drains until an `interestUpdate` carrying at least one added object; returns that name.
[[nodiscard]] inline std::string awaitFirstAddedObjectName(DispatcherFixture& f)
{
  const auto frame = popMatching(
    f,
    "awaitFirstAddedObjectName: timed out",
    [](const nlohmann::json& f) { return f.value("method", "") == "interestUpdate" && !f["params"]["added"].empty(); });
  return frame["params"]["added"][0]["objectName"].get<std::string>();
}

/// Drains until a notification whose method matches `method` arrives; throws on timeout.
[[nodiscard]] inline nlohmann::json awaitNotification(DispatcherFixture& f, std::string_view method)
{
  return popMatching(f,
                     "awaitNotification: timed out waiting for " + std::string {method},
                     [method](const nlohmann::json& frame) { return frame.value("method", "") == method; });
}

/// Looks up the JSON-encoded value string for `propertyName` in a `PropertyValueList` payload
/// (the wire shape of `propertyChanged.values` and `interestUpdate.added.currentValues` per the
/// STL: an array of `{propertyName, value: string}` pairs). Returns `std::nullopt` if absent.
[[nodiscard]] inline std::optional<std::string> findPropertyValue(const nlohmann::json& list,
                                                                  std::string_view propertyName)
{
  if (!list.is_array())
  {
    return std::nullopt;
  }
  for (const auto& entry: list)
  {
    if (entry.value("propertyName", std::string {}) == propertyName)
    {
      return entry.value("value", std::string {});
    }
  }
  return std::nullopt;
}

/// Same as `findPropertyValue` but parses the encoded string as JSON. Most assertions want the
/// typed value, not the raw string.
[[nodiscard]] inline std::optional<nlohmann::json> findDecodedPropertyValue(const nlohmann::json& list,
                                                                            std::string_view propertyName)
{
  auto encoded = findPropertyValue(list, propertyName);
  if (!encoded.has_value())
  {
    return std::nullopt;
  }
  return nlohmann::json::parse(*encoded);
}

/// Decode the JSON-encoded string in an `eventTriggered` notification's `args` field. STL declares
/// `eventTriggered.args : string`, so the wire carries the positional argument array as an
/// encoded string rather than raw JSON.
[[nodiscard]] inline nlohmann::json decodeEventArgs(const nlohmann::json& note)
{
  return nlohmann::json::parse(note["params"]["args"].get<std::string>());
}

/// Drains `propertyChanged` until one carries `propertyName: expectedValue` in `values`.
[[nodiscard]] inline nlohmann::json awaitPropertyValue(DispatcherFixture& f,
                                                       std::string_view propertyName,
                                                       const nlohmann::json& expectedValue)
{
  return popMatching(f,
                     "awaitPropertyValue: timed out waiting for " + std::string {propertyName},
                     [propertyName, &expectedValue](const nlohmann::json& frame)
                     {
                       if (frame.value("method", "") != "propertyChanged")
                       {
                         return false;
                       }
                       const auto decoded = findDecodedPropertyValue(frame["params"]["values"], propertyName);
                       return decoded.has_value() && *decoded == expectedValue;
                     });
}

/// Drains every outbound message produced over the next `steps` kernel ticks. Single-pass so
/// multi-connection tests get both connections in one go.
struct DrainedFrame
{
  ConnectionId connId;
  nlohmann::json payload;
};

[[nodiscard]] inline std::vector<DrainedFrame> drainAllFramesOverSteps(DispatcherFixture& f, std::size_t steps)
{
  std::vector<DrainedFrame> out;
  const auto consume = [&out](DispatcherFixture& fixture)
  {
    while (auto raw = fixture.tryPopRaw())
    {
      out.push_back({raw->connectionId, nlohmann::json::parse(raw->payload)});
    }
  };
  consume(f);
  for (std::size_t i = 0; i < steps; ++i)
  {
    f.step();
    consume(f);
  }
  return out;
}

/// True iff a follow-up `steps` kernel ticks produce no new outbound frames. Drains the buffer
/// first so the assertion measures *new* output, not stashed frames left by helpers above.
[[nodiscard]] inline bool noFurtherFrames(DispatcherFixture& f, std::size_t steps)
{
  std::ignore = f.drainNow();
  for (std::size_t i = 0; i < steps; ++i)
  {
    f.step();
    if (f.tryPopRaw().has_value())
    {
      return false;
    }
  }
  return true;
}

/// Connects, creates `interestName` on `local.fixture`, and waits for the bundled widget to land
/// in the first `interestUpdate.added`.
inline void primeFixtureInterest(DispatcherFixture& f, ConnectionId connId, std::string_view interestName)
{
  f.connect(connId);
  f.pushFrame(connId,
              request("createInterest", 1, {{"interestName", interestName}, {"query", "SELECT * FROM local.fixture"}}));
  const auto resp = popResponseFor(f, 1);
  ASSERT_TRUE(resp.contains("result")) << resp.dump();
  ASSERT_EQ(awaitFirstAddedObjectName(f), DispatcherFixture::widgetName);
}

}  // namespace sen::components::jsonrpc::test

#endif  // SEN_COMPONENTS_JSONRPC_TEST_FRAME_HELPERS_H
