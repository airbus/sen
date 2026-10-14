// === logger_levels_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

/// The logging surface the kernel offers a component: levels.
///
/// Both cases are about what a logger emits, which capturing a descriptor cannot change from
/// outside.

#include "sen/kernel/component_api.h"

// google test
#include <gtest/gtest.h>

// spdlog
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

// std
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace sen::kernel
{
namespace
{

/// Each test uses its own logger names, because the registry outlives a test.
std::string uniqueName(const char* prefix)
{
  static int counter = 0;
  return std::string(prefix) + "_" + std::to_string(++counter);
}

class LoggerLevelsTest: public ::testing::Test
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
    for (const auto& name: names_)
    {
      spdlog::drop(name);
    }
    // The registry outlives the test, so a level left behind silently changes every later case in
    // this binary.
    std::ignore = KernelApi::setAllLoggersLevel(levelOnEntry_);
  }

  std::shared_ptr<spdlog::logger> makeLogger(const char* prefix)
  {
    auto name = uniqueName(prefix);
    names_.push_back(name);
    return KernelApi::getOrCreateLogger(name);
  }

private:
  std::vector<std::string> names_;
  spdlog::level::level_enum levelOnEntry_ = spdlog::level::info;
};

/// @test
/// setAllLoggersLevel applies to loggers made after the call as well as to those that exist.
TEST_F(LoggerLevelsTest, TheLevelAppliesToLoggersMadeAfterwards)
{
  // A walk over the registry could only reach what existed when it ran, so a level the user asked for
  // stopped applying the moment another component made a logger.
  std::ignore = KernelApi::setAllLoggersLevel(spdlog::level::warn);
  auto later = makeLogger("level_later");
  EXPECT_EQ(later->level(), spdlog::level::warn) << "a logger made later ignored the level";

  std::ignore = KernelApi::setAllLoggersLevel(spdlog::level::info);
  EXPECT_EQ(later->level(), spdlog::level::info) << "an existing logger ignored the level";
}

/// @test
/// A level outside the enum is refused rather than passed to spdlog.
TEST_F(LoggerLevelsTest, ALevelOutsideTheEnumIsRefused)
{
  // A FuncResult, like every other once-only call in that header: spdlog takes the value without
  // complaint, so the refusal has to come from here.
  EXPECT_TRUE(KernelApi::setAllLoggersLevel(static_cast<spdlog::level::level_enum>(99)).isError())
    << "a level outside the enum was accepted";
}

}  // namespace
}  // namespace sen::kernel
