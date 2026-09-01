// === runner_schedule_test.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// kernel
#include "schedule_cycles.h"

// gtest
#include <gtest/gtest.h>

// std
#include <chrono>
#include <cstdint>

namespace
{

using sen::kernel::cyclesToCover;
using NanoSecs = std::chrono::nanoseconds;

constexpr auto ms(int64_t value) { return std::chrono::duration_cast<NanoSecs>(std::chrono::milliseconds(value)); }

/// What the schedule used to do: one period per iteration, until the instant is reached.
[[nodiscard]] int64_t cyclesByWalking(NanoSecs amount, NanoSecs period)
{
  int64_t cycles = 0;
  for (auto covered = NanoSecs {0}; covered < amount; covered += period)
  {
    ++cycles;
  }

  return cycles;
}

/// The other walk this replaces, which ran while the schedule was more than a whole cycle behind.
/// Its caller subtracts one period first, and that subtraction is what needs checking: one period
/// out would run a frame that should have been skipped.
[[nodiscard]] int64_t cyclesByWalkingAfterOversleep(NanoSecs behind, NanoSecs period)
{
  int64_t cycles = 0;
  for (auto remaining = behind; remaining > period; remaining -= period)
  {
    ++cycles;
  }

  return cycles;
}

/// @test
/// The computed count matches the loop it replaces, so this is a refactor rather than a change of
/// behaviour.
TEST(RunnerSchedule, CountsTheSameCyclesTheOldWalkDid)
{
  for (const auto period: {ms(1), ms(10), ms(100)})
  {
    for (int64_t multiple = 0; multiple < 40; ++multiple)
    {
      for (const auto offset: {NanoSecs {-1}, NanoSecs {0}, NanoSecs {1}})
      {
        const auto amount = period * multiple + offset;
        EXPECT_EQ(cyclesToCover(amount, period), cyclesByWalking(amount, period))
          << "period " << period.count() << ", amount " << amount.count();
      }
    }
  }
}

/// @test
/// The same, for the caller that has overslept. It asks about the time beyond one whole cycle, so
/// the count has to match a walk that stops one cycle short of the one above.
TEST(RunnerSchedule, CountsTheSameCyclesTheOversleepWalkDid)
{
  for (const auto period: {ms(1), ms(10), ms(100)})
  {
    for (int64_t multiple = 0; multiple < 40; ++multiple)
    {
      for (const auto offset: {NanoSecs {-1}, NanoSecs {0}, NanoSecs {1}})
      {
        const auto behind = period * multiple + offset;
        EXPECT_EQ(cyclesToCover(behind - period, period), cyclesByWalkingAfterOversleep(behind, period))
          << "period " << period.count() << ", behind " << behind.count();
      }
    }
  }
}

/// @test
/// Nothing to cover means no cycles, so an on-time frame does not move the schedule.
TEST(RunnerSchedule, AdvancesNothingWhenTheScheduleIsNotBehind)
{
  EXPECT_EQ(cyclesToCover(NanoSecs {0}, ms(10)), 0);
  EXPECT_EQ(cyclesToCover(NanoSecs {-1}, ms(10)), 0);
  EXPECT_EQ(cyclesToCover(ms(-500), ms(10)), 0);
}

/// @test
/// A period of zero cannot be divided by, and a schedule cannot advance in steps of nothing.
TEST(RunnerSchedule, RefusesAPeriodOfZero)
{
  EXPECT_EQ(cyclesToCover(ms(10), NanoSecs {0}), 0);
  EXPECT_EQ(cyclesToCover(ms(10), ms(-1)), 0);
}

/// @test
/// The schedule lands past the instant it was catching up to, which is the property the caller
/// relies on: one cycle short would leave the frame still behind.
TEST(RunnerSchedule, LandsPastTheInstantItChases)
{
  const auto period = ms(10);
  for (const auto behind: {NanoSecs {1}, ms(1), ms(10), ms(11), ms(999), ms(60000)})
  {
    const auto advanced = period * cyclesToCover(behind, period);
    EXPECT_GE(advanced.count(), behind.count()) << "behind by " << behind.count();
    EXPECT_LT((advanced - period).count(), behind.count()) << "overshot by more than a cycle";
  }
}

/// @test
/// A clock that jumps forward by years is arithmetic here, not iteration. The walk it replaces
/// would have run for billions of iterations without reading the stop flag.
TEST(RunnerSchedule, HandlesAJumpOfYearsWithoutIterating)
{
  const auto period = ms(10);  // 100 Hz
  const auto decades = std::chrono::duration_cast<NanoSecs>(std::chrono::hours(24 * 365 * 20));

  const auto cycles = cyclesToCover(decades, period);
  EXPECT_EQ(cycles, decades.count() / period.count());
  EXPECT_GT(cycles, 63000000000);
}

}  // namespace
