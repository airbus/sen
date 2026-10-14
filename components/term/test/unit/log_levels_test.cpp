// === log_levels_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

/// The level commands: what `log level` parses, which loggers it reaches, and what it refuses.

#include "log_levels.h"

// spdlog
#include <spdlog/common.h>

// google test
#include <gtest/gtest.h>

namespace sen::components::term
{
namespace
{

/// @test
/// Parses trace to the spdlog trace level.
TEST(LogLevels, Trace)
{
  spdlog::level::level_enum level {};
  EXPECT_TRUE(parseLevel("trace", level));
  EXPECT_EQ(level, spdlog::level::trace);
}

/// @test
/// Parses debug to the spdlog debug level.
TEST(LogLevels, Debug)
{
  spdlog::level::level_enum level {};
  EXPECT_TRUE(parseLevel("debug", level));
  EXPECT_EQ(level, spdlog::level::debug);
}

/// @test
/// Parses info to the spdlog info level.
TEST(LogLevels, Info)
{
  spdlog::level::level_enum level {};
  EXPECT_TRUE(parseLevel("info", level));
  EXPECT_EQ(level, spdlog::level::info);
}

/// @test
/// Parses both warn and warning to the same spdlog warn level.
TEST(LogLevels, WarnAndWarning)
{
  spdlog::level::level_enum level {};
  EXPECT_TRUE(parseLevel("warn", level));
  EXPECT_EQ(level, spdlog::level::warn);

  level = spdlog::level::off;
  EXPECT_TRUE(parseLevel("warning", level));
  EXPECT_EQ(level, spdlog::level::warn);
}

/// @test
/// Parses both error and err to the same spdlog err level.
TEST(LogLevels, ErrorAndErr)
{
  spdlog::level::level_enum level {};
  EXPECT_TRUE(parseLevel("error", level));
  EXPECT_EQ(level, spdlog::level::err);

  level = spdlog::level::off;
  EXPECT_TRUE(parseLevel("err", level));
  EXPECT_EQ(level, spdlog::level::err);
}

/// @test
/// Parses critical to the spdlog critical level.
TEST(LogLevels, Critical)
{
  spdlog::level::level_enum level {};
  EXPECT_TRUE(parseLevel("critical", level));
  EXPECT_EQ(level, spdlog::level::critical);
}

/// @test
/// Parses off to the spdlog off level.
TEST(LogLevels, Off)
{
  spdlog::level::level_enum level {};
  EXPECT_TRUE(parseLevel("off", level));
  EXPECT_EQ(level, spdlog::level::off);
}

/// @test
/// A name that is not a level is refused, and the level passed in is left alone.
TEST(LogLevels, Unknown)
{
  spdlog::level::level_enum level = spdlog::level::info;
  EXPECT_FALSE(parseLevel("nonsense", level));
  // Out-param should not be modified on failure.
  EXPECT_EQ(level, spdlog::level::info);
}

/// @test
/// An empty name is refused, and the level passed in is left alone.
TEST(LogLevels, EmptyString)
{
  spdlog::level::level_enum level = spdlog::level::info;
  EXPECT_FALSE(parseLevel("", level));
  EXPECT_EQ(level, spdlog::level::info);
}

/// @test
/// A level is accepted in any case, since log lines print the level in upper case.
TEST(LogLevels, CaseIsIgnored)
{
  // Every log line prints its level in upper case, so `log level INFO` is what a user types after reading
  // one, and it has to be accepted.
  spdlog::level::level_enum level = spdlog::level::trace;
  EXPECT_TRUE(parseLevel("INFO", level));
  EXPECT_EQ(level, spdlog::level::info);

  level = spdlog::level::trace;
  EXPECT_TRUE(parseLevel("Warn", level));
  EXPECT_EQ(level, spdlog::level::warn);

  level = spdlog::level::trace;
  EXPECT_TRUE(parseLevel("OFF", level));
  EXPECT_EQ(level, spdlog::level::off);
}

/// @test
/// Folding case does not make every word a level: an unknown name is still refused and the level
/// passed in is left alone.
TEST(LogLevels, AnUnknownNameIsStillRefusedWhateverTheCase)
{
  // Folding case must not turn every word into a level: the out-param is left alone on failure, and
  // that is the half a case-insensitive parse could break.
  spdlog::level::level_enum level = spdlog::level::info;
  EXPECT_FALSE(parseLevel("LOUD", level));
  EXPECT_FALSE(parseLevel("Verbose", level));
  EXPECT_EQ(level, spdlog::level::info);
}

}  // namespace
}  // namespace sen::components::term
