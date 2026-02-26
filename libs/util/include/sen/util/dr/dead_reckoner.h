// === dead_reckoner.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_UTIL_DR_DEAD_RECKONER_H
#define SEN_UTIL_DR_DEAD_RECKONER_H

#include "detail/dead_reckoner_base.h"
#include "detail/dead_reckoner_impl.h"

// std
#include <algorithm>

namespace sen::util
{

/// \addtogroup dr
/// @{

/// Enables the user to predict an object’s position and movement at any future time applying dead
/// reckoning. It adheres to the algorithms specified in IEEE 1278_1:2012, Annex E.
/// @tparam T rpr::BaseEntityInterface or a subclass of it
template <typename T>
class DeadReckoner: public DeadReckonerTemplateBase<T>
{
public:
  SEN_NOCOPY_NOMOVE(DeadReckoner)

public:  // RPR types from DeadReckonerTemplateBase
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

public:  // type aliases
  using SituationProcessor = std::function<Situation(sen::TimeStamp)>;
  using GeodeticSituationProcessor = std::function<GeodeticSituation(sen::TimeStamp)>;

public:
  /// Constructor for the DeadReckoner where an object inheriting from rpr::BaseEntity is inputted as a reference.
  /// This is the easiest version of the API to instantiate a DeadReckoner.
  explicit DeadReckoner(const T& object, DrConfig config = {});
  ~DeadReckoner() override = default;

public:  // overrides DeadReckonerBase
  [[nodiscard]] Situation situation(sen::TimeStamp timeStamp) override;
  [[nodiscard]] GeodeticSituation geodeticSituation(sen::TimeStamp timeStamp) override;

public:
  /// Provides direct mutable access to the internal object managed by this instance of the DeadReckoner
  [[nodiscard]] T& getObject() noexcept;

public:  // situation translation helpers
  /// Translates a SpatialVariant to a Situation struct
  [[nodiscard]] static Situation toSituation(const SpatialVariant& spatial, sen::TimeStamp timeStamp = {});

  /// Translates a SpatialVariant to a GeodeticSituation struct
  [[nodiscard]] static GeodeticSituation toGeodeticSituation(const SpatialVariant& spatial,
                                                             sen::TimeStamp timeStamp = {});

private:
  /// Provides the situation processor that will be executed depending on the type of DR algorithm used
  /// (body-centered or world-centered). NOTE: Smoothing is only available for world centered algorithms.
  [[nodiscard]] SituationProcessor getSituationProcessor(bool bodyReferenced);

  /// Provides the geodetic situation processor that will be executed depending on the type of DR algorithm used
  /// (body-centered or world-centered). NOTE: Smoothing is only available for world centered algorithms.
  [[nodiscard]] GeodeticSituationProcessor getGeodeticSituationProcessor(bool bodyReferenced);

  /// Updates the lastSpatial and lastTimeStamp members when a new Spatial is received
  void updateSpatial(sen::TimeStamp time);

private:
  const T& object_;
  SituationProcessor processSituation_;
  GeodeticSituationProcessor processGeodeticSituation_;
  sen::TimeStamp lastTimeStamp_;
  SpatialVariant lastSpatial_;
};

/// @}

//-------------------------------------------------------------------------------------------------------------------
// Utils
//-------------------------------------------------------------------------------------------------------------------

/// Transform a Situation to a GeodeticSituation
[[nodiscard]] inline GeodeticSituation toGeodeticSituation(const Situation& value)
{
  const auto geoLocation = impl::toLla(value.worldLocation);
  return GeodeticSituation {value.isFrozen,
                            value.timeStamp,
                            geoLocation,
                            impl::ecefToNed(value.orientation, geoLocation),
                            impl::ecefToNed(value.velocityVector, geoLocation),
                            value.angularVelocity,
                            impl::ecefToNed(value.accelerationVector, geoLocation),
                            value.angularAcceleration};
}

/// Transforms a GeodeticSituation to a Situation
[[nodiscard]] inline Situation toSituation(const GeodeticSituation& value)
{
  // translate input geodetic situation to standard reference system
  const auto ecefPosition = impl::toEcef(value.worldLocation);
  const auto ecefOrientation = impl::nedToEcef(value.orientation, value.worldLocation);

  return Situation {value.isFrozen,
                    value.timeStamp,
                    ecefPosition,
                    ecefOrientation,
                    impl::nedToEcef(value.velocityVector, value.worldLocation),
                    value.angularVelocity,
                    impl::nedToEcef(value.accelerationVector, value.worldLocation)};
}

//-------------------------------------------------------------------------------------------------------------------
// Inline implementation
//-------------------------------------------------------------------------------------------------------------------

template <typename T>
inline DeadReckoner<T>::DeadReckoner(const T& object, DrConfig config)
  : DeadReckonerTemplateBase<T>(config), object_ {object}
{
  // true if the DR algorithm used is body-centered
  auto isBody = std::find(bodyAlgorithms.begin(),
                          bodyAlgorithms.end(),
                          static_cast<SpatialAlgorithm>(object_.getSpatial().index())) != bodyAlgorithms.end();

  // initialize situation and geodetic situation processors
  processSituation_ = getSituationProcessor(isBody);
  processGeodeticSituation_ = getGeodeticSituationProcessor(isBody);
}

template <typename T>
inline Situation DeadReckoner<T>::situation(sen::TimeStamp timeStamp)
{
  if (!this->isSituationCached(timeStamp))
  {
    this->setCachedSituation(processSituation_(timeStamp));
  }
  return this->getCachedSituation();
}

template <typename T>
inline GeodeticSituation DeadReckoner<T>::geodeticSituation(sen::TimeStamp timeStamp)
{
  if (!this->isGeodeticSituationCached(timeStamp))
  {
    this->setCachedGeodeticSituation(processGeodeticSituation_(timeStamp));
  }
  return this->getCachedGeodeticSituation();
}

template <typename T>
inline T& DeadReckoner<T>::getObject() noexcept
{
  return const_cast<T&>(object_);  // NOLINT
}

template <typename T>
inline Situation DeadReckoner<T>::toSituation(const SpatialVariant& spatial, sen::TimeStamp timeStamp)
{
  return std::visit(sen::Overloaded {[&timeStamp](const StaticSpatial& value)
                                     {
                                       return Situation {value.isFrozen,
                                                         timeStamp,
                                                         impl::fromRprLocation(value.worldLocation),
                                                         impl::fromRprOrientation(value.orientation)};
                                     },
                                     [&timeStamp](const FpsSpatial& value)
                                     {
                                       return Situation {value.isFrozen,
                                                         timeStamp,
                                                         impl::fromRprLocation(value.worldLocation),
                                                         impl::fromRprOrientation(value.orientation),
                                                         impl::fromRprVelocity(value.velocityVector)};
                                     },
                                     [&timeStamp](const RpsSpatial& value)
                                     {
                                       return Situation {value.isFrozen,
                                                         timeStamp,
                                                         impl::fromRprLocation(value.worldLocation),
                                                         impl::fromRprOrientation(value.orientation),
                                                         impl::fromRprVelocity(value.velocityVector),
                                                         impl::fromRprAngularVelocity(value.angularVelocity)};
                                     },
                                     [&timeStamp](const RvsSpatial& value)
                                     {
                                       return Situation {value.isFrozen,
                                                         timeStamp,
                                                         impl::fromRprLocation(value.worldLocation),
                                                         impl::fromRprOrientation(value.orientation),
                                                         impl::fromRprVelocity(value.velocityVector),
                                                         impl::fromRprAngularVelocity(value.angularVelocity),
                                                         impl::fromRprAcceleration(value.accelerationVector)};
                                     },
                                     [&timeStamp](const FvsSpatial& value)
                                     {
                                       return Situation {value.isFrozen,
                                                         timeStamp,
                                                         impl::fromRprLocation(value.worldLocation),
                                                         impl::fromRprOrientation(value.orientation),
                                                         impl::fromRprVelocity(value.velocityVector),
                                                         {},
                                                         impl::fromRprAcceleration(value.accelerationVector)};
                                     }},
                    spatial);
}

template <typename T>
inline GeodeticSituation DeadReckoner<T>::toGeodeticSituation(const SpatialVariant& spatial, sen::TimeStamp timeStamp)
{
  const auto situation = toSituation(spatial, timeStamp);
  const auto geoLocation = impl::toLla(situation.worldLocation);
  const auto nedOrientation = impl::ecefToNed(situation.orientation, geoLocation);

  // body centered algorithms
  if (std::find(bodyAlgorithms.begin(), bodyAlgorithms.end(), static_cast<SpatialAlgorithm>(spatial.index())) !=
      bodyAlgorithms.end())
  {
    return {situation.isFrozen,
            situation.timeStamp,
            geoLocation,
            nedOrientation,
            impl::bodyToNed(situation.velocityVector, nedOrientation),
            situation.angularVelocity,
            impl::bodyToNed(situation.accelerationVector, nedOrientation),
            situation.angularAcceleration};
  }

  // world-centered algorithms
  return {situation.isFrozen,
          situation.timeStamp,
          geoLocation,
          nedOrientation,
          impl::ecefToNed(situation.velocityVector, geoLocation),
          situation.angularVelocity,
          impl::ecefToNed(situation.accelerationVector, geoLocation),
          situation.angularAcceleration};
}

template <typename T>
typename DeadReckoner<T>::SituationProcessor DeadReckoner<T>::getSituationProcessor(bool bodyReferenced)
{
  if (bodyReferenced)
  {
    return [this](sen::TimeStamp time)
    {
      updateSpatial(time);
      return Parent::extrapolate(lastSpatial_, time, lastTimeStamp_);
    };
  }

  return [this](sen::TimeStamp time)
  {
    updateSpatial(time);
    const auto update = Parent::extrapolate(lastSpatial_, time, lastTimeStamp_);
    DeadReckonerBase::smooth(update);
    return DeadReckonerBase::getConfig().smoothing ? DeadReckonerBase::getSmoothSituation() : update;
  };
}

template <typename T>
typename DeadReckoner<T>::GeodeticSituationProcessor DeadReckoner<T>::getGeodeticSituationProcessor(bool bodyReferenced)
{
  if (bodyReferenced)
  {
    return [this](sen::TimeStamp time)
    {
      const auto situation = processSituation_(time);
      const auto geoLocation = impl::toLla(situation.worldLocation);
      const auto nedOrientation = impl::ecefToNed(situation.orientation, geoLocation);
      return GeodeticSituation {situation.isFrozen,
                                situation.timeStamp,
                                geoLocation,
                                nedOrientation,
                                impl::bodyToNed(situation.velocityVector, nedOrientation),
                                situation.angularVelocity,
                                impl::bodyToNed(situation.accelerationVector, nedOrientation),
                                situation.angularAcceleration};
    };
  }

  return [this](sen::TimeStamp time)
  {
    const auto situation = processSituation_(time);
    const auto geoLocation = impl::toLla(situation.worldLocation);
    return GeodeticSituation {situation.isFrozen,
                              situation.timeStamp,
                              geoLocation,
                              impl::ecefToNed(situation.orientation, geoLocation),
                              impl::ecefToNed(situation.velocityVector, geoLocation),
                              situation.angularVelocity,
                              impl::ecefToNed(situation.accelerationVector, geoLocation),
                              situation.angularAcceleration};
  };
}

template <typename T>
void DeadReckoner<T>::updateSpatial(sen::TimeStamp time)
{
  if (const auto& newSpatial = object_.getSpatial(); newSpatial != lastSpatial_)
  {
    lastSpatial_ = newSpatial;
    lastTimeStamp_ = time;
    this->invalidateCache();
  }
}

}  // namespace sen::util

#endif  // SEN_UTIL_DR_DEAD_RECKONER_H
