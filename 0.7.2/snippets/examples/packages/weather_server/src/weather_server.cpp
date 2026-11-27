// === weather_server.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated code
#include "netn/netn-base.xml.h"
#include "netn/netn-metoc.xml.h"
#include "stl/weather_server.stl.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/uuid.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component_api.h"

// package
#include "randomize.h"

// std
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace weather_server  // NOLINT
{

/// The skeleton of a weather server that produces random data
class WeatherServerImpl final: public WeatherServerBase
{
public:
  SEN_NOCOPY_NOMOVE(WeatherServerImpl)

public:
  using WeatherServerBase::WeatherServerBase;
  ~WeatherServerImpl() final { env_->remove(objs_); }

public:
  void registered(sen::kernel::RegistrationApi& api) final
  {
    sen::UuidRandomGenerator()().copy(serviceId_);  // create a UUID for us
    env_ = api.getSource(getTargetBus());           // get the bus we will use to publish our objects
  }

protected:  // WeatherServer methods
  netn::WeatherCondition reqWeatherImpl(const netn::GeoReferenceVariant& ref, bool asObject) final
  {
    return makeResponse<netn::WeatherCondition, netn::WeatherBase<>>(ref, asObject);
  }

  netn::LandSurfaceCondition reqLandSurfaceImpl(const netn::GeoReferenceVariant& ref, bool asObject) final
  {
    return makeResponse<netn::LandSurfaceCondition, netn::LandSurfaceBase<>>(ref, asObject);
  }

  netn::TroposphereLayerCondition reqTroposphereLayerImpl(const netn::GeoReferenceVariant& ref,
                                                          bool asObject,
                                                          const netn::LayerStruct& layer) final
  {
    std::ignore = layer;
    return makeResponse<netn::TroposphereLayerCondition, netn::TroposphereLayerBase<>>(ref, asObject);
  }

  netn::WaterSurfaceCondition reqWaterSurfaceImpl(const netn::GeoReferenceVariant& ref, bool asObject) final
  {
    return makeResponse<netn::WaterSurfaceCondition, netn::WaterSurfaceBase<>>(ref, asObject);
  }

  netn::SubsurfaceLayerCondition reqSubsurfaceLayerImpl(const netn::GeoReferenceVariant& ref,
                                                        bool asObject,
                                                        const netn::LayerStruct& layer) final
  {
    std::ignore = layer;
    return makeResponse<netn::SubsurfaceLayerCondition, netn::SubsurfaceLayerBase<>>(ref, asObject);
  }

private:
  template <typename R, typename O>
  [[nodiscard]] R makeResponse(const netn::GeoReferenceVariant& ref, bool asObject)
  {
    auto response = MakeRandom<R>::make(ref);                                     // create a random response
    response.environmentObjectId = asObject ? addObject<O>(ref) : netn::UUID {};  // if needed, create the object
    return response;
  }

  template <typename T>
  [[nodiscard]] netn::UUID addObject(const netn::GeoReferenceVariant& ref)
  {
    netn::UUID id;
    sen::UuidRandomGenerator()().copy(id);                                   // create an id for our object
    const auto name = getName() + ".object" + std::to_string(objs_.size());  // create a name
    objs_.push_back(std::make_shared<T>(name, id, name, serviceId_, ref));   // save our new instance
    env_->add(objs_.back());                                                 // publish it to the bus
    return id;
  }

private:
  std::shared_ptr<sen::ObjectSource> env_;
  std::vector<std::shared_ptr<netn::METOCRootBase<>>> objs_;
  netn::UUID serviceId_;
};

SEN_EXPORT_CLASS(WeatherServerImpl)

}  // namespace weather_server
