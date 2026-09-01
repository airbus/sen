// === schedule_cycles.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_SCHEDULE_CYCLES_H
#define SEN_LIBS_KERNEL_SRC_SCHEDULE_CYCLES_H

// std
#include <chrono>
#include <cstdint>

namespace sen::kernel
{

/// How many cycles of "period" it takes to cover "amount", rounding up, and none when there is
/// nothing to cover. Computed rather than walked one cycle at a time: a clock that jumps forward
/// leaves an arbitrary amount to cover, and the walk is unbounded and never reads the stop flag.
[[nodiscard]] inline int64_t cyclesToCover(std::chrono::nanoseconds amount, std::chrono::nanoseconds period) noexcept
{
  if (amount.count() <= 0 || period.count() <= 0)
  {
    return 0;
  }

  // Divide before adding: the sum would overflow for a clock that jumps by decades, and an
  // overflowed count is negative, which would walk the schedule backwards.
  return amount.count() / period.count() + (amount.count() % period.count() != 0);
}

}  // namespace sen::kernel

#endif  // SEN_LIBS_KERNEL_SRC_SCHEDULE_CYCLES_H
