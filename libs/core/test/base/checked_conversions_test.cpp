// === checked_conversions_test.cpp ====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/checked_conversions.h"

// gtest
#include <gtest/gtest.h>

// std
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>

namespace
{
void triggerNegativeOverflowAssertion() { std::ignore = sen::std_util::checkedConversion<uint8_t>(-1); }

void triggerPositiveOverflowAssertion() { std::ignore = sen::std_util::checkedConversion<int8_t>(1000); }

void triggerFloatToIntPositiveOverflow() { std::ignore = sen::std_util::checkedConversion<int8_t>(1000.0); }

void triggerFloatToIntNegativeOverflow() { std::ignore = sen::std_util::checkedConversion<uint32_t>(-1.0); }

void triggerDoubleToFloatPositiveOverflow() { std::ignore = sen::std_util::checkedConversion<float>(1.0e300); }

void triggerDoubleToFloatNegativeOverflow() { std::ignore = sen::std_util::checkedConversion<float>(-1.0e300); }

}  // namespace

/// @test
/// Verifies that converting to bool works correctly for zero, positive, negative, and floating point representations
/// @requirements(SEN-575)
TEST(CheckedConversionsTest, ConvertsToBoolCorrectly)
{
  EXPECT_FALSE(sen::std_util::ignoredLossyConversion<bool>(0));
  EXPECT_FALSE(sen::std_util::ignoredLossyConversion<bool>(0.0));
  EXPECT_FALSE(sen::std_util::ignoredLossyConversion<bool>(0.0f));

  EXPECT_TRUE(sen::std_util::ignoredLossyConversion<bool>(1));
  EXPECT_TRUE(sen::std_util::ignoredLossyConversion<bool>(-1));
  EXPECT_TRUE(sen::std_util::ignoredLossyConversion<bool>(3.14));
  EXPECT_TRUE(sen::std_util::ignoredLossyConversion<bool>(std::numeric_limits<double>::max()));
  EXPECT_TRUE(sen::std_util::ignoredLossyConversion<bool>(std::numeric_limits<int64_t>::lowest()));
}

/// @test
/// Verifies float to float conversions, testing within bounds mapping and hard boundaries for max and lowest truncation
/// @requirements(SEN-1054, SEN-575)
TEST(CheckedConversionsTest, FloatingPointToFloatingPoint)
{
  EXPECT_DOUBLE_EQ(sen::std_util::ignoredLossyConversion<double>(3.14f), 3.14f);
  EXPECT_FLOAT_EQ(sen::std_util::ignoredLossyConversion<float>(std::numeric_limits<float>::max()),
                  std::numeric_limits<float>::max());
  EXPECT_FLOAT_EQ(sen::std_util::ignoredLossyConversion<float>(std::numeric_limits<float>::lowest()),
                  std::numeric_limits<float>::lowest());
  EXPECT_FLOAT_EQ(sen::std_util::ignoredLossyConversion<float>(std::numeric_limits<float>::min()),
                  std::numeric_limits<float>::min());

  constexpr double hugeDouble = 1.0e300;
  EXPECT_FLOAT_EQ(sen::std_util::ignoredLossyConversion<float>(hugeDouble), std::numeric_limits<float>::max());

  constexpr double hugeNegativeDouble = -1.0e300;
  EXPECT_FLOAT_EQ(sen::std_util::ignoredLossyConversion<float>(hugeNegativeDouble),
                  std::numeric_limits<float>::lowest());
}

/// @test
/// Verifies cross-boundary integer conversions, testing signed and unsigned mismatching and hard value clamps
/// @requirements(SEN-1054, SEN-575)
TEST(CheckedConversionsTest, IntegerToIntegerTruncation)
{
  EXPECT_EQ(sen::std_util::ignoredLossyConversion<int8_t>(int32_t {100}), 100);
  EXPECT_EQ(sen::std_util::ignoredLossyConversion<int8_t>(int32_t {1000}), std::numeric_limits<int8_t>::max());
  EXPECT_EQ(sen::std_util::ignoredLossyConversion<int8_t>(int32_t {-1000}), std::numeric_limits<int8_t>::lowest());

  EXPECT_EQ(sen::std_util::ignoredLossyConversion<uint8_t>(uint32_t {100}), 100);
  EXPECT_EQ(sen::std_util::ignoredLossyConversion<uint8_t>(uint32_t {1000}), std::numeric_limits<uint8_t>::max());

  EXPECT_EQ(sen::std_util::ignoredLossyConversion<uint8_t>(int32_t {100}), 100);
  EXPECT_EQ(sen::std_util::ignoredLossyConversion<uint8_t>(int32_t {-1}), std::numeric_limits<uint8_t>::lowest());
  EXPECT_EQ(sen::std_util::ignoredLossyConversion<uint8_t>(int32_t {-1000}), std::numeric_limits<uint8_t>::lowest());
  EXPECT_EQ(sen::std_util::ignoredLossyConversion<uint8_t>(int32_t {1000}), std::numeric_limits<uint8_t>::max());

  EXPECT_EQ(sen::std_util::ignoredLossyConversion<int8_t>(uint32_t {100}), 100);
  EXPECT_EQ(sen::std_util::ignoredLossyConversion<int8_t>(uint32_t {1000}), std::numeric_limits<int8_t>::max());
}

/// @test
/// Verifies floating point conversion to integers, checking normal bounds and extreme cuts
/// @requirements(SEN-1054, SEN-575)
TEST(CheckedConversionsTest, FloatingPointToIntegerTruncation)
{
  EXPECT_EQ(sen::std_util::ignoredLossyConversion<int32_t>(42.0), 42);
  EXPECT_EQ(sen::std_util::ignoredLossyConversion<int32_t>(1.0e12), std::numeric_limits<int32_t>::max());
  EXPECT_EQ(sen::std_util::ignoredLossyConversion<int32_t>(-1.0e12), std::numeric_limits<int32_t>::lowest());

  EXPECT_EQ(sen::std_util::ignoredLossyConversion<uint32_t>(42.0), 42);
  EXPECT_EQ(sen::std_util::ignoredLossyConversion<uint32_t>(-1.5), std::numeric_limits<uint32_t>::lowest());
  EXPECT_EQ(sen::std_util::ignoredLossyConversion<uint32_t>(1.0e12), std::numeric_limits<uint32_t>::max());
}

/// @test
/// Verifies handling of NaN values during floating point to integer conversions
/// @requirements(SEN-1054, SEN-575)
TEST(CheckedConversionsTest, FloatingPointNaNToInteger)
{
  constexpr double nanValue = std::numeric_limits<double>::quiet_NaN();
  std::ignore = sen::std_util::ignoredLossyConversion<int32_t>(nanValue);
}

/// @test
/// Verifies integers map correctly to floating point without clamping
/// @requirements(SEN-1054, SEN-575)
TEST(CheckedConversionsTest, IntegerToFloatingPoint)
{
  EXPECT_FLOAT_EQ(sen::std_util::ignoredLossyConversion<float>(42), 42.0f);
  EXPECT_DOUBLE_EQ(sen::std_util::ignoredLossyConversion<double>(-42), -42.0);
  EXPECT_FLOAT_EQ(sen::std_util::ignoredLossyConversion<float>(std::numeric_limits<int64_t>::max()),
                  static_cast<float>(std::numeric_limits<int64_t>::max()));
}

/// @test
/// Verifies that checkedConversion behaves correctly and returns expected values when no bounds are exceeded
/// @requirements(SEN-1054, SEN-575)
TEST(CheckedConversionsTest, CheckedConversionSuccessWithinBounds)
{
  EXPECT_EQ(sen::std_util::checkedConversion<int32_t>(int8_t {42}), 42);
  EXPECT_EQ(sen::std_util::checkedConversion<uint32_t>(uint8_t {255}), 255);
  EXPECT_FLOAT_EQ(sen::std_util::checkedConversion<float>(42), 42.0f);
  EXPECT_DOUBLE_EQ(sen::std_util::checkedConversion<double>(3.14f), 3.14f);
}

/// @test
/// Verifies that the assertion policy successfully terminates execution on negative integer overflow
/// @requirements(SEN-1054)
TEST(CheckedConversionsDeathTest, ReportPolicyAssertionTerminatesOnNegative)
{
  EXPECT_DEATH(triggerNegativeOverflowAssertion(), "Needed to truncate `from` as it's value was to small for ToType.");
}

/// @test
/// Verifies that the assertion policy successfully terminates execution on positive integer overflow
/// @requirements(SEN-1054)
TEST(CheckedConversionsDeathTest, ReportPolicyAssertionTerminatesOnPositive)
{
  EXPECT_DEATH(triggerPositiveOverflowAssertion(), "Needed to truncate `from` as it's value was to big for ToType.");
}

/// @test
/// Verifies that the assertion policy successfully terminates execution on floating point to integer truncation
/// @requirements(SEN-1054)
TEST(CheckedConversionsDeathTest, ReportPolicyAssertionTerminatesOnFloatToIntTruncation)
{
  EXPECT_DEATH(triggerFloatToIntPositiveOverflow(), "Needed to truncate `from` as it's value was to big for ToType.");
  EXPECT_DEATH(triggerFloatToIntNegativeOverflow(), "Needed to truncate `from` as it's value was to small for ToType.");
}

/// @test
/// Verifies that the assertion policy successfully terminates execution on double to float truncation
/// @requirements(SEN-1054)
TEST(CheckedConversionsDeathTest, ReportPolicyAssertionTerminatesOnDoubleToFloatTruncation)
{
  EXPECT_DEATH(triggerDoubleToFloatPositiveOverflow(),
               "Needed to truncate `from` as it's value was to big for ToType.");
  EXPECT_DEATH(triggerDoubleToFloatNegativeOverflow(),
               "Needed to truncate `from` as it's value was to small for ToType.");
}

/// @test
/// Verifies that the ignore policy handles truncation silently without emitting any output
/// @requirements(SEN-1054)
TEST(CheckedConversionsTest, ReportPolicyIgnoreIsSilent)
{
  testing::internal::CaptureStdout();
  testing::internal::CaptureStderr();

  std::ignore = sen::std_util::checkedConversion<uint8_t, sen::std_util::ReportPolicyIgnore>(-1);

  const std::string stdoutOutput = testing::internal::GetCapturedStdout();
  const std::string stderrOutput = testing::internal::GetCapturedStderr();

  EXPECT_TRUE(stdoutOutput.empty());
  EXPECT_TRUE(stderrOutput.empty());
}

/// @test
/// Verifies that the log policy writes a truncation warning message without aborting execution
/// @requirements(SEN-1054)
TEST(CheckedConversionsTest, ReportPolicyLogWritesWarning)
{
  testing::internal::CaptureStdout();

  const auto result = sen::std_util::checkedConversion<uint8_t, sen::std_util::ReportPolicyLog>(-1);
  const std::string output = testing::internal::GetCapturedStdout();

  EXPECT_EQ(result, 0);
  EXPECT_FALSE(output.empty());
  EXPECT_NE(output.find("Needed to truncate"), std::string::npos);
}

/// @test
/// Verifies that the trace policy emits a truncation trace message without aborting execution
/// @requirements(SEN-1054)
TEST(CheckedConversionsTest, ReportPolicyTraceEmitsMessage)
{
  testing::internal::CaptureStderr();

  const auto result = sen::std_util::checkedConversion<uint8_t, sen::std_util::ReportPolicyTrace>(-1);
  const std::string output = testing::internal::GetCapturedStderr();

  EXPECT_EQ(result, 0);
  EXPECT_TRUE(output.find("Needed to truncate") != std::string::npos);
}
