// === source_time.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_INFLUX_SRC_SOURCE_TIME_H
#define SEN_COMPONENTS_INFLUX_SRC_SOURCE_TIME_H

// sen
#include "sen/core/base/timestamp.h"

namespace sen::components::influx
{

/// The time a point carries. A property change arrives stamped with the time its value was
/// committed, and an object's creation and deletion take the same time from the object, so every
/// point of one object lands on one clock. A replayed object commits at the time its values were
/// recorded, where the component's own time is today's.
///
/// An object that has committed nothing has no time of its own, and the component's time stands in.
[[nodiscard]] inline TimeStamp sourceTime(TimeStamp committed, TimeStamp fallback) noexcept
{
  return committed.sinceEpoch().get() != 0 ? committed : fallback;
}

}  // namespace sen::components::influx

#endif  // SEN_COMPONENTS_INFLUX_SRC_SOURCE_TIME_H
