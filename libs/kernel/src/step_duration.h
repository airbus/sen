// === step_duration.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_STEP_DURATION_H
#define SEN_LIBS_KERNEL_SRC_STEP_DURATION_H

// sen
#include "sen/core/base/duration.h"
#include "sen/core/obj/callback.h"

// std
#include <limits>

namespace sen::kernel
{

/// Seeds the reduction over what the clocks report, so any answer wins over it. If it survives
/// the reduction, no clock asked to be run.
constexpr auto noStepReported = std::numeric_limits<Duration::ValueType>::max();

/// The soonest next step once one more clock has answered. An error result is left unread and
/// leaves the step where it was: reading its value ends the process.
[[nodiscard]] inline Duration soonestStep(Duration soonest, const MethodResult<Duration>& reported)
{
  if (reported.isError() || !(reported.getValue() < soonest))
  {
    return soonest;
  }

  return reported.getValue();
}

/// The step to advance by, once every clock has answered. One that no clock asked for is of no
/// duration, which the stepping loops read as nothing to advance.
[[nodiscard]] inline Duration stepOrNone(Duration soonest) noexcept
{
  return soonest == Duration {noStepReported} ? Duration {} : soonest;
}

}  // namespace sen::kernel

#endif  // SEN_LIBS_KERNEL_SRC_STEP_DURATION_H
