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

// sen
#include "sen/util/dr/algorithms.h"

// std
#include <algorithm>

namespace sen::util
{

/// \addtogroup dr
/// @{

/// Enables the user to predict an object’s position and movement at any future time applying dead
/// reckoning. It adheres to the algorithms specified in IEEE 1278_1:2012, Annex E.
/// @tparam T rpr::BaseEntityInterface or a subclass of it
// --8<-- [start:dead_reckoner]
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
  /// Extrapolates the held spatial to the given instant. Smoothing is only available for world
  /// centered algorithms, so a body referenced spatial is returned unsmoothed.
  [[nodiscard]] Situation extrapolateAt(sen::TimeStamp time);

  /// True when the spatial's algorithm is body referenced, which decides how velocities are
  /// converted and whether smoothing applies.
  [[nodiscard]] static bool isBodyReferenced(const SpatialVariant& spatial) noexcept;

  /// Updates the lastSpatial and lastTimeStamp members when a new Spatial is received
  void updateSpatial(sen::TimeStamp time);

  /// The instant the held spatial was produced, which is what the extrapolation measures from.
  /// Falls back to the query instant when the object cannot report a usable one.
  [[nodiscard]] sen::TimeStamp spatialOrigin(sen::TimeStamp time) const noexcept;

private:
  const T& object_;
  sen::TimeStamp lastTimeStamp_;
  SpatialVariant lastSpatial_;
};
// --8<-- [end:dead_reckoner]

/// @}

//-------------------------------------------------------------------------------------------------------------------
// Utils
//-------------------------------------------------------------------------------------------------------------------

/// Transform a Situation to a GeodeticSituation
[[nodiscard]] inline GeodeticSituation toGeodeticSituation(const Situation& value)
{
  return impl::toGeodeticSituation(value);
}

/// Transforms a GeodeticSituation to a Situation
[[nodiscard]] inline Situation toSituation(const GeodeticSituation& value) { return impl::toSituation(value); }

//-------------------------------------------------------------------------------------------------------------------
// Inline implementation
//-------------------------------------------------------------------------------------------------------------------

template <typename T>
inline DeadReckoner<T>::DeadReckoner(const T& object, DrConfig config)
  : DeadReckonerTemplateBase<T>(config), object_ {object}
{
  DeadReckonerBase::setCachedSituation(toSituation(object_.getSpatial()));
}

template <typename T>
inline Situation DeadReckoner<T>::situation(sen::TimeStamp timeStamp)
{
  if (!this->isSituationCached(timeStamp))
  {
    this->setCachedSituation(extrapolateAt(timeStamp));
  }
  return this->getCachedSituation();
}

template <typename T>
inline GeodeticSituation DeadReckoner<T>::geodeticSituation(sen::TimeStamp timeStamp)
{
  if (!this->isGeodeticSituationCached(timeStamp))
  {
    const auto situation = extrapolateAt(timeStamp);

    if (!isBodyReferenced(lastSpatial_))
    {
      this->setCachedGeodeticSituation(impl::toGeodeticSituation(situation));
    }
    else
    {
      const auto geoLocation = impl::toLla(situation.worldLocation);
      const auto nedOrientation = impl::ecefToNed(situation.orientation, geoLocation);
      this->setCachedGeodeticSituation(GeodeticSituation {situation.isFrozen,
                                                          situation.timeStamp,
                                                          geoLocation,
                                                          nedOrientation,
                                                          impl::bodyToNed(situation.velocityVector, nedOrientation),
                                                          situation.angularVelocity,
                                                          impl::bodyToNed(situation.accelerationVector, nedOrientation),
                                                          situation.angularAcceleration});
    }
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
inline Situation DeadReckoner<T>::extrapolateAt(sen::TimeStamp time)
{
  updateSpatial(time);
  const auto update = Parent::extrapolate(lastSpatial_, time, lastTimeStamp_);

  // A producer may publish a different algorithm at any time, so the frame is read each query.
  return isBodyReferenced(lastSpatial_) ? update : DeadReckonerBase::smoothIfEnabled(update);
}

template <typename T>
inline bool DeadReckoner<T>::isBodyReferenced(const SpatialVariant& spatial) noexcept
{
  const auto algorithm = static_cast<SpatialAlgorithm>(spatial.index());
  return std::find(bodyAlgorithms.begin(), bodyAlgorithms.end(), algorithm) != bodyAlgorithms.end();
}

template <typename T>
void DeadReckoner<T>::updateSpatial(sen::TimeStamp time)
{
  if (const auto& newSpatial = object_.getSpatial(); newSpatial != lastSpatial_)
  {
    lastSpatial_ = newSpatial;
    lastTimeStamp_ = spatialOrigin(time);
    this->invalidateCache();
  }
}

template <typename T>
inline sen::TimeStamp DeadReckoner<T>::spatialOrigin(sen::TimeStamp time) const noexcept
{
  if constexpr (impl::HasCommitTime<T>::value)
  {
    if (DeadReckonerBase::getConfig().useCommitTimeAsOrigin)
    {
      // A producer that never advances its commit time would freeze the origin and grow the delta
      // without bound, and one ahead of the query would extrapolate backwards.
      if (const auto committed = object_.asObject().getLastCommitTime();
          committed > lastTimeStamp_ && committed <= time)
      {
        return committed;
      }
    }
  }

  return time;
}

}  // namespace sen::util

#endif  // SEN_UTIL_DR_DEAD_RECKONER_H
