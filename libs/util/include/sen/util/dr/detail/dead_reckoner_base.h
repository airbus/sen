// === dead_reckoner_base.h ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_UTIL_DR_DETAIL_DEAD_RECKONER_BASE_H
#define SEN_UTIL_DR_DETAIL_DEAD_RECKONER_BASE_H

#include "dead_reckoner_impl.h"

// sen
#include "sen/util/dr/algorithms.h"

namespace sen::util
{

/// \addtogroup dr
/// @{

/// Extrapolates the Situation of an entity at a certain time. The extrapolation is smoothed by
/// default unless the user specifies otherwise.
class DeadReckonerBase
{

public:
  SEN_NOCOPY_NOMOVE(DeadReckonerBase)

public:
  explicit DeadReckonerBase(DrConfig config = {});
  virtual ~DeadReckonerBase() = default;

public:
  /// Returns the extrapolated/smoothed situation of the object at the timestamp introduced as argument, expressed in
  /// the following coordinates:
  /// - World position: ECEF coordinates
  /// - Orientation: Euler angles of the body reference system (x forward, y right, z down) with respect to ECEF
  /// - Velocity: In world coordinates for world-centered DR algorithms and in body coordinates for body-centered
  /// algorithms
  /// - Acceleration: In world coordinates for world-centered DR algorithms and in body coordinates for body-centered
  /// algorithms
  /// - AngularVelocity: In body coordinates.
  /// - AngularAcceleration: In body coordinates.
  [[nodiscard]] virtual Situation situation(sen::TimeStamp timeStamp);

  /// Returns the extrapolated/smoothed situation of the object at the timestamp introduced as argument, expressed in
  /// the following coordinates:
  /// - World position: Geodetic Latitude, Longitude and Altitude
  /// - Orientation: Euler angles of the body reference system (x forward, y right, z down) with respect to NED
  /// - Velocity: In NED coordinates for world-referenced algorithms, in body coordinates for body-referenced
  /// algorithms.
  /// - Acceleration: In NED coordinates for world-referenced algorithms, in body coordinates for body-referenced
  /// algorithms.
  /// - AngularVelocity: In body coordinates.
  /// - AngularAcceleration: In body coordinates.
  [[nodiscard]] virtual GeodeticSituation geodeticSituation(sen::TimeStamp timeStamp);

  /// Updates the last known real time Situation. A valid timestamp is needed inside the Situation provided as
  /// argument.
  void updateSituation(const Situation& value);

  /// Updates the last known real time GeodeticSituation. A valid timestamp is needed inside the GeodeticSituation
  /// provided as argument.
  void updateGeodeticSituation(const GeodeticSituation& value);

public:  // config
  [[nodiscard]] const DrConfig& getConfig() const noexcept;
  void setConfig(const DrConfig& config);

protected:
  [[nodiscard]] const Situation& getSmoothSituation() const noexcept;
  void smooth(const Situation& update);

  void invalidateCache();

  [[nodiscard]] bool isSituationCached(sen::TimeStamp timeStamp) const noexcept;
  void setCachedSituation(const Situation& situation);
  [[nodiscard]] const Situation& getCachedSituation() const noexcept;

  [[nodiscard]] bool isGeodeticSituationCached(sen::TimeStamp timeStamp) const noexcept;
  void setCachedGeodeticSituation(const GeodeticSituation& situation);
  [[nodiscard]] const GeodeticSituation& getCachedGeodeticSituation() const noexcept;

private:
  DrConfig config_;
  Situation lastSituation_;
  Situation smoothSituation_;

  // cache
  Situation cachedSituation_;
  GeodeticSituation cachedGeodeticSituation_;
};

/// Base class for the DeadReckoner and SettableDeadReckoner classes.
template <typename T>
class DeadReckonerTemplateBase: public DeadReckonerBase
{
public:
  SEN_NOCOPY_NOMOVE(DeadReckonerTemplateBase)

public:
  explicit DeadReckonerTemplateBase(DrConfig config = {});
  ~DeadReckonerTemplateBase() override = default;

public:  // relevant spatial types inferred from RPR
  using SpatialVariant = std::remove_const_t<std::remove_reference_t<decltype(std::declval<T>().getSpatial())>>;
  using StaticSpatial = std::variant_alternative_t<0U, SpatialVariant>;
  using FpsSpatial = std::variant_alternative_t<1U, SpatialVariant>;
  using RpsSpatial = std::variant_alternative_t<2U, SpatialVariant>;
  using RvsSpatial = std::variant_alternative_t<3U, SpatialVariant>;
  using FvsSpatial = std::variant_alternative_t<4U, SpatialVariant>;
  using RprLocation = decltype(std::declval<RvsSpatial>().worldLocation);
  using RprOrientation = decltype(std::declval<RvsSpatial>().orientation);
  using RprVelocity = decltype(std::declval<RvsSpatial>().velocityVector);
  using RprAcceleration = decltype(std::declval<RvsSpatial>().accelerationVector);
  using RprAngularVelocity = decltype(std::declval<RvsSpatial>().angularVelocity);

protected:
  /// Returns the extrapolated situation of the RPR Entity given its Spatial field and two timestamps that define a
  /// time delta for the extrapolation
  [[nodiscard]] static Situation extrapolate(const SpatialVariant& spatial,
                                             sen::TimeStamp time,
                                             sen::TimeStamp lastTimeStamp);
};

/// @}

//-------------------------------------------------------------------------------------------------------------------
// Inline implementation
//-------------------------------------------------------------------------------------------------------------------

template <typename T>
DeadReckonerTemplateBase<T>::DeadReckonerTemplateBase(DrConfig config): DeadReckonerBase(std::move(config))
{
}

template <typename T>
inline Situation DeadReckonerTemplateBase<T>::extrapolate(const SpatialVariant& spatial,
                                                          sen::TimeStamp time,
                                                          sen::TimeStamp lastTimeStamp)
{
  // NOTE: For efficiency, we handle more common cases prior in the switch.
  switch (static_cast<SpatialAlgorithm>(spatial.index()))
  {
    case SpatialAlgorithm::drFVW:
    {
      const auto& value = std::get<4>(spatial);
      return drFvw({value.isFrozen,
                    lastTimeStamp,
                    impl::fromRprLocation(value.worldLocation),
                    impl::fromRprOrientation(value.orientation),
                    impl::fromRprVelocity(value.velocityVector),
                    {},
                    impl::fromRprAcceleration(value.accelerationVector)},
                   time);
    }
    case SpatialAlgorithm::drRVW:
    {
      const auto& value = std::get<3>(spatial);
      return drRvw({value.isFrozen,
                    lastTimeStamp,
                    impl::fromRprLocation(value.worldLocation),
                    impl::fromRprOrientation(value.orientation),
                    impl::fromRprVelocity(value.velocityVector),
                    impl::fromRprAngularVelocity(value.angularVelocity),
                    impl::fromRprAcceleration(value.accelerationVector)},
                   time);
    }
    case SpatialAlgorithm::drFPW:
    {
      const auto& value = std::get<1>(spatial);
      return drFpw({value.isFrozen,
                    lastTimeStamp,
                    impl::fromRprLocation(value.worldLocation),
                    impl::fromRprOrientation(value.orientation),
                    impl::fromRprVelocity(value.velocityVector)},
                   time);
    }
    case SpatialAlgorithm::drRPW:
    {
      const auto& value = std::get<2>(spatial);
      return drRpw({value.isFrozen,
                    lastTimeStamp,
                    impl::fromRprLocation(value.worldLocation),
                    impl::fromRprOrientation(value.orientation),
                    impl::fromRprVelocity(value.velocityVector),
                    impl::fromRprAngularVelocity(value.angularVelocity)},
                   time);
    }
    case SpatialAlgorithm::drFVB:
    {
      const auto& value = std::get<8>(spatial);
      return drFvb({value.isFrozen,
                    lastTimeStamp,
                    impl::fromRprLocation(value.worldLocation),
                    impl::fromRprOrientation(value.orientation),
                    impl::fromRprVelocity(value.velocityVector),
                    {},
                    impl::fromRprAcceleration(value.accelerationVector)},
                   time);
    }
    case SpatialAlgorithm::drRVB:
    {
      const auto& value = std::get<7>(spatial);
      return drRvb({value.isFrozen,
                    lastTimeStamp,
                    impl::fromRprLocation(value.worldLocation),
                    impl::fromRprOrientation(value.orientation),
                    impl::fromRprVelocity(value.velocityVector),
                    impl::fromRprAngularVelocity(value.angularVelocity),
                    impl::fromRprAcceleration(value.accelerationVector)},
                   time);
    }
    case SpatialAlgorithm::drFPB:
    {
      const auto& value = std::get<5>(spatial);
      return drFpb({value.isFrozen,
                    lastTimeStamp,
                    impl::fromRprLocation(value.worldLocation),
                    impl::fromRprOrientation(value.orientation),
                    impl::fromRprVelocity(value.velocityVector)},
                   time);
    }
    case SpatialAlgorithm::drRPB:
    {
      const auto& value = std::get<6>(spatial);
      return drRpb({value.isFrozen,
                    lastTimeStamp,
                    impl::fromRprLocation(value.worldLocation),
                    impl::fromRprOrientation(value.orientation),
                    impl::fromRprVelocity(value.velocityVector),
                    impl::fromRprAngularVelocity(value.angularVelocity)},
                   time);
    }
    case SpatialAlgorithm::drStatic:
    {
      const auto& value = std::get<0>(spatial);
      return {
        value.isFrozen, time, impl::fromRprLocation(value.worldLocation), impl::fromRprOrientation(value.orientation)};
    }
    default:
      SEN_UNREACHABLE();
  }
}

inline void DeadReckonerBase::invalidateCache()
{
  cachedSituation_ = {};
  cachedGeodeticSituation_ = {};
}

inline bool DeadReckonerBase::isSituationCached(sen::TimeStamp timeStamp) const noexcept
{
  return timeStamp == cachedSituation_.timeStamp;
}

inline void DeadReckonerBase::setCachedSituation(const Situation& situation) { cachedSituation_ = situation; }

inline bool DeadReckonerBase::isGeodeticSituationCached(sen::TimeStamp timeStamp) const noexcept
{
  return timeStamp == cachedGeodeticSituation_.timeStamp;
}

inline void DeadReckonerBase::setCachedGeodeticSituation(const GeodeticSituation& situation)
{
  cachedGeodeticSituation_ = situation;
}

inline const Situation& DeadReckonerBase::getCachedSituation() const noexcept { return cachedSituation_; }

inline const GeodeticSituation& DeadReckonerBase::getCachedGeodeticSituation() const noexcept
{
  return cachedGeodeticSituation_;
}

}  // namespace sen::util

#endif  // SEN_UTIL_DR_DETAIL_DEAD_RECKONER_BASE_H
