// === algorithms.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/util/dr/algorithms.h"

// sen
#include "sen/core/base/timestamp.h"

// implementation
#include "utils.h"

namespace sen::util
{

namespace
{

// ----------------------------------------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------------------------------------

/// Returns the same situation with its TimeStamp updated to time
[[nodiscard]] Situation updateTimeStamp(const Situation& value, sen::TimeStamp time) noexcept
{
  Situation result {value};
  result.timeStamp = time;
  return result;
}

}  // namespace

Situation drFpw(const Situation& value, sen::TimeStamp time) noexcept
{
  return value.isFrozen ? updateTimeStamp(value, time)
                        : Situation {false,
                                     time,
                                     extrapolateLocationWorld(value, time - value.timeStamp),
                                     value.orientation,
                                     value.velocityVector};
}

Situation drFpb(const Situation& value, sen::TimeStamp time) noexcept
{
  return value.isFrozen ? updateTimeStamp(value, time)
                        : Situation {false,
                                     time,
                                     extrapolateLocationBody(value, time - value.timeStamp),
                                     value.orientation,
                                     value.velocityVector};
}

Situation drRpw(const Situation& value, sen::TimeStamp time) noexcept
{
  const auto delta = time - value.timeStamp;

  return value.isFrozen ? updateTimeStamp(value, time)
                        : Situation {false,
                                     time,
                                     extrapolateLocationWorld(value, delta),
                                     extrapolateOrientation(value.orientation, delta, value.angularVelocity),
                                     value.velocityVector,
                                     value.angularVelocity};
}

Situation drRpb(const Situation& value, sen::TimeStamp time) noexcept
{
  const auto delta = time - value.timeStamp;

  return value.isFrozen ? updateTimeStamp(value, time)
                        : Situation {false,
                                     time,
                                     extrapolateLocationBody(value, delta),
                                     extrapolateOrientation(value.orientation, delta, value.angularVelocity),
                                     value.velocityVector,
                                     value.angularVelocity};
}

Situation drRvw(const Situation& value, sen::TimeStamp time) noexcept
{
  const auto delta = time - value.timeStamp;

  return value.isFrozen ? updateTimeStamp(value, time)
                        : Situation {false,
                                     time,
                                     extrapolateLocationWorld(value, delta),
                                     extrapolateOrientation(value.orientation, delta, value.angularVelocity),
                                     value.velocityVector,
                                     value.angularVelocity,
                                     value.accelerationVector};
}

Situation drRvb(const Situation& value, sen::TimeStamp time) noexcept
{
  const auto delta = time - value.timeStamp;

  return value.isFrozen ? updateTimeStamp(value, time)
                        : Situation {false,
                                     time,
                                     extrapolateLocationBody(value, delta),
                                     extrapolateOrientation(value.orientation, delta, value.angularVelocity),
                                     extrapolateVelocity(value.velocityVector, value.accelerationVector, delta),
                                     value.angularVelocity,
                                     value.accelerationVector};
}

Situation drFvw(const Situation& value, sen::TimeStamp time) noexcept
{
  const auto delta = time - value.timeStamp;

  return value.isFrozen ? updateTimeStamp(value, time)
                        : Situation {false,
                                     time,
                                     extrapolateLocationWorld(value, delta),
                                     extrapolateOrientation(value.orientation, delta, value.angularVelocity),
                                     extrapolateVelocity(value.velocityVector, value.accelerationVector, delta),
                                     value.angularVelocity,
                                     value.accelerationVector};
}

Situation drFvb(const Situation& value, sen::TimeStamp time) noexcept
{
  const auto delta = time - value.timeStamp;

  return value.isFrozen ? updateTimeStamp(value, time)
                        : Situation {false,
                                     time,
                                     extrapolateLocationBody(value, delta),
                                     value.orientation,
                                     extrapolateVelocity(value.velocityVector, value.accelerationVector, delta),
                                     {},
                                     value.accelerationVector};
}

}  // namespace sen::util
