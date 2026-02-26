// === dead_reckoner_impl.h ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_UTIL_DR_DETAIL_DEAD_RECKONER_IMPL_H
#define SEN_UTIL_DR_DETAIL_DEAD_RECKONER_IMPL_H

// sen
#include "sen/util/dr/algorithms.h"

namespace sen::util::impl
{

/// \addtogroup dr
/// @{

/// Translates a Location struct to a RPR WorldLocation given as template argument
template <typename T>
[[nodiscard]] T toRpr(const Location& value);

/// Translates an Orientation struct to a RPR Orientation given as template argument
template <typename T>
[[nodiscard]] T toRpr(const Orientation& value);

/// Translates a Velocity struct to a RPR Velocity given as template argument
template <typename T>
[[nodiscard]] T toRpr(const Velocity& value);

/// Translates an Acceleration struct to a RPR Acceleration given as template argument
template <typename T>
[[nodiscard]] T toRpr(const Acceleration& value);

/// Translates an AngularVelocity struct to a RPR AngularVelocity given as template argument
template <typename T>
[[nodiscard]] T toRpr(const AngularVelocity& value);

/// Translates to a Location struct from a RPR WorldLocation
template <typename T>
[[nodiscard]] Location fromRprLocation(const T& value);

/// Translates to an Orientation struct from a RPR Orientation
template <typename T>
[[nodiscard]] Orientation fromRprOrientation(const T& value);

/// Translates to a Velocity struct from a RPR Velocity
template <typename T>
[[nodiscard]] Velocity fromRprVelocity(const T& value);

/// Translates to Acceleration struct from a RPR Acceleration
template <typename T>
[[nodiscard]] Acceleration fromRprAcceleration(const T& value);

/// Translates to AngularVelocity struct from a RPR AngularVelocity
template <typename T>
[[nodiscard]] AngularVelocity fromRprAngularVelocity(const T& value);

/// Translates from ECEF Location to Geodetic (LatLonAlt) Location
[[nodiscard]] GeodeticWorldLocation toLla(const Location& worldLocation) noexcept;

/// Translates from Geodetic (LatLonAlt) to ECEF WorldLocation coordinates.
[[nodiscard]] Location toEcef(const GeodeticWorldLocation& latLonAlt) noexcept;

/// Translates the Orientation in euler angles from ECEF to NED using quaternions
[[nodiscard]] Orientation ecefToNed(const Orientation& value, const GeodeticWorldLocation& latLonAlt) noexcept;

/// Translates Velocity vectors expressed in ECEF coordinates to NED coordinates
[[nodiscard]] Velocity ecefToNed(const Velocity& value, const GeodeticWorldLocation& geodeticPosition) noexcept;

/// Translates Acceleration vectors expressed in ECEF coordinates to NED coordinates
[[nodiscard]] Acceleration ecefToNed(const Acceleration& value, const GeodeticWorldLocation& geodeticPosition) noexcept;

/// Translates the Orientation in euler angles from NED to ECEF using quaternions
[[nodiscard]] Orientation nedToEcef(const Orientation& value, const GeodeticWorldLocation& latLonAlt) noexcept;

/// Translates a Velocity vector from NED to ECEF coordinates
[[nodiscard]] Velocity nedToEcef(const Velocity& value, const GeodeticWorldLocation& location) noexcept;

/// Translates an Acceleration vector from NED to ECEF coordinates
[[nodiscard]] Acceleration nedToEcef(const Acceleration& value, const GeodeticWorldLocation& location) noexcept;

/// Translates Velocity vectors expressed in body coordinates to NED coordinates
[[nodiscard]] Velocity bodyToNed(const Velocity& value, const Orientation& orientationNed) noexcept;

/// Translates AngularVelocity vectors expressed in body coordinates to NED coordinates
[[nodiscard]] AngularVelocity bodyToNed(const AngularVelocity& value, const Orientation& orientationNed) noexcept;

/// Translates AngularAcceleration vectors expressed in body coordinates to NED coordinates
[[nodiscard]] AngularAcceleration bodyToNed(const AngularAcceleration& value,
                                            const Orientation& orientationNed) noexcept;

/// Translates acceleration vectors expressed in body coordinates to NED coordinates
[[nodiscard]] Acceleration bodyToNed(const Acceleration& value, const Orientation& orientationNed) noexcept;

/// Updates the smoothed situation, both in position and orientation
void smoothImpl(Situation& situation, const Situation& update, const DrConfig& config);

/// Computes the distance between two World locations
[[nodiscard]] LengthMeters computeDistance(const Location& start, const Location& end);

/// @}

//-------------------------------------------------------------------------------------------------------------------
// Inline implementation
//-------------------------------------------------------------------------------------------------------------------

template <typename T>
inline T toRpr(const Location& value)
{
  return {value.x, value.y, value.z};
}

template <typename T>
inline T toRpr(const Orientation& value)
{
  return {value.psi, value.theta, value.phi};
}

template <typename T>
inline T toRpr(const Velocity& value)
{
  return {value.x, value.y, value.z};
}

template <typename T>
inline T toRpr(const Acceleration& value)
{
  return {value.x, value.y, value.z};
}

template <typename T>
inline T toRpr(const AngularVelocity& value)
{
  return {value.x, value.y, value.z};
}

template <typename T>
inline Location fromRprLocation(const T& value)
{
  return {value.x, value.y, value.z};
}

template <typename T>
inline Orientation fromRprOrientation(const T& value)
{
  return {value.psi, value.theta, value.phi};
}

/// Translates to Velocity struct from the rpr::VelocityVectorStruct type given as template parameter
template <typename T>
inline Velocity fromRprVelocity(const T& value)
{
  return {value.xVelocity, value.yVelocity, value.zVelocity};
}

/// Translates to Acceleration struct from the rpr::AccelerationVectorStruct type given as template parameter
template <typename T>
inline Acceleration fromRprAcceleration(const T& value)
{
  return {value.xAcceleration, value.yAcceleration, value.zAcceleration};
}

/// Translates to AngularVelocity struct from the rpr::AngularVelocityStruct type given as template parameter
template <typename T>
inline AngularVelocity fromRprAngularVelocity(const T& value)
{
  return {value.xAngularVelocity, value.yAngularVelocity, value.zAngularVelocity};
}

}  // namespace sen::util::impl

#endif  // SEN_UTIL_DR_DETAIL_DEAD_RECKONER_IMPL_H
