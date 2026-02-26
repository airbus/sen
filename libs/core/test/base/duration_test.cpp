// === duration_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/duration.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <utility>

using sen::Duration;

namespace
{

constexpr Duration::ValueType oneSecond = 1000000000;
constexpr Duration::ValueType halfSecond = 500000000;

}  // namespace

/// @test
/// Check basic duration class methods
/// @requirements(SEN-358)
TEST(Duration, basics)
{
  Duration duration;
  EXPECT_EQ(duration.getNanoseconds(), 0);
  EXPECT_EQ(duration.toChrono().count(), 0);
  EXPECT_EQ(duration.toSeconds(), 0.0);
  EXPECT_EQ(duration, Duration());
}

/// @test
/// Check initialization with some value
/// @requirements(SEN-358)
TEST(Duration, constructor_value)
{
  Duration duration(5);
  EXPECT_EQ(duration.getNanoseconds(), 5);
  EXPECT_EQ(duration.toChrono().count(), 5);
  EXPECT_EQ(duration.toSeconds(), 5e-9);
  EXPECT_EQ(duration, Duration(5));
}

/// @test
/// Check initilization with chrono value
/// @requirements(SEN-358)
TEST(Duration, constructor_chrono)
{
  Duration duration(std::chrono::seconds {1});
  EXPECT_EQ(duration.getNanoseconds(), oneSecond);
  EXPECT_EQ(duration.toChrono().count(), oneSecond);
  EXPECT_EQ(duration.toSeconds(), 1);
  EXPECT_EQ(duration, Duration(oneSecond));
}

/// @test
/// Check equality operation (s)
/// @requirements(SEN-358)
TEST(Duration, equality_operator_one_sec)
{
  Duration duration1(std::chrono::seconds {1});
  Duration duration2(oneSecond);
  EXPECT_EQ(duration1, duration2);
}

/// @test
/// Check equality operation (ms)
/// @requirements(SEN-358)
TEST(Duration, equality_operator_one_millisecond)
{
  Duration duration1(std::chrono::milliseconds {1});
  Duration duration2(1000000);
  EXPECT_EQ(duration1, duration2);
}

/// @test
/// Check equality operation (us)
/// @requirements(SEN-358)
TEST(Duration, equality_operator_one_microsecond)
{
  Duration duration1(std::chrono::microseconds {1});
  Duration duration2(1000);
  EXPECT_EQ(duration1, duration2);
}

/// @test
/// Check equality operation (ns)
/// @requirements(SEN-358)
TEST(Duration, equality_operator_one_nanosecond)
{
  Duration duration1(std::chrono::nanoseconds {1});
  Duration duration2(1);
  EXPECT_EQ(duration1, duration2);
}

/// @test
/// Check != operator
/// @requirements(SEN-358)
TEST(Duration, not_equal_operator)
{
  Duration duration1(2000);
  Duration duration2(1000);
  EXPECT_NE(duration1, duration2);
}

/// @test
/// Check < operator
/// @requirements(SEN-358)
TEST(Duration, less_than_operator)
{
  Duration duration1(2000);
  Duration duration2(1000);
  EXPECT_LT(duration2, duration1);
}

/// @test
/// Check <= operator
/// @requirements(SEN-358)
TEST(Duration, less_equal_than_operator)
{
  Duration duration1(2000);
  Duration duration2(1000);
  Duration duration3(1000);

  EXPECT_LE(duration2, duration1);
  EXPECT_LE(duration2, duration3);
}

/// @test
/// Check > operator
/// @requirements(SEN-358)
TEST(Duration, greater_than_operator)
{
  Duration duration1(2000);
  Duration duration2(1000);
  EXPECT_GT(duration1, duration2);
}

/// @test
/// Check >= operator
/// @requirements(SEN-358)
TEST(Duration, greater_equal_than_operator)
{
  Duration duration1(2000);
  Duration duration2(1000);
  Duration duration3(1000);

  EXPECT_GE(duration1, duration2);
  EXPECT_GE(duration2, duration3);
}

/// @test
/// Check += operator
/// @requirements(SEN-358)
TEST(Duration, plus_equal_operator)
{
  Duration duration(std::chrono::seconds(1));

  duration += Duration(std::chrono::milliseconds(500));
  EXPECT_EQ(duration.getNanoseconds(), oneSecond + halfSecond);

  duration += Duration(500);

  EXPECT_EQ(duration.getNanoseconds(), oneSecond + halfSecond + 500);
}

/// @test
/// Check -= operator
/// @requirements(SEN-358)
TEST(Duration, minus_equal_operator)
{
  Duration duration(std::chrono::seconds(1));

  duration -= Duration(std::chrono::milliseconds(500));
  EXPECT_EQ(duration.getNanoseconds(), oneSecond - halfSecond);

  duration -= Duration(500);

  EXPECT_EQ(duration.getNanoseconds(), oneSecond - halfSecond - 500);
}

/// @test
/// Check + operator
/// @requirements(SEN-358)
TEST(Duration, plus_operator)
{
  const auto lhs = Duration(std::chrono::seconds(1));
  auto result = lhs + Duration(std::chrono::milliseconds(500));
  EXPECT_EQ(result.getNanoseconds(), oneSecond + halfSecond);
}

/// @test
/// Check - operator
/// @requirements(SEN-358)
TEST(Duration, minus_operator)
{
  const auto lhs = Duration(std::chrono::seconds(1));
  auto result = lhs - Duration(std::chrono::milliseconds(500));
  EXPECT_EQ(result.getNanoseconds(), oneSecond - halfSecond);
}

/// @test
/// Check - operator for negative result values
/// @requirements(SEN-358)
TEST(Duration, self_minus_operator)
{
  const auto result = Duration(std::chrono::seconds(1));
  EXPECT_EQ(result.getNanoseconds(), oneSecond);
  EXPECT_EQ((-result).getNanoseconds(), -oneSecond);
}

/// @test
/// Check equality of durations specify in Hertzs
/// @requirements(SEN-358)
TEST(Duration, from_hertz)
{
  EXPECT_EQ(Duration::fromHertz(1.0).getNanoseconds(), oneSecond);
  EXPECT_EQ(Duration::fromHertz(1.0f).getNanoseconds(), oneSecond);

  EXPECT_EQ(Duration::fromHertz(0.5), Duration(std::chrono::milliseconds(2000)));
  EXPECT_EQ(Duration::fromHertz(0.5f), Duration(std::chrono::milliseconds(2000)));

  EXPECT_EQ(Duration::fromHertz(2.0), Duration(std::chrono::milliseconds(500)));
  EXPECT_EQ(Duration::fromHertz(2.0f), Duration(std::chrono::milliseconds(500)));
}

/// @test
/// Check operations with different duration unit types
/// @requirements(SEN-358)
TEST(Duration, operations_different_units)
{
  Duration duration(std::chrono::hours(1));
  Duration duration2(std::chrono::seconds(3601));

  EXPECT_EQ(duration.toSeconds(), 3600.0);
  EXPECT_EQ(duration.getNanoseconds(), 3600 * oneSecond);
  EXPECT_EQ(duration.toChrono().count(), 3600 * oneSecond);
  EXPECT_NE(duration.toSeconds(), duration2.toSeconds());
  EXPECT_LT(duration, duration2);

  const auto result1 = duration + Duration(std::chrono::seconds(3600));
  EXPECT_EQ(result1, Duration(std::chrono::minutes(120)));

  const auto result2 = duration2 - duration;
  EXPECT_EQ(result2, oneSecond);

  const auto result3 = duration - Duration(std::chrono::microseconds(1));
  EXPECT_NE(result3, duration);
  EXPECT_FLOAT_EQ(result3.toSeconds(), duration.toSeconds());
}

/// @test
/// Check payload comparison between different durations
/// @requirements(SEN-358)
TEST(Duration, payload_comparison)
{
  Duration duration1(oneSecond);
  Duration duration2(std::chrono::seconds(1));
  Duration duration3(std::chrono::milliseconds(100));

  EXPECT_EQ(duration1, duration2);
  EXPECT_EQ(duration1, duration1);
  EXPECT_NE(duration1, duration3);

  duration1 = duration3;
  EXPECT_EQ(duration1, duration3);

  duration1 = duration2;
  duration3 = std::move(duration1);
  EXPECT_EQ(duration3, oneSecond);
}
