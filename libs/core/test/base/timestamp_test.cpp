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
#include <string>

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
  const TimeStamp ts;
  EXPECT_EQ(ts.sinceEpoch().getNanoseconds(), 0);
  EXPECT_EQ(ts, TimeStamp {});
}

/// @test
/// Check duration time getter since epoch method
/// @requirements(SEN-1050)
TEST(Timestamp, get_time_since_epoch)
{
  const Duration tse(std::chrono::seconds(1));
  const TimeStamp ts(tse);
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

  EXPECT_TRUE(timestamp1 == timestamp2);
  EXPECT_FALSE(timestamp1 != timestamp2);

  EXPECT_TRUE(timestamp1 != timestamp3);
  EXPECT_FALSE(timestamp1 == timestamp3);

  EXPECT_TRUE(timestamp1 > timestamp3);
  EXPECT_TRUE(timestamp1 >= timestamp3);
  EXPECT_TRUE(timestamp1 >= timestamp2);

  EXPECT_TRUE(timestamp3 < timestamp1);
  EXPECT_TRUE(timestamp3 <= timestamp1);
  EXPECT_TRUE(timestamp3 <= timestamp4);

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
  const auto result = lhs + Duration(std::chrono::milliseconds(500));
  EXPECT_EQ(result.sinceEpoch().getNanoseconds(), oneSecond + halfSecond);
}

/// @test
/// Check minus operator with a duration type
/// @requirements(SEN-1050)
TEST(Timestamp, operator_minus)
{
  const auto lhs = TimeStamp(Duration(std::chrono::seconds(1)));
  const auto result = lhs - Duration(std::chrono::milliseconds(500));
  EXPECT_EQ(result.sinceEpoch().getNanoseconds(), oneSecond - halfSecond);
}

/// @test
/// Check minus operator with another timestamp
/// @requirements(SEN-1050)
TEST(Timestamp, operator_minus_timestamp)
{
  const auto lhs = TimeStamp(Duration(std::chrono::seconds(1)));
  const auto result = lhs - TimeStamp(Duration(std::chrono::milliseconds(500)));
  EXPECT_EQ(result.getNanoseconds(), oneSecond - halfSecond);
}

/// @test
/// Check toUtcString format mapping
/// @requirements(SEN-1050)
TEST(Timestamp, to_utc_string)
{
  const TimeStamp ts;
  EXPECT_EQ(ts.toUtcString(), "1970-01-01 00:00:00 000000");

  const TimeStamp ts2(Duration(std::chrono::seconds(1)));
  EXPECT_EQ(ts2.toUtcString(), "1970-01-01 00:00:01 000000");
}

/// @test
/// Check toLocalString format size and structural integrity
/// @requirements(SEN-1050)
TEST(Timestamp, to_local_string)
{
  const TimeStamp ts;
  const std::string localStr = ts.toLocalString();

  EXPECT_EQ(localStr.length(), 26);
  EXPECT_EQ(localStr.substr(19, 7), " 000000");
}

/// @test
/// Check toUtcStringNs format mapping (RFC-3339, ns precision).
TEST(Timestamp, to_utc_string_ns_epoch_and_post_epoch)
{
  const TimeStamp ts;
  EXPECT_EQ(ts.toUtcStringNs(), "1970-01-01T00:00:00.000000000Z");

  const TimeStamp ts2(Duration(std::chrono::nanoseconds(int64_t {1500000001})));
  EXPECT_EQ(ts2.toUtcStringNs(), "1970-01-01T00:00:01.500000001Z");
}

/// @test
/// Pre-epoch timestamps must format with a non-negative sub-second component and the correct
/// whole-second boundary. Pre-fix this printed e.g. "...59.-500000000Z" or skipped seconds.
TEST(Timestamp, to_utc_string_ns_pre_epoch_floor_divides_correctly)
{
  // -1.5s from epoch = 1969-12-31T23:59:58.500000000Z
  const TimeStamp ts(Duration(std::chrono::nanoseconds(int64_t {-1500000000})));
  EXPECT_EQ(ts.toUtcStringNs(), "1969-12-31T23:59:58.500000000Z");

  // -1ns from epoch = 1969-12-31T23:59:59.999999999Z
  const TimeStamp ts2(Duration(std::chrono::nanoseconds(int64_t {-1})));
  EXPECT_EQ(ts2.toUtcStringNs(), "1969-12-31T23:59:59.999999999Z");
}

/// @test
/// Sub-microsecond precision must survive the round-trip through the new ns formatter.
TEST(Timestamp, to_utc_string_ns_sub_microsecond_precision)
{
  // 123 ns past epoch.
  const TimeStamp ts(Duration(std::chrono::nanoseconds(int64_t {123})));
  EXPECT_EQ(ts.toUtcStringNs(), "1970-01-01T00:00:00.000000123Z");
}

/// @test
/// toUtcString pre-epoch should also produce a valid whole-second + fraction pair.
TEST(Timestamp, to_utc_string_pre_epoch)
{
  // -1.5s -> 1969-12-31 23:59:58 500000
  const TimeStamp ts(Duration(std::chrono::microseconds(int64_t {-1500000})));
  EXPECT_EQ(ts.toUtcString(), "1969-12-31 23:59:58 500000");
}

/// @test
/// Check make factory rejects malformed string inputs
/// @requirements(SEN-1050)
TEST(Timestamp, make_invalid)
{
  const auto res = TimeStamp::make("invalid-date-format");
  EXPECT_FALSE(res.isOk());
}

#ifdef __linux__
/// @test
/// Check make factory properly parses valid UTC time
/// @requirements(SEN-1050)
TEST(Timestamp, make_valid)
{
  const auto res = TimeStamp::make("1970-01-01 00:00:00");
  EXPECT_TRUE(res.isOk());

  if (res.isOk())
  {
    EXPECT_EQ(res.getValue<>().sinceEpoch().getNanoseconds(), 0);
  }
}

#endif
