// === timestamp_test.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/timestamp.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>

using sen::Duration;
using sen::TimeStamp;

namespace
{

constexpr Duration::ValueType oneSecond = 1000000000;
constexpr Duration::ValueType halfSecond = 500000000;

}  // namespace

/// @test
/// Check basic timestamp class methods
/// @requirements(SEN-1050)
TEST(Timestamp, basics)
{
  TimeStamp ts;
  EXPECT_EQ(ts.sinceEpoch().getNanoseconds(), 0);
  EXPECT_EQ(ts, TimeStamp {});
}

/// @test
/// Check duration time getter since epoch method
/// @requirements(SEN-1050)
TEST(Timestamp, get_time_since_epoch)
{
  Duration tse(std::chrono::seconds(1));
  TimeStamp ts(tse);
  EXPECT_EQ(ts.sinceEpoch(), tse);
}

/// @test
/// Check comparison operator between timestamps
/// @requirements(SEN-1050)
TEST(Timestamp, comparisons)
{
  TimeStamp timestamp1(Duration(std::chrono::seconds(1)));
  TimeStamp timestamp2(Duration(std::chrono::seconds(1)));
  TimeStamp timestamp3(Duration(std::chrono::milliseconds(500)));
  TimeStamp timestamp4(Duration(std::chrono::milliseconds(500)));

  EXPECT_EQ(timestamp1, timestamp2);
  EXPECT_NE(timestamp1, timestamp3);
  EXPECT_GT(timestamp1, timestamp3);
  EXPECT_GE(timestamp1, timestamp3);
  EXPECT_GE(timestamp1, timestamp2);

  EXPECT_NE(timestamp2, timestamp3);
  EXPECT_NE(timestamp2, timestamp4);
  EXPECT_EQ(timestamp2, timestamp1);
  EXPECT_GT(timestamp2, timestamp3);
  EXPECT_GE(timestamp2, timestamp3);

  EXPECT_LT(timestamp3, timestamp1);
  EXPECT_LE(timestamp3, timestamp1);
  EXPECT_LE(timestamp3, timestamp4);
  EXPECT_GE(timestamp3, timestamp4);
  EXPECT_EQ(timestamp3, timestamp4);
}

/// @test
/// Check plus equal operator with a duration type
/// @requirements(SEN-1050)
TEST(Timestamp, operator_plus_equal)
{
  TimeStamp timestamp(Duration(std::chrono::seconds(1)));

  timestamp += Duration(std::chrono::milliseconds(500));
  EXPECT_EQ(timestamp.sinceEpoch().getNanoseconds(), oneSecond + halfSecond);

  timestamp += Duration(500);

  EXPECT_EQ(timestamp.sinceEpoch().getNanoseconds(), oneSecond + halfSecond + 500);
}

/// @test
/// Check minus equal operator with a duration type
/// @requirements(SEN-1050)
TEST(Timestamp, operator_minus_equal)
{
  TimeStamp timestamp(Duration(std::chrono::seconds(1)));

  timestamp -= Duration(std::chrono::milliseconds(500));
  EXPECT_EQ(timestamp.sinceEpoch().getNanoseconds(), oneSecond - halfSecond);

  timestamp -= Duration(500);

  EXPECT_EQ(timestamp.sinceEpoch().getNanoseconds(), oneSecond - halfSecond - 500);
}

/// @test
/// Check plus operator with a duration type
/// @requirements(SEN-1050)
TEST(Timestamp, operator_plus)
{
  const auto lhs = TimeStamp(Duration(std::chrono::seconds(1)));
  auto result = lhs + Duration(std::chrono::milliseconds(500));
  EXPECT_EQ(result.sinceEpoch().getNanoseconds(), oneSecond + halfSecond);
}

/// @test
/// Check minus operator with a duration type
/// @requirements(SEN-1050)
TEST(Timestamp, operator_minus)
{
  const auto lhs = TimeStamp(Duration(std::chrono::seconds(1)));
  auto result = lhs - Duration(std::chrono::milliseconds(500));
  EXPECT_EQ(result.sinceEpoch().getNanoseconds(), oneSecond - halfSecond);
}

/// @test
/// Check minus operator with another timestamp
/// @requirements(SEN-1050)
TEST(Timestamp, operator_minus_timestamp)
{
  const auto lhs = TimeStamp(Duration(std::chrono::seconds(1)));
  auto result = lhs - TimeStamp(Duration(std::chrono::milliseconds(500)));
  EXPECT_EQ(result.getNanoseconds(), oneSecond - halfSecond);
}
