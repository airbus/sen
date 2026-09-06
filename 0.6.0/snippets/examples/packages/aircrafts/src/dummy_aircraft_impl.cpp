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
#include "sen/util/dr/dead_reckoner.h"
#include "sen/util/dr/settable_dead_reckoner.h"

// std
#include <cmath>
#include <memory>

namespace aircrafts
{

// An aircraft that updates its position.
class DummyAircraftImpl: public DummyAircraftBase<>
{
  SEN_NOCOPY_NOMOVE(DummyAircraftImpl)

  // type alias
  using SettableDr = sen::util::SettableDeadReckoner<rpr::BaseEntityBase<>>;
  using DeadReckoner = sen::util::DeadReckoner<rpr::BaseEntityBase<>>;

public:
  using DummyAircraftBase<>::DummyAircraftBase;
  ~DummyAircraftImpl() override = default;

public:
  void registered(sen::kernel::RegistrationApi& /*api*/) override
  {
    // the DeadReckoner is used to update the world location of the aircraft given the configured speed. This is not
    // the regular use of the DeadReckoner but it comes in handy for the example.
    deadReckoner_ = std::make_unique<DeadReckoner>(*this);

    // the SettablaDeadReckoner is used to update the Spatial field of the aircraft
    settableDeadReckoner_ = std::make_unique<SettableDr>(*this);
  }

  void update(sen::kernel::RunApi& api) override
  {
    // move the entity using the dead reckoner with the specified speed
    auto situation = deadReckoner_->geodeticSituation(api.getTime());

    // initialize the situation of the entity in the first iteration
    if (!init_)
    {
      situation.worldLocation = {40.0, 0.0, 10000.0};
      init_ = true;
    }

    // update the speed (can be changed while the model is running)
    situation.velocityVector = {getSpeed(), 0, 0};

    // update the spatial using the settable dead reckoner
    settableDeadReckoner_->setSpatial(situation);
  }

private:
  bool init_ = false;
  std::unique_ptr<DeadReckoner> deadReckoner_;
  std::unique_ptr<SettableDr> settableDeadReckoner_;
};

SEN_EXPORT_CLASS(DummyAircraftImpl)

}  // namespace aircrafts
