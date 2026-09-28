// === log_router_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "app.h"
#include "log_router.h"
#include "log_sink.h"

// spdlog
#include <spdlog/common.h>
#include <spdlog/logger.h>

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <memory>
#include <string>
#include <tuple>

namespace sen::components::term
{

/// Reaches the two bounds on the log queue. They are the interesting part of LogRouter and they cannot
/// be driven from outside: the producer runs on whichever thread logged, and the consumer renders into a
/// pane that needs a terminal. The App here is never initialised, which is safe as long as nothing is
/// rendered, so these tests use the queue and never `update()`.
class LogRouterTestAccess
{
public:
  explicit LogRouterTestAccess(LogRouter& router): router_(router) {}

  void enqueue(const std::string& text) { router_.enqueue(spdlog::level::info, text); }
  [[nodiscard]] std::size_t queued() const { return router_.pendingMessages_.size(); }
  [[nodiscard]] std::size_t droppedSoFar() const { return router_.droppedMessages_; }
  [[nodiscard]] LogRouter::Batch takeBatch() { return router_.takeBatch(); }

private:
  LogRouter& router_;
};

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

TEST(LogRouterParseLevel, CaseIsIgnored)
{
  // Every log line prints its level in upper case, so `log level INFO` is what a user types after reading
  // one, and it has to be accepted.
  spdlog::level::level_enum level = spdlog::level::trace;
  EXPECT_TRUE(LogRouter::parseLevel("INFO", level));
  EXPECT_EQ(level, spdlog::level::info);

  level = spdlog::level::trace;
  EXPECT_TRUE(LogRouter::parseLevel("Warn", level));
  EXPECT_EQ(level, spdlog::level::warn);

  level = spdlog::level::trace;
  EXPECT_TRUE(LogRouter::parseLevel("OFF", level));
  EXPECT_EQ(level, spdlog::level::off);
}

TEST(LogRouterParseLevel, AnUnknownNameIsStillRefusedWhateverTheCase)
{
  // Folding case must not turn every word into a level: the out-param is left alone on failure, and
  // that is the half a case-insensitive parse could break.
  spdlog::level::level_enum level = spdlog::level::info;
  EXPECT_FALSE(LogRouter::parseLevel("LOUD", level));
  EXPECT_FALSE(LogRouter::parseLevel("Verbose", level));
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

TEST(LogRouterQueue, TheProducerIsBoundedAndCountsWhatItRefuses)
{
  // `log level debug` on a kernel with live traffic makes the bus loggers emit on almost every
  // message. The queue had no cap, so memory climbed until the user who turned the level up to diagnose
  // something lost the tool they were diagnosing with.
  App app([](const std::string&) {});
  LogRouter router(app);
  LogRouterTestAccess access(router);

  constexpr std::size_t flood = 2500;
  for (std::size_t i = 0; i < flood; ++i)
  {
    access.enqueue("line " + std::to_string(i));
  }

  EXPECT_EQ(access.queued(), 2000U) << "the queue grew past its cap";
  EXPECT_EQ(access.droppedSoFar(), flood - 2000U) << "refused lines were not counted";
}

TEST(LogRouterQueue, OneTickTakesABudgetAndReportsTheDropsOnce)
{
  // Rendering everything that arrived built an element per line, so a burst became a frame longer than
  // the burst itself and the next tick started further behind.
  App app([](const std::string&) {});
  LogRouter router(app);
  LogRouterTestAccess access(router);

  for (std::size_t i = 0; i < 2500; ++i)
  {
    access.enqueue("line " + std::to_string(i));
  }

  auto first = access.takeBatch();
  EXPECT_EQ(first.messages.size(), 500U) << "a tick took more than its budget";
  EXPECT_EQ(first.dropped, 500U) << "the drop count was not handed over";
  EXPECT_EQ(access.queued(), 1500U) << "the rest should stay queued for the next tick";

  auto second = access.takeBatch();
  EXPECT_EQ(second.messages.size(), 500U);
  EXPECT_EQ(second.dropped, 0U) << "the drop count was reported twice";

  // And a queue shorter than the budget comes out whole.
  for (int i = 0; i < 3; ++i)
  {
    std::ignore = access.takeBatch();
  }
  EXPECT_EQ(access.queued(), 0U);
  EXPECT_EQ(access.takeBatch().messages.size(), 0U);
}

}  // namespace
}  // namespace sen::components::term
