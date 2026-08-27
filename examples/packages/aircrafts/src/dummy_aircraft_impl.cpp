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
#include "sen/core/meta/class_type.h"
#include "sen/kernel/component_api.h"
#include "sen/util/dr/detail/dead_reckoner_base.h"
#include "sen/util/dr/settable_dead_reckoner.h"

// std
#include <cmath>
#include <memory>
#include <utility>

namespace aircrafts
{

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

    // commanded speed can be modified by the user
    speedGuard_ = onSpeedChanged({this,
                                  [this]()
                                  {
                                    auto situation = deadReckoner_.geodeticSituation(api_->getTime());
                                    const auto& [north, east, down] = getSpeed();
                                    situation.velocityVector = {north, east, down};
                                    deadReckoner_.updateGeodeticSituation(std::move(situation));
                                  }});
  }

  void update(sen::kernel::RunApi& runApi) override
  {
    api_ = &runApi;

    if (const auto currentTime = runApi.getTime(); currentTime == runApi.getStartTime())
    {
      sen::util::GeodeticSituation initialSituation;
      initialSituation.timeStamp = currentTime;
      initialSituation.worldLocation = {40.0, 0.0, 10000.0};
      const auto& [north, east, down] = getSpeed();
      initialSituation.velocityVector = {north, east, down};
      deadReckoner_.updateGeodeticSituation(std::move(initialSituation));
    }
    // move the entity using the dead reckoner with the specified speed
    auto situation = deadReckoner_.geodeticSituation(runApi.getTime());

    // update the spatial using the settable dead reckoner
    settableDeadReckoner_->setSpatial(situation);
  }

private:
  DeadReckonerBase deadReckoner_;
  std::unique_ptr<SettableDr> settableDeadReckoner_;
  sen::ConnectionGuard speedGuard_;
  sen::kernel::RunApi* api_ = nullptr;
};

SEN_EXPORT_CLASS(DummyAircraftImpl)

}  // namespace aircrafts
