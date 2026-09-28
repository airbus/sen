// === logger_sink_registry_test.cpp ===================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/component_api.h"

// google test
#include <gtest/gtest.h>

// spdlog
#include <spdlog/common.h>
#include <spdlog/details/console_globals.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/spdlog.h>

// The console sink family differs by platform, and consoleIsAudible below names both, so each side
// needs the header that declares its own. Unguarded, one of the two is dead weight on every build.
#ifdef _WIN32
#  include <spdlog/sinks/wincolor_sink.h>
#else
#  include <spdlog/sinks/ansicolor_sink.h>
#endif

// std
#include <memory>
#include <mutex>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace sen::kernel
{
namespace
{

/// Counts what reaches it, which is what a component rendering logs itself needs to know.
class CountingSink: public spdlog::sinks::base_sink<std::mutex>
{
public:
  [[nodiscard]] int count() const { return count_; }

  /// The last line as this sink's own formatter rendered it, which is how a test sees whether the
  /// pattern the sink was given is still the pattern in force. `base_sink::set_pattern` is final, so
  /// the pattern cannot be observed by intercepting the call.
  [[nodiscard]] std::string lastLine() const { return lastLine_; }

protected:
  void sink_it_(const spdlog::details::log_msg& msg) override
  {
    ++count_;
    spdlog::memory_buf_t buffer;
    formatter_->format(msg, buffer);
    lastLine_ = std::string(buffer.data(), buffer.size());
  }
  void flush_() override {}

private:
  int count_ = 0;
  std::string lastLine_;
};

/// Whether a console sink on this logger would still print. Claiming the terminal silences them rather
/// than detaching them, so the level is the question rather than presence.
///
/// Both sink families are checked. getOrCreateLogger builds `stdout_color_sink_mt`, which is the
/// wincolor family on Windows and the ansicolor family everywhere else, so a one-family helper reports
/// "no console sink" on Windows and aborts the test before it asserts anything.
bool consoleIsAudible(const std::shared_ptr<spdlog::logger>& logger)
{
  for (const auto& sink: logger->sinks())
  {
    auto* raw = sink.get();
    const bool isConsole =
#ifdef _WIN32
      dynamic_cast<spdlog::sinks::wincolor_sink<spdlog::details::console_mutex>*>(raw) != nullptr ||
      dynamic_cast<spdlog::sinks::wincolor_sink<spdlog::details::console_nullmutex>*>(raw) != nullptr;
#else
      dynamic_cast<spdlog::sinks::ansicolor_sink<spdlog::details::console_mutex>*>(raw) != nullptr ||
      dynamic_cast<spdlog::sinks::ansicolor_sink<spdlog::details::console_nullmutex>*>(raw) != nullptr;
#endif
    if (isConsole && sink->level() != spdlog::level::off)
    {
      return true;
    }
  }
  return false;
}

/// Each test uses its own logger names, because the registry outlives a test.
std::string uniqueName(const char* prefix)
{
  static int counter = 0;
  return std::string(prefix) + "_" + std::to_string(++counter);
}

class LoggerSinkRegistryTest: public ::testing::Test
{
protected:
  void SetUp() override
  {
    // Set the level as well as remember it. A logger made here inherits the registry's level, so
    // whatever the test before this one left there decides whether these loggers emit at all.
    levelOnEntry_ = KernelApi::getAllLoggersLevel();
    std::ignore = KernelApi::setAllLoggersLevel(spdlog::level::info);
  }

  void TearDown() override
  {
    for (const auto& sink: registered_)
    {
      std::ignore = KernelApi::removeLoggerSink(sink);
    }
    for (const auto& name: names_)
    {
      spdlog::drop(name);
    }
    // The registry outlives the test, so a level left behind silently changes every later case in this
    // binary, including the precondition of the console tests below.
    std::ignore = KernelApi::setAllLoggersLevel(levelOnEntry_);
  }

  std::shared_ptr<spdlog::logger> makeLogger(const char* prefix)
  {
    auto name = uniqueName(prefix);
    names_.push_back(name);
    return KernelApi::getOrCreateLogger(name);
  }

  void trackForRemoval(std::shared_ptr<spdlog::sinks::sink> sink) { registered_.push_back(std::move(sink)); }

private:
  std::vector<std::shared_ptr<spdlog::sinks::sink>> registered_;
  std::vector<std::string> names_;
  spdlog::level::level_enum levelOnEntry_ = spdlog::level::info;
};

TEST_F(LoggerSinkRegistryTest, AddedSinkReachesALoggerThatAlreadyExisted)
{
  auto before = makeLogger("existing");
  auto sink = std::make_shared<CountingSink>();
  trackForRemoval(sink);

  std::ignore = KernelApi::addLoggerSink(sink);

  before->info("one");
  EXPECT_EQ(sink->count(), 1) << "a logger made before the registration did not reach the sink";
}

TEST_F(LoggerSinkRegistryTest, AddedSinkReachesALoggerMadeAfterwards)
{
  auto sink = std::make_shared<CountingSink>();
  trackForRemoval(sink);
  std::ignore = KernelApi::addLoggerSink(sink);

  // The case the component could not cover on its own: it has no way to know this logger appeared.
  auto later = makeLogger("later");
  later->info("one");
  EXPECT_EQ(sink->count(), 1) << "a logger made after the registration did not reach the sink";
}

TEST_F(LoggerSinkRegistryTest, OwningTheTerminalSilencesConsoleSinksBothWays)
{
  auto before = makeLogger("owned_before");
  ASSERT_TRUE(consoleIsAudible(before)) << "getOrCreateLogger stopped giving new loggers a console sink";

  auto sink = std::make_shared<CountingSink>();
  trackForRemoval(sink);
  std::ignore = KernelApi::addLoggerSink(sink, KernelApi::TerminalOwnership::owned);

  EXPECT_FALSE(consoleIsAudible(before)) << "an existing logger kept writing to the terminal";

  auto after = makeLogger("owned_after");
  EXPECT_FALSE(consoleIsAudible(after)) << "a logger made later still writes to the terminal";
  after->info("one");
  EXPECT_EQ(sink->count(), 1) << "the owning component did not receive the line either";
}

TEST_F(LoggerSinkRegistryTest, ReleasingTheTerminalGivesTheConsoleBack)
{
  // Detaching console sinks without recording what they were would leave a kernel that outlives its
  // terminal component unable to print again.
  auto before = makeLogger("released_before");
  ASSERT_TRUE(consoleIsAudible(before));

  auto sink = std::make_shared<CountingSink>();
  std::ignore = KernelApi::addLoggerSink(sink, KernelApi::TerminalOwnership::owned);
  auto during = makeLogger("released_during");
  ASSERT_FALSE(consoleIsAudible(before)) << "nothing was silenced, so the restore below proves nothing";
  ASSERT_FALSE(consoleIsAudible(during));

  std::ignore = KernelApi::removeLoggerSink(sink);

  EXPECT_TRUE(consoleIsAudible(before)) << "a logger silenced before the claim stayed silent";
  EXPECT_TRUE(consoleIsAudible(during)) << "a logger made during the claim stayed silent";

  auto after = makeLogger("released_after");
  EXPECT_TRUE(consoleIsAudible(after)) << "a logger made after the release was silenced anyway";
}

TEST_F(LoggerSinkRegistryTest, ReAddingWithOwnedClaimsTheTerminal)
{
  // The ownership rides on the add call, so re-adding is the only way a component that registered early
  // can take the screen later.
  auto logger = makeLogger("reclaim");
  auto sink = std::make_shared<CountingSink>();
  trackForRemoval(sink);

  std::ignore = KernelApi::addLoggerSink(sink, KernelApi::TerminalOwnership::shared);
  ASSERT_TRUE(consoleIsAudible(logger)) << "a shared registration silenced the console";

  std::ignore = KernelApi::addLoggerSink(sink, KernelApi::TerminalOwnership::owned);
  EXPECT_FALSE(consoleIsAudible(logger)) << "re-adding with owned did not claim the terminal";
}

TEST_F(LoggerSinkRegistryTest, ARegisteredSinkKeepsItsOwnPattern)
{
  // A new logger takes the registry's formatter and pushes it down onto its sinks, which would replace a
  // component's pattern with the global one. The relay forwards the unformatted message instead.
  auto sink = std::make_shared<CountingSink>();
  trackForRemoval(sink);
  sink->set_pattern("component-pattern %v");
  std::ignore = KernelApi::addLoggerSink(sink);

  auto first = makeLogger("pattern_first");
  first->info("one");
  ASSERT_EQ(sink->count(), 1) << "nothing arrived, so the pattern below is not being exercised";

  // Creating another logger is what would clobber it.
  auto second = makeLogger("pattern_second");
  second->info("two");
  EXPECT_NE(sink->lastLine().find("component-pattern two"), std::string::npos)
    << "a new logger replaced the sink's pattern; the line came out as: " << sink->lastLine();
}

TEST_F(LoggerSinkRegistryTest, RemovingTakesTheSinkOffAndStopsNewLoggersGettingIt)
{
  auto sink = std::make_shared<CountingSink>();
  std::ignore = KernelApi::addLoggerSink(sink);
  auto held = makeLogger("removed");
  held->info("before removal");
  ASSERT_EQ(sink->count(), 1) << "the sink never received anything, so the removal below proves nothing";

  std::ignore = KernelApi::removeLoggerSink(sink);

  held->info("after removal");
  EXPECT_EQ(sink->count(), 1) << "a line reached the sink after it was removed";

  auto later = makeLogger("removed_later");
  later->info("after removal");
  EXPECT_EQ(sink->count(), 1) << "a logger made after removal still reached the sink";
}

TEST_F(LoggerSinkRegistryTest, AddingTheSameSinkTwiceDoesNotDoubleIt)
{
  auto sink = std::make_shared<CountingSink>();
  trackForRemoval(sink);
  std::ignore = KernelApi::addLoggerSink(sink);
  std::ignore = KernelApi::addLoggerSink(sink);

  auto logger = makeLogger("twice");
  logger->info("one");
  EXPECT_EQ(sink->count(), 1) << "the message arrived more than once, so the sink is attached twice";
}

TEST_F(LoggerSinkRegistryTest, RegisteringDoesNotTouchALoggersSinkVector)
{
  // This is the property the whole design rests on, so it is asserted rather than argued. spdlog walks
  // a logger's sinks with no lock, so appending to one while another thread emits through it is a
  // use-after-free, and a component cannot register before the threads start, because the kernel is
  // already logging by the time any component runs. Registering must therefore leave the vector alone.
  auto logger = makeLogger("untouched");
  const auto sinksBefore = logger->sinks();

  auto sink = std::make_shared<CountingSink>();
  trackForRemoval(sink);
  std::ignore = KernelApi::addLoggerSink(sink, KernelApi::TerminalOwnership::owned);

  EXPECT_EQ(logger->sinks(), sinksBefore) << "registering a sink changed an existing logger's sink vector";

  logger->info("one");
  EXPECT_EQ(sink->count(), 1) << "the line did not arrive, so the check above is vacuous";

  std::ignore = KernelApi::removeLoggerSink(sink);
  EXPECT_EQ(logger->sinks(), sinksBefore) << "removing a sink changed an existing logger's sink vector";
}

TEST_F(LoggerSinkRegistryTest, TheLevelAppliesToLoggersMadeAfterwards)
{
  // A walk over the registry could only reach what existed when it ran, so a level the user asked for
  // stopped applying the moment another component made a logger.
  std::ignore = KernelApi::setAllLoggersLevel(spdlog::level::warn);
  auto later = makeLogger("level_later");
  EXPECT_EQ(later->level(), spdlog::level::warn) << "a logger made later ignored the level";

  std::ignore = KernelApi::setAllLoggersLevel(spdlog::level::info);
  EXPECT_EQ(later->level(), spdlog::level::info) << "an existing logger ignored the level";
}

TEST_F(LoggerSinkRegistryTest, RegisteringSaysWhatItDid)
{
  // A void return would leave a component unable to learn whether its registration took, whether anything
  // was registered already, or who held the terminal. A component that renders logs needs all three,
  // because the registration is not arbitrated.
  auto first = std::make_shared<CountingSink>();
  auto report = KernelApi::addLoggerSink(first, KernelApi::TerminalOwnership::owned);
  ASSERT_TRUE(report.isOk()) << "a valid registration was refused";
  EXPECT_TRUE(report.getValue().added);
  EXPECT_TRUE(report.getValue().ownsTerminal);
  EXPECT_FALSE(report.getValue().terminalOwnedElsewhere) << "nothing else had claimed it";
  EXPECT_EQ(report.getValue().registeredSinks, 1U);

  // A second renderer is allowed in, and is told that someone else already owns the screen.
  auto second = std::make_shared<CountingSink>();
  auto secondReport = KernelApi::addLoggerSink(second, KernelApi::TerminalOwnership::owned);
  ASSERT_TRUE(secondReport.isOk());
  EXPECT_TRUE(secondReport.getValue().added);
  EXPECT_TRUE(secondReport.getValue().terminalOwnedElsewhere) << "the first sink's claim was not reported";
  EXPECT_EQ(secondReport.getValue().registeredSinks, 2U);

  // Re-adding reports that nothing new was registered.
  auto again = KernelApi::addLoggerSink(first, KernelApi::TerminalOwnership::owned);
  ASSERT_TRUE(again.isOk());
  EXPECT_FALSE(again.getValue().added) << "a sink already registered was reported as newly added";
  EXPECT_EQ(again.getValue().registeredSinks, 2U);

  std::ignore = KernelApi::removeLoggerSink(first);
  std::ignore = KernelApi::removeLoggerSink(second);
}

TEST_F(LoggerSinkRegistryTest, TheCallsThatCanFailReportIt)
{
  // A null sink was a silent no-op in both directions, and an out-of-range level went straight into
  // spdlog. All three said nothing either way, in a header whose every other once-only call returns a
  // FuncResult.
  EXPECT_TRUE(KernelApi::addLoggerSink(nullptr).isError()) << "a null sink was accepted";
  EXPECT_TRUE(KernelApi::removeLoggerSink(nullptr).isError()) << "unregistering nothing was accepted";
  EXPECT_TRUE(KernelApi::setAllLoggersLevel(static_cast<spdlog::level::level_enum>(99)).isError())
    << "a level outside the enum was accepted";

  // And removing a sink that was never registered is deliberately not an error.
  auto never = std::make_shared<CountingSink>();
  EXPECT_TRUE(KernelApi::removeLoggerSink(never).isOk()) << "removing an unregistered sink should be allowed";
}

}  // namespace
}  // namespace sen::kernel
