// === constants.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_UTIL_SRC_DR_CONSTANTS_H
#define SEN_LIBS_UTIL_SRC_DR_CONSTANTS_H

// sen
#include "sen/core/base/numbers.h"

namespace sen::util
{

/// PI constant
constexpr f64 pi = 3.14159265358979323846;
constexpr f32 pif = static_cast<f32>(pi);

/// 2 * PI constant
constexpr f64 twoPi = 2.0 * pi;

/// PI / 2 constant
constexpr f64 halfPi = pi * 0.5;

/// Earth's semi.major axis in meters
constexpr f64 earthSemiMajorAxis = 6378137.0;

/// Earth's flattening
constexpr f64 earthFlattening = 1.0 / 298.257223563;

/// Square of first eccentricity
constexpr f64 eccentricitySquared = 6.69437999014e-3;

/// Min value used to determine that an entity is not moving/accelerating
constexpr f32 epsilon = 0.1f;

}  // namespace sen::util

#endif  // SEN_LIBS_UTIL_SRC_DR_CONSTANTS_H
