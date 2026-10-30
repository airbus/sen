// === step_duration_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// implementation
#include "step_duration.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/result.h"
#include "sen/core/obj/callback.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <exception>
#include <stdexcept>

namespace
{

using sen::Duration;
using sen::MethodResult;
using sen::kernel::noStepReported;
using sen::kernel::soonestStep;
using sen::kernel::stepOrNone;

[[nodiscard]] MethodResult<Duration> couldNotAnswer()
{
  return MethodResult<Duration> {sen::Err(std::make_exception_ptr(std::runtime_error("no answer")))};
}

/// @test
/// A clock that could not answer leaves the step where the other clocks put it.
TEST(StepDuration, AClockThatCouldNotAnswerLeavesTheStepAlone)
{
  const Duration soonest {std::chrono::seconds(2)};

  EXPECT_EQ(soonestStep(soonest, couldNotAnswer()), soonest);
  EXPECT_EQ(soonestStep(Duration {noStepReported}, couldNotAnswer()), Duration {noStepReported});
}

/// @test
/// The soonest step any clock asked for wins, whichever order the clocks answer in.
TEST(StepDuration, TheSoonestAnswerWins)
{
  const Duration sooner {std::chrono::milliseconds(20)};
  const Duration later {std::chrono::seconds(1)};

  EXPECT_EQ(soonestStep(Duration {noStepReported}, MethodResult<Duration> {sen::Ok(later)}), later);
  EXPECT_EQ(soonestStep(later, MethodResult<Duration> {sen::Ok(sooner)}), sooner);
  EXPECT_EQ(soonestStep(sooner, MethodResult<Duration> {sen::Ok(later)}), sooner);
}

/// @test
/// A step no clock asked for is one of no duration.
TEST(StepDuration, AStepNoClockAskedForIsOfNoDuration)
{
  const Duration asked {std::chrono::milliseconds(500)};

  EXPECT_EQ(stepOrNone(Duration {noStepReported}), Duration {});
  EXPECT_EQ(stepOrNone(asked), asked);
  EXPECT_EQ(stepOrNone(Duration {}), Duration {});
}

}  // namespace
