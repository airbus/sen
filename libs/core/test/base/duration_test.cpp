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
/// Defaults to a zero duration, reporting zero nanoseconds, a zero chrono count, 0.0 seconds, and equality with
/// another default-constructed Duration.
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
/// Stores a raw nanosecond count given at construction, Duration(5) reporting 5 nanoseconds, a chrono count of 5,
/// and 5e-9 seconds.
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
/// Converts a chrono value given at construction, one std::chrono second reporting 1000000000 nanoseconds and
/// 1.0 seconds.
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
/// Compares equal when built from one chrono second and from the raw nanosecond count 1000000000.
/// @requirements(SEN-358)
TEST(Duration, equality_operator_one_sec)
{
  Duration duration1(std::chrono::seconds {1});
  Duration duration2(oneSecond);
  EXPECT_EQ(duration1, duration2);
}

/// @test
/// Compares equal when built from one chrono millisecond and from the raw nanosecond count 1000000.
/// @requirements(SEN-358)
TEST(Duration, equality_operator_one_millisecond)
{
  Duration duration1(std::chrono::milliseconds {1});
  Duration duration2(1000000);
  EXPECT_EQ(duration1, duration2);
}

/// @test
/// Compares equal when built from one chrono microsecond and from the raw nanosecond count 1000.
/// @requirements(SEN-358)
TEST(Duration, equality_operator_one_microsecond)
{
  Duration duration1(std::chrono::microseconds {1});
  Duration duration2(1000);
  EXPECT_EQ(duration1, duration2);
}

/// @test
/// Compares equal when built from one chrono nanosecond and from the raw nanosecond count 1.
/// @requirements(SEN-358)
TEST(Duration, equality_operator_one_nanosecond)
{
  Duration duration1(std::chrono::nanoseconds {1});
  Duration duration2(1);
  EXPECT_EQ(duration1, duration2);
}

/// @test
/// Reports 2000 and 1000 nanosecond durations as not equal.
/// @requirements(SEN-358)
TEST(Duration, not_equal_operator)
{
  Duration duration1(2000);
  Duration duration2(1000);
  EXPECT_NE(duration1, duration2);
}

/// @test
/// Orders 1000 nanoseconds strictly before 2000 nanoseconds under operator<.
/// @requirements(SEN-358)
TEST(Duration, less_than_operator)
{
  Duration duration1(2000);
  Duration duration2(1000);
  EXPECT_LT(duration2, duration1);
}

/// @test
/// Satisfies operator<= both for a strictly smaller duration and for an equal one.
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
/// Orders 2000 nanoseconds strictly after 1000 nanoseconds under operator>.
/// @requirements(SEN-358)
TEST(Duration, greater_than_operator)
{
  Duration duration1(2000);
  Duration duration2(1000);
  EXPECT_GT(duration1, duration2);
}

/// @test
/// Satisfies operator>= both for a strictly larger duration and for an equal one.
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
/// Accumulates in place with operator+=, one second plus 500 chrono milliseconds plus 500 raw nanoseconds
/// reaching 1500000500 nanoseconds.
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
/// Reduces in place with operator-=, one second minus 500 chrono milliseconds minus 500 raw nanoseconds leaving
/// 499999500 nanoseconds.
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
/// Adds two durations with operator+, one second plus 500 milliseconds yielding 1500000000 nanoseconds.
/// @requirements(SEN-358)
TEST(Duration, plus_operator)
{
  const auto lhs = Duration(std::chrono::seconds(1));
  auto result = lhs + Duration(std::chrono::milliseconds(500));
  EXPECT_EQ(result.getNanoseconds(), oneSecond + halfSecond);
}

/// @test
/// Subtracts two durations with operator-, one second minus 500 milliseconds yielding 500000000 nanoseconds.
/// @requirements(SEN-358)
TEST(Duration, minus_operator)
{
  const auto lhs = Duration(std::chrono::seconds(1));
  auto result = lhs - Duration(std::chrono::milliseconds(500));
  EXPECT_EQ(result.getNanoseconds(), oneSecond - halfSecond);
}

/// @test
/// Negates with unary minus, one second reporting minus 1000000000 nanoseconds.
/// @requirements(SEN-358)
TEST(Duration, self_minus_operator)
{
  const auto result = Duration(std::chrono::seconds(1));
  EXPECT_EQ(result.getNanoseconds(), oneSecond);
  EXPECT_EQ((-result).getNanoseconds(), -oneSecond);
}

/// @test
/// Converts a frequency to its period, 1 Hz giving one second, 0.5 Hz two seconds, and 2 Hz 500 milliseconds,
/// from both double and float arguments.
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
/// Keeps comparisons and arithmetic exact across construction units. One chrono hour reports 3600 seconds, orders
/// before 3601 chrono seconds, and sums with 3600 chrono seconds to 120 chrono minutes, while subtracting one
/// microsecond yields an unequal duration whose seconds value is still float-equal.
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
/// Compares by value across construction forms and keeps that value through copy and move assignment, the
/// assigned duration equaling its source.
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
