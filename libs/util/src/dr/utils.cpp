// === utils.cpp =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "utils.h"

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/util/dr/algorithms.h"

// implementation
#include "constants.h"
#include "mat3.h"
#include "quat.h"
#include "vec3.h"

// std
#include <array>
#include <cmath>

namespace sen::util
{

Location toWorldLocation(const Vec3d& value) { return {value.getX(), value.getY(), value.getZ()}; }

Vec3d fromWorldLocation(const Location& value) { return {value.x, value.y, value.z}; }

Vec3d fromVelocity(const Velocity& value)
{
  return {std_util::checkedConversion<double>(value.x),
          std_util::checkedConversion<double>(value.y),
          std_util::checkedConversion<double>(value.z)};
}

Velocity toVelocity(const Vec3d& value) { return {value.getX(), value.getY(), value.getZ()}; }

Vec3d fromAcceleration(const Acceleration& value)
{
  return {std_util::checkedConversion<double>(value.x),
          std_util::checkedConversion<double>(value.y),
          std_util::checkedConversion<double>(value.z)};
}

Acceleration toAcceleration(const Vec3d& value) { return {value.getX(), value.getY(), value.getZ()}; }

AngularAcceleration toAngularAcceleration(const Vec3d& value) { return {value.getX(), value.getY(), value.getZ()}; }

Vec3d fromAngularVelocity(const AngularVelocity& value)
{
  return {std_util::checkedConversion<double>(value.x),
          std_util::checkedConversion<double>(value.y),
          std_util::checkedConversion<double>(value.z)};
}

Vec3d fromAngularAcceleration(const AngularAcceleration& value)
{
  return {std_util::checkedConversion<double>(value.x),
          std_util::checkedConversion<double>(value.y),
          std_util::checkedConversion<double>(value.z)};
}

AngularVelocity toAngularVelocity(const Vec3d& value) { return {value.getX(), value.getY(), value.getZ()}; }

Orientation toOrientation(const Vec3d& value) { return {value.getX(), value.getY(), value.getZ()}; }

Vec3d fromOrientationToVec(const Orientation& value)
{
  return {std_util::checkedConversion<double>(value.psi),
          std_util::checkedConversion<double>(value.theta),
          std_util::checkedConversion<double>(value.phi)};
}

Quatd fromOrientationToQuat(const Orientation& value)
{
  return {std_util::checkedConversion<double>(value.psi),
          std_util::checkedConversion<double>(value.theta),
          std_util::checkedConversion<double>(value.phi)};
}

Vec3d nedToEcef(const Vec3d& value, const GeodeticWorldLocation& latLonAlt) noexcept
{
  return Quatd {toRad(latLonAlt.longitude), -halfPi - toRad(latLonAlt.latitude), 0.0} * value;
}

Vec3d ecefToNed(const Vec3d& value, const GeodeticWorldLocation& latLonAlt) noexcept
{
  return Quatd {toRad(latLonAlt.longitude), -halfPi - toRad(latLonAlt.latitude), 0.0}.inverse() * value;
}

Vec3d bodyToNed(const Vec3d& value, const Orientation& orientationNed) noexcept
{
  return fromOrientationToQuat(orientationNed) * value;
}

Vec3d nedToBody(const Vec3d& value, const Orientation& orientationNed) noexcept
{
  return fromOrientationToQuat(orientationNed).inverse() * value;
}

f64 toRad(f64 value) noexcept { return value * pi / 180.0; }

f64 toDeg(f64 value) noexcept { return value * 180.0 / pi; }

Mat3d makeR1(const Vec3d& omega, f64 delta)
{
  // handle case where omega is null
  if (omega.length() == 0)
  {
    return Mat3d::makeIdentity() * delta;
  }
  const auto omegaLen = omega.length();
  const auto omegaLen2 = omegaLen * omegaLen;
  const auto angle = omegaLen * delta;
  const auto sinAngle = sin(angle);
  const auto cosAngle = cos(angle);

  return (Mat3d::makeOmegaOmegaTranspose(omega) * ((angle - sinAngle) / (omegaLen2 * omegaLen))) +
         (Mat3d::makeIdentity() * (sinAngle / omegaLen)) +
         (Mat3d::makeSkewMatrix(omega) * ((1.0 - cosAngle) / omegaLen2));
}

Mat3d makeR2(const Vec3d& omega, f64 delta)
{
  // handle case where omega is null
  if (omega.length() == 0)
  {
    return Mat3d::makeIdentity() * 0.5 * delta * delta;
  }

  const auto wMod = omega.length();
  const auto wMod2 = wMod * wMod;
  const auto angle = wMod * delta;
  const auto sinAngle = sin(angle);
  const auto cosAngle = cos(angle);

  return (Mat3d::makeOmegaOmegaTranspose(omega) *
          ((0.5 * wMod2 * delta * delta - cosAngle - wMod * delta * sinAngle + 1.0) / (wMod2 * wMod2))) +
         (Mat3d::makeIdentity() * ((cosAngle + wMod * delta * sinAngle - 1.0) / wMod2)) +
         (Mat3d::makeSkewMatrix(omega) * ((sinAngle - wMod * delta * cosAngle) / (wMod2 * wMod)));
}

Vec3d bodyToWorld(const Vec3d& value, const Orientation& bodyEulerAngles) noexcept
{
  return fromOrientationToQuat(bodyEulerAngles) * value;
}

Vec3d worldToBody(const Vec3d& value, const Orientation& bodyEulerAngles) noexcept
{
  return fromOrientationToQuat(bodyEulerAngles).inverse() * value;
}

Location extrapolateLocationWorld(const Situation& value, sen::Duration delta) noexcept
{
  const auto deltaSeconds = delta.toSeconds();

  return toWorldLocation(fromWorldLocation(value.worldLocation) + fromVelocity(value.velocityVector) * deltaSeconds +
                         fromAcceleration(value.accelerationVector) * 0.5 * deltaSeconds * deltaSeconds);
}

Location extrapolateLocationBody(const Situation& value, sen::Duration delta) noexcept
{
  const auto deltaSeconds = delta.toSeconds();

  const auto orientationQuat = fromOrientationToQuat(value.orientation);

  // matrices used for the integration of the position in body coordinates
  const auto r1 = makeR1(fromAngularVelocity(value.angularVelocity), deltaSeconds);
  const auto r2 = makeR2(fromAngularVelocity(value.angularVelocity), deltaSeconds);

  return toWorldLocation(fromWorldLocation(value.worldLocation) +
                         orientationQuat * (r1 * fromVelocity(value.velocityVector)) +
                         orientationQuat * (r2 * fromAcceleration(value.accelerationVector)));
}

Velocity extrapolateVelocity(const Velocity& value, const Acceleration& acceleration, sen::Duration delta) noexcept
{
  return toVelocity(fromVelocity(value) + fromAcceleration(acceleration) * delta.toSeconds());
}

Orientation extrapolateOrientation(const Orientation& value,
                                   sen::Duration delta,
                                   const AngularVelocity& angularVelocity,
                                   const AngularAcceleration& angularAcceleration) noexcept
{
  const auto deltaSeconds = delta.toSeconds();

  // compute equivalent angular velocity in world coordinates (equivalent to ang vel and angular acceleration in the
  // delta extrapolated)
  const auto eqOmega = bodyToWorld(fromAngularVelocity(angularVelocity), value) +
                       bodyToWorld(fromAngularAcceleration(angularAcceleration), value) * 0.5 * deltaSeconds;

  // compute orientation quaternion and rotate it
  auto orientationQuat = fromOrientationToQuat(value);
  orientationQuat.makeRotate(eqOmega.length() * deltaSeconds, eqOmega);

  return toOrientation(orientationQuat.getRotateInEulerYPB());
}

std::array<Vec3d, 3U> getNedTrihedron(const GeodeticWorldLocation& location)
{
  // NED vectors expressed in ECEF coordinates in the 0 N, 0 E.
  const Vec3d n0 {0, 0, 1};
  const Vec3d e0 {0, 1, 0};
  const Vec3d d0 {-1, 0, 0};

  // quaternion of the transformation from the NED vectors in the 0 N, 0E to the NED vectors at a point with the
  // input latitude and longitude
  const Quatd tranNed {toRad(location.longitude), -toRad(location.latitude), 0.0};

  // return the transformed NED vectors
  return {tranNed * n0, tranNed * e0, tranNed * d0};
}

Orientation eulerAnglesFromTrihedrons(const Vec3d& xi,
                                      const Vec3d& yi,
                                      const Vec3d& zi,
                                      const Vec3d& xf,
                                      const Vec3d& yf)
{
  // compute yaw and pitch first with the projections of the xf vector with respect to the initial trihedron
  const auto yaw = atan2(xf * yi, xf * xi);
  const auto pitch = atan2(-xf * zi, sqrt((xf * xi) * (xf * xi) + (xf * yi) * (xf * yi)));

  // compute the intermediate y and z axis that result from applying the yaw and the pitch to the initial trihedron
  // (y2 and z2)
  const auto y2 = Quatd {yaw, zi} * yi;
  const auto z2 = Quatd {pitch, y2} * zi;

  const auto roll = atan2(yf * z2, yf * y2);

  return {yaw, pitch, roll};
}

HaversineData haversine(const GeodeticWorldLocation& start, const GeodeticWorldLocation& end)
{

  // to radians
  const auto startLatRad = toRad(start.latitude);
  const auto endLatRad = toRad(end.latitude);

  const auto cosStartLat = cos(startLatRad);
  const auto sinStartLat = sin(startLatRad);
  const auto cosEndLat = cos(endLatRad);
  const auto sinEndLat = sin(endLatRad);

  const auto deltaLon = toRad(end.longitude) - toRad(start.longitude);
  const auto cosDeltaLon = cos(deltaLon);
  const auto sinDeltaLon = sin(deltaLon);
  const auto sinHalfDeltaLon = sin(deltaLon / 2.0);

  const auto deltaLat = endLatRad - startLatRad;
  const auto sinHalfDeltaLat = sin(deltaLat / 2.0);

  // terms involved in the computation of the Geoid angle
  const auto a = sinHalfDeltaLat * sinHalfDeltaLat + cosStartLat * cosEndLat * sinHalfDeltaLon * sinHalfDeltaLon;
  const auto c = 2.0 * atan2(sqrt(a), sqrt(1 - a));

  // radius of the WGS84 model in meters
  constexpr f64 rwgs84 = 6371000;
  // distance calculation discarding changes in altitude
  const auto surfaceDistance = rwgs84 * c;

  // heading of the trajectory between the two points
  const auto heading = atan2(sinDeltaLon * cosEndLat, cosStartLat * sinEndLat - sinStartLat * cosEndLat * cosDeltaLon);

  // pitch of the trajectory between the two points
  const auto pitch = atan2((end.altitude - start.altitude), surfaceDistance);

  return HaversineData {surfaceDistance, heading, pitch};
}

f64 hypot(f64 var1, f64 var2) noexcept { return sqrt(var1 * var1 + var2 * var2); }

bool hasVelocity(const Velocity& velocity) noexcept
{
  return std::abs(velocity.x - 0.0f) > epsilon || std::abs(velocity.y - 0.0f) > epsilon ||
         std::abs(velocity.z - 0.0f) > epsilon;
}

bool hasAcceleration(const Acceleration& acceleration) noexcept
{
  return std::abs(acceleration.x - 0.0f) > epsilon || std::abs(acceleration.y - 0.0f) > epsilon ||
         std::abs(acceleration.z - 0.0f) > epsilon;
}

bool hasAngularVelocity(const AngularVelocity& omega) noexcept
{
  constexpr f32 rotationEpsilon = 1e-3f;
  return std::abs(omega.x - 0.0f) > rotationEpsilon || std::abs(omega.y - 0.0f) > rotationEpsilon ||
         std::abs(omega.z - 0.0f) > rotationEpsilon;
}

}  // namespace sen::util
