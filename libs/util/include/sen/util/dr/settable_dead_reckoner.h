// === settable_dead_reckoner.h ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_UTIL_DR_SETTABLE_DEAD_RECKONER_H
#define SEN_UTIL_DR_SETTABLE_DEAD_RECKONER_H

#include "dead_reckoner.h"
#include "detail/settable_dead_reckoner_impl.h"
#include "sen/util/dr/algorithms.h"

// std
#include <cmath>

namespace sen::util
{

/// \addtogroup dr
/// @{

//--------------------------------------------------------------------------------------------------------------
// Types
//--------------------------------------------------------------------------------------------------------------

/// Enumeration of the reference system: world-centered or body-centered
enum class ReferenceSystem : u32
{
  world,
  body
};

/// Threshold configuration structure with the position error threshold (maximum distance between extrapolation and
/// data) and the entity dynamics (speed and changes of direction)
struct DrThreshold
{
  LengthMeters distanceThreshold {1.0};
  AngleRadians orientationThreshold {0.05f};
  ReferenceSystem referenceSystem {ReferenceSystem::world};
};

/// Allows the user to get the extrapolated situation of an object and to set the Spatial when the error of the
/// extrapolation exceeds a configurable threshold.
template <typename T>
/// @tparam T rpr::BaseEntityBase or a subclass of it
class SettableDeadReckoner: public DeadReckonerTemplateBase<T>
{
public:
  SEN_NOCOPY_NOMOVE(SettableDeadReckoner)

public:
  explicit SettableDeadReckoner(T& object, DrThreshold thresholds = {});
  ~SettableDeadReckoner() override = default;

public:
  using Parent = DeadReckonerTemplateBase<T>;
  using SpatialVariant = typename Parent::SpatialVariant;
  using StaticSpatial = typename Parent::StaticSpatial;
  using FpsSpatial = typename Parent::FpsSpatial;
  using RpsSpatial = typename Parent::RpsSpatial;
  using RvsSpatial = typename Parent::RvsSpatial;
  using FvsSpatial = typename Parent::FvsSpatial;
  using RprLocation = typename Parent::RprLocation;
  using RprOrientation = typename Parent::RprOrientation;
  using RprVelocity = typename Parent::RprVelocity;
  using RprAcceleration = typename Parent::RprAcceleration;
  using RprAngularVelocity = typename Parent::RprAngularVelocity;

public:
  /// Returns a mutable reference to the RPR object whose position is extrapolated
  [[nodiscard]] T& object() noexcept;

  /// Returns a constant reference to the RPR object whose position is extrapolated
  [[nodiscard]] const T& object() const noexcept;

public:
  /// Updates the spatial property of the object if the update period is exceeded or if the extrapolation error
  /// exceeds the threshold.
  void setSpatial(const Situation& situation);

  /// Updates the spatial property of the object from a Geodetic Situation input if the update period is exceeded or
  /// if the extrapolation error exceeds the threshold.
  void setSpatial(const GeodeticSituation& situation);

  /// Directly sets the frozen state of the object's spatial to true/false. The timestamp is needed to coherently
  /// apply the new frozen state to the current extrapolated situation.
  void setFrozen(sen::TimeStamp timeStamp, bool value);

private:
  /// Returns the spatial corresponding to the Situation given as input. The type of spatial is automatically
  /// selected given the reference system received in the threshold config and the dynamics of the entity
  SpatialVariant toSpatialVariant(const Situation& situation, const DrThreshold& threshold);

private:
  T& obj_;
  sen::TimeStamp lastTimeStamp_;
  DrThreshold threshold_;
  const f64 orientationThresholdCos;  // constant used to determine if the orientation threshold has been exceeded
};

/// @}

//-------------------------------------------------------------------------------------------------------------------
// Inline implementation
//-------------------------------------------------------------------------------------------------------------------

template <typename T>
inline SettableDeadReckoner<T>::SettableDeadReckoner(T& object, DrThreshold thresholds)
  : obj_ {object}
  , threshold_ {thresholds}
  , orientationThresholdCos {cos(static_cast<f64>(threshold_.orientationThreshold) * 0.5)}
{
}

template <typename T>
inline T& SettableDeadReckoner<T>::object() noexcept
{
  return obj_;
}

template <typename T>
inline const T& SettableDeadReckoner<T>::object() const noexcept
{
  return obj_;
}

template <typename T>
inline void SettableDeadReckoner<T>::setSpatial(const Situation& situation)
{
  const auto extrapolation = Parent::extrapolate(obj_.getSpatial(), situation.timeStamp, lastTimeStamp_);
  if (impl::maxDistanceExceeded(situation.worldLocation, extrapolation.worldLocation, threshold_.distanceThreshold) ||
      impl::maxRotationExceeded(situation.orientation, extrapolation.orientation, orientationThresholdCos))
  {
    lastTimeStamp_ = situation.timeStamp;
    obj_.setNextSpatial(toSpatialVariant(situation, threshold_));
  }
}

template <typename T>
inline void SettableDeadReckoner<T>::setSpatial(const GeodeticSituation& situation)
{
  // translate input geodetic situation to standard reference system
  const auto ecefPosition = impl::toEcef(situation.worldLocation);
  const auto ecefOrientation = impl::nedToEcef(situation.orientation, situation.worldLocation);
  const auto extrapolation = Parent::extrapolate(obj_.getSpatial(), situation.timeStamp, lastTimeStamp_);
  if (impl::maxDistanceExceeded(ecefPosition, extrapolation.worldLocation, threshold_.distanceThreshold) ||
      impl::maxRotationExceeded(ecefOrientation, extrapolation.orientation, orientationThresholdCos))
  {
    lastTimeStamp_ = situation.timeStamp;
    obj_.setNextSpatial(toSpatialVariant({situation.isFrozen,
                                          situation.timeStamp,
                                          ecefPosition,
                                          ecefOrientation,
                                          impl::nedToEcef(situation.velocityVector, situation.worldLocation),
                                          situation.angularVelocity,
                                          impl::nedToEcef(situation.accelerationVector, situation.worldLocation)},
                                         threshold_));
  }
}

template <typename T>
inline void SettableDeadReckoner<T>::setFrozen(sen::TimeStamp timeStamp, bool value)
{
  auto situation = Parent::extrapolate(obj_.getNextSpatial(), timeStamp, lastTimeStamp_);
  situation.isFrozen = value;
  lastTimeStamp_ = timeStamp;

  obj_.setNextSpatial(toSpatialVariant(situation, threshold_));
}

template <typename T>
inline typename SettableDeadReckoner<T>::SpatialVariant SettableDeadReckoner<T>::toSpatialVariant(
  const Situation& situation,
  const DrThreshold& threshold)
{
  const auto isMoving = impl::isMoving(situation.velocityVector);
  const auto isAccelerating = impl::isAccelerating(situation.accelerationVector);

  if (!isMoving && !isAccelerating)
  {
    return SpatialVariant {std::in_place_index<static_cast<size_t>(SpatialAlgorithm::drStatic)>,
                           StaticSpatial {impl::toRpr<RprLocation>(situation.worldLocation),
                                          situation.isFrozen,
                                          impl::toRpr<RprOrientation>(situation.orientation)}};
  }

  const auto isRotating = impl::isRotating(situation.angularVelocity);
  const auto isWorld = threshold.referenceSystem == ReferenceSystem::world;

  // slow moving
  if (isMoving && !isAccelerating)
  {
    // not rotating
    if (!isRotating)
    {
      if (isWorld)
      {
        return SpatialVariant {std::in_place_index<static_cast<size_t>(SpatialAlgorithm::drFPW)>,
                               FpsSpatial {impl::toRpr<RprLocation>(situation.worldLocation),
                                           situation.isFrozen,
                                           impl::toRpr<RprOrientation>(situation.orientation),
                                           impl::toRpr<RprVelocity>(situation.velocityVector)}};
      }
      return SpatialVariant {std::in_place_index<static_cast<size_t>(SpatialAlgorithm::drFPB)>,
                             FpsSpatial {impl::toRpr<RprLocation>(situation.worldLocation),
                                         situation.isFrozen,
                                         impl::toRpr<RprOrientation>(situation.orientation),
                                         impl::toRpr<RprVelocity>(situation.velocityVector)}};
    }

    // rotating
    if (isWorld)
    {
      return SpatialVariant {std::in_place_index<static_cast<size_t>(SpatialAlgorithm::drRPW)>,
                             RpsSpatial {impl::toRpr<RprLocation>(situation.worldLocation),
                                         situation.isFrozen,
                                         impl::toRpr<RprOrientation>(situation.orientation),
                                         impl::toRpr<RprVelocity>(situation.velocityVector),
                                         impl::toRpr<RprAngularVelocity>(situation.angularVelocity)}};
    }
    return SpatialVariant {std::in_place_index<static_cast<size_t>(SpatialAlgorithm::drRPB)>,
                           RpsSpatial {impl::toRpr<RprLocation>(situation.worldLocation),
                                       situation.isFrozen,
                                       impl::toRpr<RprOrientation>(situation.orientation),
                                       impl::toRpr<RprVelocity>(situation.velocityVector),
                                       impl::toRpr<RprAngularVelocity>(situation.angularVelocity)}};
  }
  // fast-moving not rotating
  if (!isRotating)
  {
    if (isWorld)
    {
      return SpatialVariant {std::in_place_index<static_cast<size_t>(SpatialAlgorithm::drFVW)>,
                             FvsSpatial {impl::toRpr<RprLocation>(situation.worldLocation),
                                         situation.isFrozen,
                                         impl::toRpr<RprOrientation>(situation.orientation),
                                         impl::toRpr<RprVelocity>(situation.velocityVector),
                                         impl::toRpr<RprAcceleration>(situation.accelerationVector)}};
    }
    return SpatialVariant {std::in_place_index<static_cast<size_t>(SpatialAlgorithm::drFVB)>,
                           FvsSpatial {impl::toRpr<RprLocation>(situation.worldLocation),
                                       situation.isFrozen,
                                       impl::toRpr<RprOrientation>(situation.orientation),
                                       impl::toRpr<RprVelocity>(situation.velocityVector),
                                       impl::toRpr<RprAcceleration>(situation.accelerationVector)}};
  }

  // fast-moving rotating
  if (isWorld)
  {
    return SpatialVariant {std::in_place_index<static_cast<size_t>(SpatialAlgorithm::drRVW)>,
                           RvsSpatial {impl::toRpr<RprLocation>(situation.worldLocation),
                                       situation.isFrozen,
                                       impl::toRpr<RprOrientation>(situation.orientation),
                                       impl::toRpr<RprVelocity>(situation.velocityVector),
                                       impl::toRpr<RprAcceleration>(situation.accelerationVector),
                                       impl::toRpr<RprAngularVelocity>(situation.angularVelocity)}};
  }
  return SpatialVariant {std::in_place_index<static_cast<size_t>(SpatialAlgorithm::drRVB)>,
                         RvsSpatial {impl::toRpr<RprLocation>(situation.worldLocation),
                                     situation.isFrozen,
                                     impl::toRpr<RprOrientation>(situation.orientation),
                                     impl::toRpr<RprVelocity>(situation.velocityVector),
                                     impl::toRpr<RprAcceleration>(situation.accelerationVector),
                                     impl::toRpr<RprAngularVelocity>(situation.angularVelocity)}};
}

}  // namespace sen::util

#endif  // SEN_UTIL_DR_SETTABLE_DEAD_RECKONER_H
