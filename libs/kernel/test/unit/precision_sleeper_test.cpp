// === precision_sleeper_test.cpp ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// kernel
#include "precision_sleeper.h"
#include "wall_clock.h"

// stl
#include "stl/sen/kernel/basic_types.stl.h"

// gtest
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <string>
#include <tuple>

#if defined(__linux__)
#  include <sys/prctl.h>
#endif

namespace
{

using sen::kernel::NanoSecs;
using sen::kernel::SleepOverhead;

constexpr auto ms(int64_t value) { return std::chrono::duration_cast<NanoSecs>(std::chrono::milliseconds(value)); }
constexpr auto us(int64_t value) { return std::chrono::duration_cast<NanoSecs>(std::chrono::microseconds(value)); }

/// A sleep that costs what it asks for plus a fixed overhead, which is how the operating system
/// behaves. Lets the real stepping loop run thousands of cycles without waiting for them.
class CostModel
{
public:
  explicit CostModel(NanoSecs overhead) noexcept: overhead_(overhead) {}

  [[nodiscard]] NanoSecs sleepFor(NanoSecs requested) noexcept
  {
    ++sleeps_;
    return requested + overhead_;
  }

  [[nodiscard]] uint64_t sleeps() const noexcept { return sleeps_; }

private:
  NanoSecs overhead_;
  uint64_t sleeps_ {0};
};

/// What a run of cycles cost: how much of each window was left to spin, and how often it overran.
struct RunResult
{
  double spinFraction {0.0};   ///< over the last part of the run, once it has settled
  double lateFraction {0.0};   ///< cycles that ran past the end of the window
  double worstLateness {0.0};  ///< the worst overshoot, as a fraction of the window
};

/// Drives the real stepping loop over many cycles at a fixed idle window.
[[nodiscard]] RunResult run(SleepOverhead& cost, NanoSecs window, NanoSecs overhead, int cycles)
{
  CostModel ops {overhead};
  NanoSecs settledSpin {0};
  int settledCycles = 0;
  int late = 0;
  int64_t worst = 0;

  // the last fifth is where it has settled; the rest is whatever it took to get there
  const auto settleAfter = cycles - cycles / 5;

  for (auto i = 0; i < cycles; ++i)
  {
    cost.decay();
    const auto left = sen::kernel::stepDownWith(window, ms(1), cost, ops);

    if (left.count() < 0)
    {
      ++late;
      worst = std::max(worst, -left.count());
    }

    if (i >= settleAfter)
    {
      ++settledCycles;
      settledSpin += std::max(left, NanoSecs {0});
    }
  }

  const auto settledTotal = static_cast<double>(window.count()) * settledCycles;
  return {static_cast<double>(settledSpin.count()) / settledTotal,
          static_cast<double>(late) / cycles,
          static_cast<double>(worst) / static_cast<double>(window.count())};
}

/// @test
/// The step must never run past the end of the window. Oversleeping is the failure the whole
/// class exists to avoid, and it only shows up across many cycles.
TEST(SleepStepping, NeverStepsPastTheWindow)
{
  for (const auto overhead: {us(55), us(105), ms(1)})
  {
    for (const auto window: {us(200), us(450), ms(2), ms(6), ms(50)})
    {
      SleepOverhead cost {ms(1)};
      const auto result = run(cost, window, overhead, 5000);
      // Occasionally late is expected: recovering from an over-large overhead means trying a
      // sleep to see what it costs now. Late by an amount that matters is not.
      EXPECT_LT(result.worstLateness, 0.05)
        << "overhead " << overhead.count() << ", window " << window.count() << ": worst overshoot was "
        << (result.worstLateness * 100.0) << "% of the window, on " << (result.lateFraction * 100.0) << "% of cycles";
    }
  }
}

/// @test
/// Where sleeps are cheap the step must keep sleeping until close to the wake-up time, leaving
/// little to spin.
TEST(SleepStepping, CutsTheSpinWhereSleepsAreCheap)
{
  SleepOverhead cost {ms(1)};
  const auto result = run(cost, ms(6), us(55), 5000);

  EXPECT_LT(result.spinFraction, 0.20) << "left " << (result.spinFraction * 100.0)
                                       << "% of the window to be spun, so the step is barely running";
}

/// @test
/// An overhead learned while the machine was slow must not keep the step from running once it is
/// fast again.
TEST(SleepStepping, RecoversWhenTheMachineGetsFasterAgain)
{
  SleepOverhead cost {ms(1)};

  // learn a slow machine
  cost.observe(ms(1), ms(1) + ms(20));
  ASSERT_GT(cost.overhead().count(), NanoSecs {ms(5)}.count()) << "the overhead never rose, so nothing is proven";

  const auto result = run(cost, ms(6), us(55), 20000);
  EXPECT_LT(result.spinFraction, 0.25) << "still spinning " << (result.spinFraction * 100.0)
                                       << "% of the window, so the overhead never came back down";
}

/// @test
/// One stalled sleep raises the overhead, but must not hold it there: it measures a moment, not
/// the machine, and leaving it in place means every later cycle spins.
TEST(SleepOverheadModel, DecaysAwayOneStalledSleep)
{
  SleepOverhead cost {ms(1)};
  for (auto i = 0; i < 100; ++i)
  {
    cost.observe(ms(1), ms(1) + us(55));
  }

  const auto settled = cost.overhead();
  cost.observe(ms(1), ms(1) + ms(500));
  EXPECT_GT(cost.overhead().count(), settled.count()) << "a stalled sleep was not learned from at all";

  // and it comes back down on its own
  for (auto i = 0; i < 10000; ++i)
  {
    cost.decay();
  }

  EXPECT_LT(cost.overhead().count(), NanoSecs {us(100)}.count())
    << "one stalled sleep still has the steps stopped thousands of cycles later";
}

/// @test
/// A sleep cannot return early, so a measurement saying it did is the clock moving. Not learned
/// from.
TEST(SleepOverheadModel, IgnoresASleepThatAppearsToHaveReturnedEarly)
{
  SleepOverhead cost {ms(1)};
  for (auto i = 0; i < 100; ++i)
  {
    cost.observe(ms(1), ms(1) + us(55));
  }

  const auto settled = cost.overhead();
  const auto before = cost.samples();
  cost.observe(ms(1), us(1));

  EXPECT_EQ(cost.samples(), before) << "a backwards measurement was folded into the overhead";
  EXPECT_EQ(cost.overhead().count(), settled.count());
}

}  // namespace
