// === log_router_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "log_router.h"
#include "log_sink.h"

// google test
#include <gtest/gtest.h>

// spdlog
#include <spdlog/common.h>
#include <spdlog/logger.h>

// std
#include <memory>
#include <string>

namespace sen::components::term
{
namespace
{

TEST(LogRouterParseLevel, Trace)
{
  spdlog::level::level_enum level {};
  EXPECT_TRUE(LogRouter::parseLevel("trace", level));
  EXPECT_EQ(level, spdlog::level::trace);
}

TEST(LogRouterParseLevel, Debug)
{
  spdlog::level::level_enum level {};
  EXPECT_TRUE(LogRouter::parseLevel("debug", level));
  EXPECT_EQ(level, spdlog::level::debug);
}

TEST(LogRouterParseLevel, Info)
{
  spdlog::level::level_enum level {};
  EXPECT_TRUE(LogRouter::parseLevel("info", level));
  EXPECT_EQ(level, spdlog::level::info);
}

TEST(LogRouterParseLevel, WarnAndWarning)
{
  spdlog::level::level_enum level {};
  EXPECT_TRUE(LogRouter::parseLevel("warn", level));
  EXPECT_EQ(level, spdlog::level::warn);

  level = spdlog::level::off;
  EXPECT_TRUE(LogRouter::parseLevel("warning", level));
  EXPECT_EQ(level, spdlog::level::warn);
}

TEST(LogRouterParseLevel, ErrorAndErr)
{
  spdlog::level::level_enum level {};
  EXPECT_TRUE(LogRouter::parseLevel("error", level));
  EXPECT_EQ(level, spdlog::level::err);

  level = spdlog::level::off;
  EXPECT_TRUE(LogRouter::parseLevel("err", level));
  EXPECT_EQ(level, spdlog::level::err);
}

TEST(LogRouterParseLevel, Critical)
{
  spdlog::level::level_enum level {};
  EXPECT_TRUE(LogRouter::parseLevel("critical", level));
  EXPECT_EQ(level, spdlog::level::critical);
}

TEST(LogRouterParseLevel, Off)
{
  spdlog::level::level_enum level {};
  EXPECT_TRUE(LogRouter::parseLevel("off", level));
  EXPECT_EQ(level, spdlog::level::off);
}

TEST(LogRouterParseLevel, Unknown)
{
  spdlog::level::level_enum level = spdlog::level::info;
  EXPECT_FALSE(LogRouter::parseLevel("nonsense", level));
  // Out-param should not be modified on failure.
  EXPECT_EQ(level, spdlog::level::info);
}

TEST(LogRouterParseLevel, EmptyString)
{
  spdlog::level::level_enum level = spdlog::level::info;
  EXPECT_FALSE(LogRouter::parseLevel("", level));
  EXPECT_EQ(level, spdlog::level::info);
}

TEST(LogRouterParseLevel, CaseSensitive)
{
  // The current implementation is case-sensitive, assert that to lock the behaviour.
  spdlog::level::level_enum level = spdlog::level::info;
  EXPECT_FALSE(LogRouter::parseLevel("INFO", level));
  EXPECT_FALSE(LogRouter::parseLevel("Info", level));
  EXPECT_EQ(level, spdlog::level::info);
}

TEST(TermLogSink, ADetachedSinkStopsCallingBack)
{
  // A logger holds the sink through a shared_ptr, so it outlives the LogRouter whose state the callback
  // reaches into. The teardown detaches first, and this is what makes that enough: a logger the sweep
  // misses then calls nothing rather than a destroyed mutex and a destroyed vector.
  int calls = 0;
  auto sink = std::make_shared<TermLogSink>([&calls](spdlog::level::level_enum, const std::string&) { ++calls; });

  spdlog::logger logger("detach_test", sink);
  logger.set_level(spdlog::level::trace);

  logger.info("before");
  ASSERT_EQ(calls, 1) << "the sink never reached the callback, so the test proves nothing";

  sink->detach();
  logger.info("after");
  EXPECT_EQ(calls, 1) << "a detached sink still reached the callback";
}

}  // namespace
}  // namespace sen::components::term
