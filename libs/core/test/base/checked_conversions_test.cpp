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
#include <exception>
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
/// Converting to bool yields false for zero in integer and floating point form, and true for any
/// non-zero value, negative and extreme ones included.
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
/// A float value survives the trip to double, the float limits pass through unchanged, and a
/// double beyond float range clamps to float's max or lowest.
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
/// An integer that fits the narrower type passes unchanged, one beyond its range clamps to the
/// target's max or lowest, and a negative value clamps to zero when the target is unsigned.
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
/// A floating point value in range converts to the integer it names, and one beyond the target's
/// range clamps to the target's max or lowest.
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
/// NaN converts to zero for every integer target, where the raw cast is undefined, and stays NaN
/// for a floating point target.
/// @requirements(SEN-1054, SEN-575)
TEST(CheckedConversionsTest, FloatingPointNaNToInteger)
{
  constexpr double nanValue = std::numeric_limits<double>::quiet_NaN();

  // No integer type can represent NaN and the cast to one is undefined, so it
  // is reported and converted to zero.
  EXPECT_EQ(sen::std_util::ignoredLossyConversion<int32_t>(nanValue), 0);
  EXPECT_EQ(sen::std_util::ignoredLossyConversion<uint32_t>(nanValue), 0U);
  EXPECT_EQ(sen::std_util::ignoredLossyConversion<int64_t>(nanValue), 0);

  // A floating-point target keeps it: that cast is defined.
  EXPECT_TRUE(std::isnan(sen::std_util::ignoredLossyConversion<float>(nanValue)));
}

/// @test
/// An integer converts to its floating point value without clamping, up to int64_t's max.
/// @requirements(SEN-1054, SEN-575)
TEST(CheckedConversionsTest, IntegerToFloatingPoint)
{
  EXPECT_FLOAT_EQ(sen::std_util::ignoredLossyConversion<float>(42), 42.0f);
  EXPECT_DOUBLE_EQ(sen::std_util::ignoredLossyConversion<double>(-42), -42.0);
  EXPECT_FLOAT_EQ(sen::std_util::ignoredLossyConversion<float>(std::numeric_limits<int64_t>::max()),
                  static_cast<float>(std::numeric_limits<int64_t>::max()));
}

/// @test
/// checkedConversion returns the value unchanged when it fits the target type, across integer
/// widening and integer to float and float to double conversions.
/// @requirements(SEN-1054, SEN-575)
TEST(CheckedConversionsTest, CheckedConversionSuccessWithinBounds)
{
  EXPECT_EQ(sen::std_util::checkedConversion<int32_t>(int8_t {42}), 42);
  EXPECT_EQ(sen::std_util::checkedConversion<uint32_t>(uint8_t {255}), 255);
  EXPECT_FLOAT_EQ(sen::std_util::checkedConversion<float>(42), 42.0f);
  EXPECT_DOUBLE_EQ(sen::std_util::checkedConversion<double>(3.14f), 3.14f);
}

/// @test
/// checkedConversion's default policy terminates the process when a negative value cannot fit an
/// unsigned target, reporting the value as too small.
/// @requirements(SEN-1054)
TEST(CheckedConversionsDeathTest, ReportPolicyAssertionTerminatesOnNegative)
{
  EXPECT_DEATH(triggerNegativeOverflowAssertion(), "the value is too small for the target type");
}

/// @test
/// checkedConversion's default policy terminates the process when a value overflows the target's
/// upper bound, reporting it as too big.
/// @requirements(SEN-1054)
TEST(CheckedConversionsDeathTest, ReportPolicyAssertionTerminatesOnPositive)
{
  EXPECT_DEATH(triggerPositiveOverflowAssertion(), "the value is too large for the target type");
}

/// @test
/// checkedConversion's default policy terminates the process when a floating point value
/// overflows an integer target, in either direction.
/// @requirements(SEN-1054)
TEST(CheckedConversionsDeathTest, ReportPolicyAssertionTerminatesOnFloatToIntTruncation)
{
  EXPECT_DEATH(triggerFloatToIntPositiveOverflow(), "the value is too large for the target type");
  EXPECT_DEATH(triggerFloatToIntNegativeOverflow(), "the value is too small for the target type");
}

/// @test
/// checkedConversion's default policy terminates the process when a double beyond float range
/// would truncate to float, in either direction.
/// @requirements(SEN-1054)
TEST(CheckedConversionsDeathTest, ReportPolicyAssertionTerminatesOnDoubleToFloatTruncation)
{
  EXPECT_DEATH(triggerDoubleToFloatPositiveOverflow(), "the value is too large for the target type");
  EXPECT_DEATH(triggerDoubleToFloatNegativeOverflow(), "the value is too small for the target type");
}

/// @test
/// ReportPolicyIgnore truncates without writing anything to stdout or stderr.
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
/// ReportPolicyLog clamps the value, writes a truncation warning to stdout, and does not abort.
/// @requirements(SEN-1054)
TEST(CheckedConversionsTest, ReportPolicyLogWritesWarning)
{
  testing::internal::CaptureStdout();

  const auto result = sen::std_util::checkedConversion<uint8_t, sen::std_util::ReportPolicyLog>(-1);
  const std::string output = testing::internal::GetCapturedStdout();

  EXPECT_EQ(result, 0);
  EXPECT_FALSE(output.empty());
  EXPECT_NE(output.find("the value is too"), std::string::npos);
}

/// @test
/// ReportPolicyTrace clamps the value, writes a truncation message to stderr, and does not abort.
/// @requirements(SEN-1054)
TEST(CheckedConversionsTest, ReportPolicyTraceEmitsMessage)
{
  testing::internal::CaptureStderr();

  const auto result = sen::std_util::checkedConversion<uint8_t, sen::std_util::ReportPolicyTrace>(-1);
  const std::string output = testing::internal::GetCapturedStderr();

  EXPECT_EQ(result, 0);
  EXPECT_TRUE(output.find("the value is too") != std::string::npos);
}

/// @test
/// ReportPolicyThrow refuses the conversion instead of clamping, and the message names the
/// direction. Every other policy returns a value the caller never asked for.
/// @requirements(SEN-1054)
TEST(CheckedConversionsTest, ReportPolicyThrowRefusesOutOfRange)
{
  try
  {
    std::ignore = sen::std_util::checkedConversion<uint8_t, sen::std_util::ReportPolicyThrow>(-1);
    FAIL() << "expected a throw for a value below the target range";
  }
  catch (const std::exception& error)
  {
    EXPECT_NE(std::string(error.what()).find("too small"), std::string::npos);
  }

  EXPECT_THROW(std::ignore = (sen::std_util::checkedConversion<uint8_t, sen::std_util::ReportPolicyThrow>(300)),
               std::exception);
}

/// @test
/// A value the target can hold passes through ReportPolicyThrow untouched.
/// @requirements(SEN-1054)
TEST(CheckedConversionsTest, ReportPolicyThrowLeavesAValueThatFits)
{
  EXPECT_EQ((sen::std_util::checkedConversion<uint8_t, sen::std_util::ReportPolicyThrow>(42)), 42);
}
