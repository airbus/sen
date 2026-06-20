// === util_test.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Wire-shape pins for varToJson: i64 / u64 / Duration as decimal strings, TimeStamp as
// RFC-3339 UTC ns, narrow ints / floats as JSON numbers.

// local
#include "util.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/var.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <limits>

namespace
{

using sen::components::jsonrpc::varToJson;

TEST(VarToJson, narrowIntsStayAsJsonNumbers)
{
  EXPECT_TRUE(varToJson(sen::Var {int32_t {42}}).is_number_integer());
  EXPECT_EQ(varToJson(sen::Var {int32_t {42}}).get<int32_t>(), 42);
  EXPECT_TRUE(varToJson(sen::Var {uint16_t {7}}).is_number_unsigned());
}

TEST(VarToJson, floatsStayAsJsonNumbers)
{
  EXPECT_TRUE(varToJson(sen::Var {float {3.5F}}).is_number_float());
  EXPECT_TRUE(varToJson(sen::Var {double {2.5}}).is_number_float());
}

TEST(VarToJson, i64MaxRoundTripsAsString)
{
  const auto value = std::numeric_limits<int64_t>::max();
  const auto json = varToJson(sen::Var {value});
  ASSERT_TRUE(json.is_string()) << json.dump();
  EXPECT_EQ(json.get<std::string>(), std::to_string(value));
}

TEST(VarToJson, u64MaxRoundTripsAsString)
{
  const auto value = std::numeric_limits<uint64_t>::max();
  const auto json = varToJson(sen::Var {value});
  ASSERT_TRUE(json.is_string()) << json.dump();
  EXPECT_EQ(json.get<std::string>(), std::to_string(value));
}

TEST(VarToJson, durationEmitsNanosecondsAsString)
{
  const sen::Duration value {std::chrono::milliseconds {1500}};
  const auto json = varToJson(sen::Var {value});
  ASSERT_TRUE(json.is_string()) << json.dump();
  EXPECT_EQ(json.get<std::string>(), std::to_string(value.getNanoseconds()));
}

TEST(VarToJson, timeStampEmitsRfc3339UtcWithNs)
{
  const sen::TimeStamp value {sen::Duration {std::chrono::nanoseconds {1750000000000000001LL}}};
  const auto json = varToJson(sen::Var {value});
  ASSERT_TRUE(json.is_string()) << json.dump();
  const auto s = json.get<std::string>();
  EXPECT_NE(s.find('T'), std::string::npos);
  EXPECT_EQ(s.back(), 'Z');
}

TEST(VarToJson, stringsStayAsStrings)
{
  const auto json = varToJson(sen::Var {std::string {"hello"}});
  ASSERT_TRUE(json.is_string());
  EXPECT_EQ(json.get<std::string>(), "hello");
}

}  // namespace
