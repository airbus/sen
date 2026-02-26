// === dead_reckoner_impl.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/util/dr/detail/dead_reckoner_impl.h"

// sen
#include "sen/core/base/numbers.h"
#include "sen/util/dr/algorithms.h"

// implementation
#include "constants.h"
#include "quat.h"
#include "utils.h"
#include "vec3.h"

// std
#include <algorithm>
#include <cmath>

namespace sen::util::impl
{

GeodeticWorldLocation toLla(const Location& worldLocation) noexcept
{
  GeodeticWorldLocation result {};

  // used constants
  constexpr auto e4 = eccentricitySquared * eccentricitySquared;
  constexpr auto e2m = (1.0 - earthFlattening) * (1.0 - earthFlattening);

  // projection of the point onto the equatorial plane
  const auto R = hypot(worldLocation.x, worldLocation.y);  // NOLINT[readability-identifier-naming]

  // distance to the center of the earth
  result.altitude = hypot(R, worldLocation.z);

  const auto p = (R / earthSemiMajorAxis) * (R / earthSemiMajorAxis);
  const auto q = e2m * (worldLocation.z / earthSemiMajorAxis) * (worldLocation.z / earthSemiMajorAxis);
  const auto r = (p + q - e4) / 6.0;

  if (e4 * q != 0.0 || r > 0.0)
  {
    const auto S = e4 * p * q / 4.0;  // NOLINT[readability-identifier-naming]
    const auto r2 = r * r;
    const auto r3 = r * r2;
    const auto disc = S * (2.0 * r3 + S);
    auto u = r;

    if (disc >= 0.0)
    {
      auto T3 = r3 + S;  // NOLINT[readability-identifier-naming]
      T3 += T3 < 0.0 ? -std::sqrt(disc) : std::sqrt(disc);
      const auto T = std::cbrt(T3);  // NOLINT[readability-identifier-naming]
      u += T + (T != 0.0 ? r2 / T : 0.0);
    }
    else
    {
      const auto ang = std::atan2(std::sqrt(-disc), -(S + r3));
      u += 2.0 * r * std::cos(ang / 3.0);
    }
    const auto v = std::sqrt(u * u + e4 * q);
    const auto uv = u < 0.0 ? e4 * q / (v - u) : u + v;
    const auto w = std::max(0.0, eccentricitySquared * (uv - q) / (2.0 * v));
    const auto k = uv / (std::sqrt(uv + w * w) + w);
    const auto k1 = k;
    const auto k2 = k + eccentricitySquared;
    const auto d = k1 * R / k2;
    result.latitude = std::atan2(worldLocation.z / k1, R / k2);
    result.altitude = (1.0 - e2m / k1) * hypot(d, worldLocation.z);
  }
  else
  {
    const auto zz = std::sqrt((e4 - p) / eccentricitySquared);
    const auto xx = std::sqrt(p);
    result.latitude = std::atan2(zz, xx);

    if (worldLocation.z < 0.0)
    {
      result.latitude = -result.latitude;
    }
    result.altitude = -earthSemiMajorAxis * e2m * hypot(xx, zz) / eccentricitySquared;
  }
  result.latitude = toDeg(result.latitude);
  result.longitude = toDeg(-atan2(-worldLocation.y, worldLocation.x));

  return result;
}

Location toEcef(const GeodeticWorldLocation& latLonAlt) noexcept
{
  Location result {};

  auto lon = latLonAlt.longitude.get();
  lon = lon >= 180.0 ? lon - 360.0 : lon < -180.0 ? lon + 360.0 : lon;

  const auto latRad = toRad(latLonAlt.latitude);
  const auto lonRad = toRad(lon);
  const auto sineLat = std::sin(latRad);
  const auto cosLat = std::abs(latLonAlt.latitude) == 90.0 ? 0.0 : cos(latRad);

  // compute the prime vertical radius of curvature (rn)
  const auto rn = earthSemiMajorAxis / std::sqrt(1.0 - eccentricitySquared * sineLat * sineLat);

  // compute ECEF coordinates
  result.z = ((1.0 - eccentricitySquared) * rn + latLonAlt.altitude) * sineLat;
  result.x = (rn + latLonAlt.altitude) * cosLat;
  result.y = result.x * (lon == -180.0 ? 0.0 : std::sin(lonRad));
  result.x = result.x * (std::abs(lon) == 90.0 ? 0.0 : std::cos(lonRad));

  return result;
}

Orientation ecefToNed(const Orientation& value, const GeodeticWorldLocation& latLonAlt) noexcept
{
  // compute the NED trihedron at the point with the input latLonAlt
  const auto [x0, y0, z0] = getNedTrihedron(latLonAlt);

  // quaternion containing the orientation with respect to the ECEF coordinates
  const Quatd inputOrientation {value.psi, value.theta, value.phi};

  // i and j unit vectors transformed with the input orientation quaternion
  const auto xf = inputOrientation * Vec3d {1, 0, 0};
  const auto yf = inputOrientation * Vec3d {0, 1, 0};

  // calculate euler angles of the orientation with respect to NED using the transformed unit vectors and
  // trigonometry
  return eulerAnglesFromTrihedrons(x0, y0, z0, xf, yf);
}

Velocity ecefToNed(const Velocity& value, const GeodeticWorldLocation& geodeticPosition) noexcept
{
  return toVelocity(ecefToNed(fromVelocity(value), geodeticPosition));
}

Acceleration ecefToNed(const Acceleration& value, const GeodeticWorldLocation& geodeticPosition) noexcept
{
  return toAcceleration(ecefToNed(fromAcceleration(value), geodeticPosition));
}

Orientation nedToEcef(const Orientation& value, const GeodeticWorldLocation& latLonAlt) noexcept
{
  // compute the NED trihedron at the point with the input lat and lon
  const auto [x0, y0, z0] = getNedTrihedron(latLonAlt);

  // compute final x and y vectors resulting from the transformation of the orientation with respect to NED
  const auto xf = Quatd {value.theta, y0} * Quatd {value.psi, z0} * x0;
  const auto yf = Quatd {value.phi, x0} * Quatd {value.psi, z0} * y0;

  // ECEF unit vectors
  const Vec3d xi {1, 0, 0};
  const Vec3d yi {0, 1, 0};
  const Vec3d zi {0, 0, 1};

  // euler angles to transform the ECEF trihedron to the final vectors
  return eulerAnglesFromTrihedrons(xi, yi, zi, xf, yf);
}

Velocity nedToEcef(const Velocity& value, const GeodeticWorldLocation& location) noexcept
{
  return toVelocity(nedToEcef(fromVelocity(value), location));
}

Acceleration nedToEcef(const Acceleration& value, const GeodeticWorldLocation& location) noexcept
{
  return toAcceleration(nedToEcef(fromAcceleration(value), location));
}

Velocity bodyToNed(const Velocity& value, const Orientation& orientationNed) noexcept
{
  return toVelocity(bodyToNed(fromVelocity(value), orientationNed));
}

AngularVelocity bodyToNed(const AngularVelocity& value, const Orientation& orientationNed) noexcept
{
  return toAngularVelocity(bodyToNed(fromAngularVelocity(value), orientationNed));
}

AngularAcceleration bodyToNed(const AngularAcceleration& value, const Orientation& orientationNed) noexcept
{
  return toAngularAcceleration(bodyToNed(fromAngularAcceleration(value), orientationNed));
}

Acceleration bodyToNed(const Acceleration& value, const Orientation& orientationNed) noexcept
{
  return toAcceleration(bodyToNed(fromAcceleration(value), orientationNed));
}

void smoothImpl(Situation& situation, const Situation& update, const DrConfig& config)
{
  auto delta = update.timeStamp - situation.timeStamp;

  // update the smoothed situation using steps not exceeding the smoothing interval
  while (delta.toSeconds() > 1e-6)
  {
    const auto step = std::min(config.smoothingInterval, delta);

    // update smoothed location
    {
      situation.worldLocation = extrapolateLocationWorld(situation, step);
      situation.velocityVector = extrapolateVelocity(situation.velocityVector, situation.accelerationVector, step);

      const auto p0 = fromWorldLocation(situation.worldLocation);
      const auto v0 = fromVelocity(situation.velocityVector);
      const auto p1 = fromWorldLocation(update.worldLocation);
      const auto v1 = fromVelocity(update.velocityVector);
      const auto a1 = fromAcceleration(update.accelerationVector);

      const auto t = config.positionConvergenceTime.toSeconds();

      situation.accelerationVector = toAcceleration((p1 + v1 * t + a1 * 0.5 * t * t - p0 - v0 * t) * 2.0 / (t * t) +
                                                    (v1 - v0) * config.positionDamping);

      // zeroize smooth situation values in case of stopped entity
      if (!hasVelocity(situation.velocityVector) && !hasAcceleration(situation.accelerationVector))
      {
        situation.worldLocation = update.worldLocation;
        situation.velocityVector = {};
        situation.accelerationVector = {};
      }
    }

    // update smoothed orientation
    {
      situation.orientation =
        extrapolateOrientation(situation.orientation, step, situation.angularVelocity, situation.angularAcceleration);
      situation.angularVelocity =
        toAngularVelocity(fromAngularVelocity(situation.angularVelocity) +
                          fromAngularAcceleration(situation.angularAcceleration) * step.toSeconds());

      // compute the delta rotation in angle/axis format
      f64 rotationAngle = 0;
      Vec3d rotationAxis {};
      const auto q0 = fromOrientationToQuat(situation.orientation);
      const auto q1 = fromOrientationToQuat(update.orientation);
      const auto q01 = q0.inverse() * q1;
      q01.getRotate(rotationAngle, rotationAxis);

      const auto deltaTheta = worldToBody(rotationAxis * rotationAngle, situation.orientation);
      const auto omega0 = fromAngularVelocity(situation.angularVelocity);
      const auto omega1 = fromAngularVelocity(update.angularVelocity);

      const auto t = config.orientationConvergenceTime.toSeconds();

      situation.angularAcceleration = toAngularAcceleration((deltaTheta + (omega1 - omega0) * t) * 2.0 / (t * t) +
                                                            (omega1 - omega0) * config.orientationDamping);
    }

    delta -= step;
  }

  situation.timeStamp = update.timeStamp;
}

LengthMeters computeDistance(const Location& start, const Location& end)
{
  return (fromWorldLocation(end) - fromWorldLocation(start)).length();
}

}  // namespace sen::util::impl
