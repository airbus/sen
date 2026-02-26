// === algorithms.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_UTIL_DR_ALGORITHMS_H
#define SEN_UTIL_DR_ALGORITHMS_H

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/quantity.h"
#include "sen/core/base/timestamp.h"

// std
#include <array>

namespace sen::util
{

/// \addtogroup dr
/// @{

/// Length in meters
SEN_NON_RANGED_QUANTITY(LengthMeters, f64)

/// Velocity in meters per second
SEN_NON_RANGED_QUANTITY(VelocityMetersPerSecond, f32)

/// Acceleration in meters per second squared
SEN_NON_RANGED_QUANTITY(AccelerationMetersPerSecondSquared, f32)

/// Angle in radians
SEN_NON_RANGED_QUANTITY(AngleRadians, f32)

/// AngularVelocity in radians per second
SEN_NON_RANGED_QUANTITY(AngularVelocityRadiansPerSecond, f32)

/// AngularAcceleration in radians per second squared
SEN_NON_RANGED_QUANTITY(AngularAccelerationRadiansPerSecondSquared, f32)

/// Latitude in degrees
SEN_RANGED_QUANTITY(LatitudeDegrees, f64, -90.0, 90.0)

/// Longitude in degrees
SEN_RANGED_QUANTITY(LongitudeDegrees, f64, -180.0, 180.0)

/// Time quantity in seconds
SEN_NON_RANGED_QUANTITY(TimeSeconds, f64)

/// Non-dimensional damping coefficient
SEN_NON_RANGED_QUANTITY(DampingCoefficient, f64)

/// Dead Reckoning configuration.
struct DrConfig
{
  /// If true, the position and orientation of the input data is smoothed removing noise
  bool smoothing = true;

  ///  No smoothing is performed for displacements bigger than this distance.
  LengthMeters maxDistance = 100000.0;

  /// No smoothing is performed for time deltas bigger than this duration
  sen::Duration maxDeltaTime {std::chrono::seconds(1)};

  /// Maximum time interval used to update the smoothed solution. It is used to prevent the smoothed solution from
  /// becoming unstable.
  sen::Duration smoothingInterval {std::chrono::milliseconds(20)};

  /// Convergence time for the smoothed position to match the updated position
  sen::Duration positionConvergenceTime {std::chrono::milliseconds(500)};

  /// Damping coefficient for the smoothed position solution.
  DampingCoefficient positionDamping = 1.0;

  /// Convergence time for the smoothed orientation to match the updated orientation
  sen::Duration orientationConvergenceTime {std::chrono::milliseconds(50)};

  /// Damping coefficient for the smoothed orientation solution.
  DampingCoefficient orientationDamping = 20.0;
};

/// World Location struct
struct Location
{
  LengthMeters x;
  LengthMeters y;
  LengthMeters z;
};

/// World location in geodetic coordinates
struct GeodeticWorldLocation
{
  LatitudeDegrees latitude;
  LongitudeDegrees longitude;
  LengthMeters altitude;
};

/// Orientation struct
struct Orientation
{
  AngleRadians psi;
  AngleRadians theta;
  AngleRadians phi;
};

/// Velocity struct
struct Velocity
{
  VelocityMetersPerSecond x;
  VelocityMetersPerSecond y;
  VelocityMetersPerSecond z;
};

/// Acceleration struct
struct Acceleration
{
  AccelerationMetersPerSecondSquared x;
  AccelerationMetersPerSecondSquared y;
  AccelerationMetersPerSecondSquared z;
};

/// AngularVelocity struct
struct AngularVelocity
{
  AngularVelocityRadiansPerSecond x;
  AngularVelocityRadiansPerSecond y;
  AngularVelocityRadiansPerSecond z;
};

/// AngularAcceleration struct
struct AngularAcceleration
{
  AngularAccelerationRadiansPerSecondSquared x;
  AngularAccelerationRadiansPerSecondSquared y;
  AngularAccelerationRadiansPerSecondSquared z;
};

/// Situation structure with the following parameters:
struct Situation
{
  /// When true, no extrapolation is performed because the entity is frozen.
  bool isFrozen = false;

  /// TimeStamp of the instant when the situation is computed.
  sen::TimeStamp timeStamp {};

  ///  Position in ECEF.
  Location worldLocation {};

  /// Orientation of the body reference system (x forward, y right, z down) with respect to ECEF.
  Orientation orientation {};

  /// Velocity vector with respect to ECEF or body reference system (depending on
  /// the reference system of the algorithm extrapolated).
  Velocity velocityVector {};

  /// Angular velocity vector with respect to body reference system.
  AngularVelocity angularVelocity {};

  /// Acceleration vector with respect to ECEF or body reference system (depending on
  /// the reference system of the algorithm extrapolated).
  Acceleration accelerationVector {};

  /// Angular acceleration vector with respect to body reference system.
  AngularAcceleration angularAcceleration {};
};

/// GeodeticSituation structure with the following parameters:
struct GeodeticSituation
{
  /// When true, no extrapolation is performed because the entity is frozen.
  bool isFrozen = false;

  /// TimeStamp of the instant when the situation is computed.
  sen::TimeStamp timeStamp {};

  /// World Location in Geodetic (Latitude, Longitude, Altitude).
  GeodeticWorldLocation worldLocation {};

  /// Orientation of the body reference system (x forward, y right, z down)
  /// with respect to NED (North - East - Down).
  Orientation orientation {};

  /// Velocity vector with respect to NED.
  Velocity velocityVector {};

  /// Angular velocity vector with respect to body-reference system.
  AngularVelocity angularVelocity {};

  ///  Acceleration vector with respect to NED.
  Acceleration accelerationVector {};

  /// Angular acceleration vector with respect to body-reference system.
  AngularAcceleration angularAcceleration {};
};

/// Enumeration of the different Spatial algorithms
enum class SpatialAlgorithm
{
  drStatic = 0,
  drFPW = 1,
  drRPW = 2,
  drRVW = 3,
  drFVW = 4,
  drFPB = 5,
  drRPB = 6,
  drRVB = 7,
  drFVB = 8,
};

constexpr std::array<SpatialAlgorithm, 4U> bodyAlgorithms {SpatialAlgorithm::drFPB,
                                                           SpatialAlgorithm::drRPB,
                                                           SpatialAlgorithm::drRVB,
                                                           SpatialAlgorithm::drFVB};

/// Returns the extrapolated situation using the FPW algorithm
[[nodiscard]] Situation drFpw(const Situation& value, sen::TimeStamp time) noexcept;

/// Returns the extrapolated situation using the FPB algorithm. The velocity is expressed in body coordinates.
[[nodiscard]] Situation drFpb(const Situation& value, sen::TimeStamp time) noexcept;

/// Returns the extrapolated situation using the RPW algorithm
[[nodiscard]] Situation drRpw(const Situation& value, sen::TimeStamp time) noexcept;

/// Returns the extrapolated situation using the RPB algorithm. The input situation should be expressed in body
/// coordinates and the returned situation in world coordinates.
[[nodiscard]] Situation drRpb(const Situation& value, sen::TimeStamp time) noexcept;

/// Returns the extrapolated situation using the RVW algorithm
[[nodiscard]] Situation drRvw(const Situation& value, sen::TimeStamp time) noexcept;

/// Returns the extrapolated situation using the RVB algorithm
[[nodiscard]] Situation drRvb(const Situation& value, sen::TimeStamp time) noexcept;

/// Returns the extrapolated situation using the FVW algorithm
[[nodiscard]] Situation drFvw(const Situation& value, sen::TimeStamp time) noexcept;

/// Returns the extrapolated situation using the FVB algorithm
[[nodiscard]] Situation drFvb(const Situation& value, sen::TimeStamp time) noexcept;

/// @}

}  // namespace sen::util

#endif  // SEN_UTIL_DR_ALGORITHMS_H
