// === settable_dead_reckoner_impl.cpp =================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/util/dr/detail/settable_dead_reckoner_impl.h"

// sen
#include "sen/core/base/numbers.h"
#include "sen/util/dr/algorithms.h"

// implementation
#include "quat.h"
#include "sen/core/base/numbers.h"
#include "sen/util/dr/algorithms.h"
#include "utils.h"

// std
#include <cmath>

namespace sen::util::impl
{

bool isMoving(const Velocity& velocity) { return hasVelocity(velocity); }

bool isAccelerating(const Acceleration& acceleration) { return hasAcceleration(acceleration); }

bool isRotating(const AngularVelocity& omega) { return hasAngularVelocity(omega); }

AngularVelocity nedToBody(const AngularVelocity& value, const Orientation& nedOrientation) noexcept
{
  return toAngularVelocity(nedToBody(fromAngularVelocity(value), nedOrientation));
}

bool maxDistanceExceeded(const Location& newPosition, const Location& extrapolatedPosition, float64_t threshold)
{
  return std::abs(newPosition.x - extrapolatedPosition.x) > threshold ||
         std::abs(newPosition.y - extrapolatedPosition.y) > threshold ||
         std::abs(newPosition.z - extrapolatedPosition.z) > threshold;
}

bool maxRotationExceeded(const Orientation& newOrientation,
                         const Orientation& extrapolatedOrientation,
                         float64_t threshold)
{
  return Quatd(fromOrientationToVec(newOrientation)).dot(Quatd(fromOrientationToVec(extrapolatedOrientation))) <
         threshold;
}

}  // namespace sen::util::impl
