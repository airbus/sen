// === dummy_aircraft.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// implementation
#include "util.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/component_api.h"

// generated code
#include "rpr/rpr-base_v2.0.xml.h"
#include "rpr/rpr-enumerations_v2.0.xml.h"
#include "rpr/rpr-physical_v2.0.xml.h"

// std
#include <random>
#include <string>

namespace dummy_entities
{

using Aircraft = rpr::AircraftBase<>;

class DummyAircraft: public Aircraft
{
  SEN_NOCOPY_NOMOVE(DummyAircraft)

public:
  DummyAircraft(const std::string& name, const sen::VarMap& args)
    : Aircraft(name, makePlatformArgs(args)), gen_(rd_()), distrib_(1, 10)
  {
    seed(noise_, {"x", "y", "x", "vx", "vy", "vz", "phi", "psi", "theta"});

    rpr::SpatialStaticStruct v;
    v.isFrozen = false;
    v.orientation.phi = noise_[0]({0.44});
    v.orientation.psi = noise_[0]({0.25});
    v.orientation.theta = noise_[0]({0.53});
    v.worldLocation.x = noise_[0]({0.11});
    v.worldLocation.y = noise_[0]({23.44});
    v.worldLocation.z = noise_[0]({99.33});
    setNextRelativeSpatial(v);
  }

  ~DummyAircraft() override = default;

public:
  void update(sen::kernel::RunApi& api) override
  {
    setNextSpatial(toFpwSpatial(updateSpatial(spatial_, noise_, api)));

    if (distrib_(gen_) % 4 == 0)
    {
      switch (distrib_(gen_))
      {
        case 1:
          setNextBrakeLightsOn(!getBrakeLightsOn());
          setNextBlackOutLightsOn(!getBlackOutLightsOn());
          setNextDamageState(rpr::DamageStatusEnum32::moderateDamage);
          break;
        case 2:
          setNextTentDeployed(!getTentDeployed());
          setNextDamageState(rpr::DamageStatusEnum32::destroyed);
          setNextFirePowerDisabled(!getFirePowerDisabled());
          break;
        case 3:
          setNextBlackOutLightsOn(!getBlackOutLightsOn());
          setNextEngineSmokeOn(!getEngineSmokeOn());
          setNextTailLightsOn(!getTailLightsOn());
          setNextDamageState(rpr::DamageStatusEnum32::noDamage);
          break;
        case 4:
          setNextBrakeLightsOn(!getBrakeLightsOn());
          setNextFormationLightsOn(!getFormationLightsOn());
          setNextDamageState(rpr::DamageStatusEnum32::slightDamage);
          break;
        case 5:
          setNextRampDeployed(!getRampDeployed());
          setNextDamageState(rpr::DamageStatusEnum32::destroyed);
          setNextEngineSmokeOn(!getEngineSmokeOn());
          break;
        case 6:
          setNextTailLightsOn(!getTailLightsOn());
          setNextEngineSmokeOn(!getEngineSmokeOn());
          setNextDamageState(rpr::DamageStatusEnum32::noDamage);
          setNextCamouflageType(rpr::CamouflageEnum32::uniformPaintScheme);
          break;
        case 7:
          setNextEngineSmokeOn(!getEngineSmokeOn());
          setNextTentDeployed(!getTentDeployed());
          setNextBrakeLightsOn(!getBrakeLightsOn());
          setNextDamageState(rpr::DamageStatusEnum32::slightDamage);
          break;
        case 8:
          setNextAcousticSignatureIndex(distrib_(gen_));  // NOLINT
          setNextCamouflageType(rpr::CamouflageEnum32::forestCamouflage);
          break;
        case 9:
          setNextCamouflageType(rpr::CamouflageEnum32::desertCamouflage);
          setNextFirePowerDisabled(!getFirePowerDisabled());
          break;
        case 10:
          setNextEngineSmokeOn(!getEngineSmokeOn());
          break;
        default:
          break;
      }
    }
  }

private:
  SpatialNoise noise_;
  rpr::SpatialFPStruct spatial_;
  std::random_device rd_;
  std::mt19937 gen_;
  std::uniform_int_distribution<> distrib_;
};

SEN_EXPORT_CLASS(DummyAircraft)

}  // namespace dummy_entities
