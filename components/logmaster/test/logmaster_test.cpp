// === logmaster_test.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "logger_impl.h"

// sen
#include "stl/sen/kernel/log.stl.h"

// spdlog
#include <spdlog/common.h>

// google test
#include <gtest/gtest.h>

namespace logmaster::test
{

using sen::components::logmaster::LoggerImpl;
using SenLevel = sen::kernel::log::LogLevel;

/// @test
/// Every level Sen names maps to the spdlog level of the same name, so asking for one level does
/// not silently set another.
TEST(LogLevelMapping, SenLevelsMapToTheirSpdlogCounterpart)
{
  EXPECT_EQ(LoggerImpl::mapLogLevel(SenLevel::trace), spdlog::level::trace);
  EXPECT_EQ(LoggerImpl::mapLogLevel(SenLevel::debug), spdlog::level::debug);
  EXPECT_EQ(LoggerImpl::mapLogLevel(SenLevel::info), spdlog::level::info);
  EXPECT_EQ(LoggerImpl::mapLogLevel(SenLevel::warn), spdlog::level::warn);
  EXPECT_EQ(LoggerImpl::mapLogLevel(SenLevel::err), spdlog::level::err);
  EXPECT_EQ(LoggerImpl::mapLogLevel(SenLevel::critical), spdlog::level::critical);
  EXPECT_EQ(LoggerImpl::mapLogLevel(SenLevel::off), spdlog::level::off);
}

/// @test
/// Every spdlog level maps back to the Sen level of the same name, which is what a logger reports
/// when it is first discovered.
TEST(LogLevelMapping, SpdlogLevelsMapBackToTheirSenCounterpart)
{
  EXPECT_EQ(LoggerImpl::mapLogLevel(spdlog::level::trace), SenLevel::trace);
  EXPECT_EQ(LoggerImpl::mapLogLevel(spdlog::level::debug), SenLevel::debug);
  EXPECT_EQ(LoggerImpl::mapLogLevel(spdlog::level::info), SenLevel::info);
  EXPECT_EQ(LoggerImpl::mapLogLevel(spdlog::level::warn), SenLevel::warn);
  EXPECT_EQ(LoggerImpl::mapLogLevel(spdlog::level::err), SenLevel::err);
  EXPECT_EQ(LoggerImpl::mapLogLevel(spdlog::level::critical), SenLevel::critical);
  EXPECT_EQ(LoggerImpl::mapLogLevel(spdlog::level::off), SenLevel::off);
}

/// @test
/// A level neither side names falls back to off rather than to whatever the enum value happens to
/// be, so an spdlog that grows a level does not quietly turn logging up.
TEST(LogLevelMapping, AnUnknownLevelBecomesOff)
{
  EXPECT_EQ(LoggerImpl::mapLogLevel(static_cast<spdlog::level::level_enum>(spdlog::level::n_levels)), SenLevel::off);
  EXPECT_EQ(LoggerImpl::mapLogLevel(static_cast<SenLevel>(99)), spdlog::level::off);
}

}  // namespace logmaster::test
