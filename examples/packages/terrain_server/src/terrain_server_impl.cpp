// === terrain_server_impl.cpp =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated code
#include "netn/netn-base.xml.h"
#include "netn/netn-metoc.xml.h"
#include "stl/terrain_server.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/uuid.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component_api.h"

// std
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace terrain_server
{

//------------------------------------------------------------------------------------------------------------//
// Random data generators
//------------------------------------------------------------------------------------------------------------//

/// A very basic random number generator.
template <typename T = float32_t>
[[nodiscard]] T getRand(T low, T high) noexcept
{
  return low + static_cast<T>(rand()) / (static_cast<T>(static_cast<T>(RAND_MAX) / (high - low)));  // NOLINT
}

/// Returns a HatHot structure filled in with random data.
HatHot makeRandomHatHot()
{
  HatHot hatHot {};
  hatHot.hat = getRand<float64_t>(0.0, 10000.0);
  hatHot.hot = getRand<float64_t>(0.0, 10000.0);
  hatHot.materialCode = getRand<uint32_t>(0, 127);
  hatHot.normalVector.azimuth = getRand<float32_t>(-180.0f, 180.0f);
  hatHot.normalVector.elevation = getRand<float32_t>(-90.0f, 90.0f);
  hatHot.point.altitude = getRand<float64_t>(0.0, 10000.0);
  hatHot.point.latitude = getRand<float64_t>(-90.0, 90.0);
  hatHot.point.longitude = getRand<float64_t>(-180.0, 180.0);
  return hatHot;
}

/// Returns a LoS structure filled in with random data.
LoS makeRandomLoS()
{
  LoS los {};
  los.range = getRand<float64_t>(0.0, 10000.0);
  los.materialCode = getRand<uint32_t>(0, 127);
  los.normalVector.azimuth = getRand<float32_t>(-180.0f, 180.0f);
  los.normalVector.elevation = getRand<float32_t>(-90.0f, 90.0f);
  los.intersection.altitude = getRand<float64_t>(0.0, 10000.0);
  los.intersection.latitude = getRand<float64_t>(-90.0, 90.0);
  los.intersection.longitude = getRand<float64_t>(-180.0, 180.0);
  return los;
}

/// Implementation of a HatHotQuery that generates random data.
class HatHotQuery: public HatHotQueryBase
{
public:
  SEN_NOCOPY_NOMOVE(HatHotQuery)

public:
  using HatHotQueryBase::HatHotQueryBase;
  ~HatHotQuery() override = default;

public:
  void update(sen::kernel::RunApi& /*api*/) override { setNextHatHot(makeRandomHatHot()); }
};

/// Implementation of a LineOfSightQuery that generates random data.
class LineOfSightQuery: public LosQueryBase
{
public:
  SEN_NOCOPY_NOMOVE(LineOfSightQuery)

public:
  using LosQueryBase::LosQueryBase;
  ~LineOfSightQuery() override = default;

public:
  void update(sen::kernel::RunApi& /*api*/) override { setNextLos(makeRandomLoS()); }
};

//------------------------------------------------------------------------------------------------------------//
// TerrainServerImpl
//------------------------------------------------------------------------------------------------------------//

class TerrainServerImpl final: public TerrainServerBase
{
public:
  SEN_NOCOPY_NOMOVE(TerrainServerImpl)

public:
  using TerrainServerBase::TerrainServerBase;

  ~TerrainServerImpl() override
  {
    env_->remove(objs_);  // remove all the objects that we published.
  }

protected:
  void registered(sen::kernel::RegistrationApi& api) override
  {
    env_ = api.getSource(getTargetBus());  // open and store the bus where we will publish
  }

protected:  // TerrainServer methods
  [[nodiscard]] LoS requestLosImpl(const netn::GeoReferenceVariant& ref,
                                   const Range& /*range*/,
                                   bool /*noClouds*/,
                                   bool asObject) override
  {
    addObjectIfNeeded<LineOfSightQuery>(asObject, ref);
    return makeRandomLoS();
  }

  [[nodiscard]] HatHot requestHatHotImpl(const netn::GeoReferenceVariant& ref, bool asObject) override
  {
    addObjectIfNeeded<HatHotQuery>(asObject, ref);
    return makeRandomHatHot();
  }

private:
  template <typename T>
  void addObjectIfNeeded(bool needed, const netn::GeoReferenceVariant& geoReference)
  {
    if (needed)
    {
      objs_.push_back(std::make_shared<T>(getName() + ".object" + std::to_string(objs_.size()), geoReference));
      env_->add(objs_.back());
    }
  }

private:
  std::shared_ptr<sen::ObjectSource> env_;
  std::vector<std::shared_ptr<sen::NativeObject>> objs_;
};

SEN_EXPORT_CLASS(TerrainServerImpl)

}  // namespace terrain_server
