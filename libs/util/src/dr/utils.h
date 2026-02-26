// === utils.h =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_UTIL_SRC_DR_UTILS_H
#define SEN_LIBS_UTIL_SRC_DR_UTILS_H

#include "mat3.h"
#include "quat.h"

// sen
#include "sen/util/dr/algorithms.h"

namespace sen::util
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

/// Translates a Vec3d instance to an RPR world location structure.
[[nodiscard]] Location toWorldLocation(const Vec3d& value);

/// Converts an RPR world location structure to a Vec3d instance.
[[nodiscard]] Vec3d fromWorldLocation(const Location& value);

/// Converts an RPR velocity vector structure to a Vec3d instance.
[[nodiscard]] Vec3d fromVelocity(const Velocity& value);

/// Converts a Vec3 to a RPR velocity vector structure.
[[nodiscard]] Velocity toVelocity(const Vec3d& value);

/// Converts an RPR acceleration vector structure to a Vec3d instance.
[[nodiscard]] Vec3d fromAcceleration(const Acceleration& value);

/// Converts an RPR acceleration vector structure to a Vec3d instance.
[[nodiscard]] Acceleration toAcceleration(const Vec3d& value);

/// Converts an RPR angular acceleration vector structure to a Vec3d instance.
[[nodiscard]] AngularAcceleration toAngularAcceleration(const Vec3d& value);

/// Converts an RPR angular velocity vector structure to a Vec3d instance.
[[nodiscard]] Vec3d fromAngularVelocity(const AngularVelocity& value);

[[nodiscard]] Vec3d fromAngularAcceleration(const AngularAcceleration& value);

/// Converts an RPR angular velocity vector structure to a Vec3d instance.
[[nodiscard]] AngularVelocity toAngularVelocity(const Vec3d& value);

/// Translates a Vec3d instance to an RPR orientation structure.
[[nodiscard]] Orientation toOrientation(const Vec3d& value);

/// Converts an RPR orientation structure to a Vec3d instance.
[[nodiscard]] Vec3d fromOrientationToVec(const Orientation& value);

/// Converts an RPR orientation structure to a Quatf instance.
[[nodiscard]] Quatd fromOrientationToQuat(const Orientation& value);

/// Translates from NED to ECEF location coordinates using quaternions
[[nodiscard]] Vec3d nedToEcef(const Vec3d& value, const GeodeticWorldLocation& latLonAlt) noexcept;

/// Translates from ECEF to NED location coordinates using quaternions
[[nodiscard]] Vec3d ecefToNed(const Vec3d& value, const GeodeticWorldLocation& latLonAlt) noexcept;

/// Changes the basis of a vector from body referenced coordinates to NED coordinates
[[nodiscard]] Vec3d bodyToNed(const Vec3d& value, const Orientation& orientationNed) noexcept;

/// Changes the basis of a vector from NED coordinates to body referenced coordinates
[[nodiscard]] Vec3d nedToBody(const Vec3d& value, const Orientation& orientationNed) noexcept;

/// Degrees to radians
[[nodiscard]] f64 toRad(f64 value) noexcept;

/// Radians to degrees
[[nodiscard]] f64 toDeg(f64 value) noexcept;

/// Returns the R1 matrix used in the Dead Reckoning algorithms involving velocity in body coordinates
[[nodiscard]] Mat3d makeR1(const Vec3d& omega, f64 delta);

/// Returns the R2 matrix used in the Dead Reckoning algorithms involving acceleration in body coordinates
[[nodiscard]] Mat3d makeR2(const Vec3d& omega, f64 delta);

/// Transforms a vector from body coordinates (x forward, y right, z down ) to world coordinates using quaternions
/// given the Euler Angles of the transformation from the world reference system to the body reference system
[[nodiscard]] Vec3d bodyToWorld(const Vec3d& value, const Orientation& bodyEulerAngles) noexcept;

/// Transforms a vector from world coordinates (ECEF) to body coordinates using quaternions
/// given the Euler Angles of the transformation from the world reference system to the body reference system
[[nodiscard]] Vec3d worldToBody(const Vec3d& value, const Orientation& bodyEulerAngles) noexcept;

/// Extrapolates the WorldLocation for the algorithms that use the World reference system
[[nodiscard]] Location extrapolateLocationWorld(const Situation& value, sen::Duration delta) noexcept;

/// Extrapolates the WorldLocation for the algorithms that use the Body reference system
[[nodiscard]] Location extrapolateLocationBody(const Situation& value, sen::Duration delta) noexcept;

/// Extrapolates Velocities with constant Acceleration
[[nodiscard]] Velocity extrapolateVelocity(const Velocity& value,
                                           const Acceleration& acceleration,
                                           sen::Duration delta) noexcept;

/// Performs a linear extrapolation the orientation of an entity using quaternions given a delta of time and the
/// angular velocity in world coordinates
[[nodiscard]] Orientation extrapolateOrientation(const Orientation& value,
                                                 sen::Duration delta,
                                                 const AngularVelocity& angularVelocity,
                                                 const AngularAcceleration& angularAcceleration = {}) noexcept;

/// Returns the NED trihedron at a certain world location given the Latitude and Longitude
[[nodiscard]] std::array<Vec3d, 3U> getNedTrihedron(const GeodeticWorldLocation& location);

/// Given two trihedrons of vectors, computes the Euler Angles needed to transform the initial
/// trihedron (xi, yi, zi) into the final trihedron (xf, yf, zf).
[[nodiscard]] Orientation eulerAnglesFromTrihedrons(const Vec3d& xi,
                                                    const Vec3d& yi,
                                                    const Vec3d& zi,
                                                    const Vec3d& xf,
                                                    const Vec3d& yf);

/// Return type for the haversine helper containing the haversine distance and header between the start and end
/// points.
struct HaversineData
{
  LengthMeters distance;
  AngleRadians heading;
  AngleRadians pitch;
};

/// Given the start and end Geodetic Locations, returns the Haversine distance and heading.
[[nodiscard]] HaversineData haversine(const GeodeticWorldLocation& start, const GeodeticWorldLocation& end);

/// Computes the hypotenuse given the sides
[[nodiscard]] f64 hypot(f64 var1, f64 var2) noexcept;

/// Returns true if any of the components of the entity velocity is bigger than 0.1 m/s
[[nodiscard]] bool hasVelocity(const Velocity& velocity) noexcept;

/// Returns true if any of the components of the entity acceleration is bigger than 0.1 m/s2
[[nodiscard]] bool hasAcceleration(const Acceleration& acceleration) noexcept;

/// Returns true if any of the components of the entity angular velocity is bigger than 1e-3 rad/s
[[nodiscard]] bool hasAngularVelocity(const AngularVelocity& omega) noexcept;

}  // namespace sen::util

#endif  // SEN_LIBS_UTIL_SRC_DR_UTILS_H
