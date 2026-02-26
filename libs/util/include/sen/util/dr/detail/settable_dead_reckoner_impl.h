// === settable_dead_reckoner_impl.h ===================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_UTIL_DR_DETAIL_SETTABLE_DEAD_RECKONER_IMPL_H
#define SEN_UTIL_DR_DETAIL_SETTABLE_DEAD_RECKONER_IMPL_H

// sen
#include "sen/util/dr/algorithms.h"

namespace sen::util::impl
{

/// \addtogroup dr
/// @{

/// Returns true when entity velocity is not null
[[nodiscard]] bool isMoving(const Velocity& velocity);

/// Returns true when entity acceleration is not null
[[nodiscard]] bool isAccelerating(const Acceleration& acceleration);

/// Returns true when angular velocity acceleration is not null
[[nodiscard]] bool isRotating(const AngularVelocity& omega);

/// Translates an AngularVelocity vector from NED to body coordinates
[[nodiscard]] AngularVelocity nedToBody(const AngularVelocity& value, const Orientation& nedOrientation) noexcept;

/// Returns true when the distance threshold has been exceeded
[[nodiscard]] bool maxDistanceExceeded(const Location& newPosition,
                                       const Location& extrapolatedPosition,
                                       f64 threshold);

/// Returns true when the rotation threshold has been exceeded
[[nodiscard]] bool maxRotationExceeded(const Orientation& newOrientation,
                                       const Orientation& extrapolatedOrientation,
                                       f64 threshold);

/// @}

}  // namespace sen::util::impl

#endif  // SEN_UTIL_DR_DETAIL_SETTABLE_DEAD_RECKONER_IMPL_H
