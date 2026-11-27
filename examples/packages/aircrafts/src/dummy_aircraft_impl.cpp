// === dummy_aircraft_impl.cpp =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated code
#include "rpr/rpr-base_v2.0.xml.h"
#include "stl/dummy_aircraft.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/class_type.h"
#include "sen/kernel/component_api.h"
#include "sen/util/dr/detail/dead_reckoner_base.h"
#include "sen/util/dr/settable_dead_reckoner.h"

// std
#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <utility>

namespace aircrafts
{

namespace
{

// --8<-- [start:dead_reckoning]
// Advance the commanded local velocity on the WGS84 ellipsoid. The midpoint
// approximation is accurate over the short intervals between corrections.
void advance(sen::util::GeodeticSituation& situation, sen::TimeStamp time)
{
  constexpr float64_t pi = 3.14159265358979323846;
  constexpr float64_t semiMajorAxis = 6378137.0;
  constexpr float64_t eccentricitySquared = 6.69437999014e-3;
  constexpr float64_t poleLimit = pi / 2.0 - 1e-8;
  auto latitude = static_cast<float64_t>(situation.worldLocation.latitude) * pi / 180.0;
  auto longitude = static_cast<float64_t>(situation.worldLocation.longitude) * pi / 180.0;
  auto altitude = static_cast<float64_t>(situation.worldLocation.altitude);
  const auto north = static_cast<float64_t>(situation.velocityVector.x);
  const auto east = static_cast<float64_t>(situation.velocityVector.y);
  const auto down = static_cast<float64_t>(situation.velocityVector.z);
  auto remaining = (time - situation.timeStamp).toSeconds();

  while (remaining > 0.0)
  {
    const auto step = std::min(remaining, 10.0);
    const auto sine = std::sin(latitude);
    const auto factor = 1.0 - eccentricitySquared * sine * sine;
    const auto meridianRadius = semiMajorAxis * (1.0 - eccentricitySquared) / std::pow(factor, 1.5);
    const auto midpoint =
      std::clamp(latitude + north * step / (2.0 * (meridianRadius + altitude)), -poleLimit, poleLimit);
    const auto middleSine = std::sin(midpoint);
    const auto middleFactor = 1.0 - eccentricitySquared * middleSine * middleSine;
    const auto primeVertical = semiMajorAxis / std::sqrt(middleFactor);
    const auto middleMeridian = primeVertical * (1.0 - eccentricitySquared) / middleFactor;
    const auto middleAltitude = altitude - down * step / 2.0;
    const auto nextLatitude = latitude + north * step / (middleMeridian + middleAltitude);
    const auto horizontalStep =
      std::abs(nextLatitude) > poleLimit
        ? step * (std::copysign(poleLimit, nextLatitude) - latitude) / (nextLatitude - latitude)
        : step;
    latitude = std::clamp(nextLatitude, -poleLimit, poleLimit);
    longitude = std::remainder(
      longitude + east * horizontalStep / ((primeVertical + middleAltitude) * std::cos(midpoint)), 2.0 * pi);
    altitude -= down * step;
    remaining -= step;
  }
  situation.worldLocation = {latitude * 180.0 / pi, longitude * 180.0 / pi, altitude};
  situation.timeStamp = time;
}

}  // namespace

// An aircraft that updates its position.
class DummyAircraftImpl: public DummyAircraftBase<>
{
  SEN_NOCOPY_NOMOVE(DummyAircraftImpl)

  // type alias
  using SettableDr = sen::util::SettableDeadReckoner<rpr::BaseEntityBase<>>;
  using DeadReckonerBase = sen::util::DeadReckonerBase;

public:
  using DummyAircraftBase<>::DummyAircraftBase;
  ~DummyAircraftImpl() override = default;

public:
  void registered(sen::kernel::RegistrationApi& api) override
  {
    std::ignore = api;

    // used to update the spatial field of the aircraft
    settableDeadReckoner_ =
      std::make_unique<SettableDr>(*this, sen::util::DrThreshold {0.0, 0.0f, sen::util::ReferenceSystem::world});

    // commanded speed can be modified by the user; the next update applies it at its own time
    speedGuard_ = onSpeedChanged({this, [this]() { speedChanged_ = true; }});
  }

  void update(sen::kernel::RunApi& runApi) override
  {
    const auto currentTime = runApi.getTime();

    // seed the dead reckoner on the first update
    if (!started_)
    {
      started_ = true;
      speedChanged_ = false;

      correctedSituation_.timeStamp = currentTime;
      correctedSituation_.worldLocation = {40.0, 0.0, 10000.0};
      const auto& [north, east, down] = getSpeed();
      correctedSituation_.velocityVector = {north, east, down};
      correctedSituation_.orientation.psi = std::atan2(east, north);
      deadReckoner_.updateGeodeticSituation(correctedSituation_);
    }
    else if (speedChanged_ || currentTime - correctedSituation_.timeStamp >= sen::Duration {std::chrono::seconds(10)})
    {
      // Correct the ECEF extrapolation against the aircraft's local NED motion.
      // Advance using the previous command before applying a newly received one.
      advance(correctedSituation_, currentTime);
      speedChanged_ = false;
      const auto& [north, east, down] = getSpeed();
      correctedSituation_.velocityVector = {north, east, down};
      // North and east are undefined at a pole. Stop horizontal motion until
      // a new command takes the aircraft back towards the equator.
      const auto latitude = static_cast<float64_t>(correctedSituation_.worldLocation.latitude);
      if ((latitude >= 89.999999 && north >= 0.0) || (latitude <= -89.999999 && north <= 0.0))
      {
        correctedSituation_.velocityVector.x = 0.0;
        correctedSituation_.velocityVector.y = 0.0;
      }
      if (north != 0.0 || east != 0.0)
      {
        correctedSituation_.orientation.psi = std::atan2(east, north);
      }
      deadReckoner_.updateGeodeticSituation(correctedSituation_);
    }

    // move the entity using the dead reckoner with the specified speed
    auto situation = deadReckoner_.geodeticSituation(currentTime);

    // update the spatial using the settable dead reckoner
    settableDeadReckoner_->setSpatial(situation);
  }

private:
  DeadReckonerBase deadReckoner_;
  sen::util::GeodeticSituation correctedSituation_;
  std::unique_ptr<SettableDr> settableDeadReckoner_;
  sen::ConnectionGuard speedGuard_;
  bool started_ = false;
  bool speedChanged_ = false;
};
// --8<-- [end:dead_reckoning]

SEN_EXPORT_CLASS(DummyAircraftImpl)

}  // namespace aircrafts
