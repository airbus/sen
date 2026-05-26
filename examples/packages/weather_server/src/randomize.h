// === randomize.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_WEATHER_SERVER_RANDOMIZE_H
#define SEN_WEATHER_SERVER_RANDOMIZE_H

// generated code
#include "hla_fom/hla.stl.h"
#include "netn/netn-metoc.xml.h"

// sen
#include "sen/core/base/numbers.h"

// std
#include <random>

namespace weather_server  // NOLINT
{

template <typename T = float32_t>
[[nodiscard]] T getRand(T low, T high) noexcept
{
  static std::mt19937 rng {std::random_device {}()};
  std::uniform_real_distribution<T> dist {low, high};
  return dist(rng);
}

template <typename T>
void randomizeData(T& result, const netn::GeoReferenceVariant& ref)
{
  result.geoReference = ref;
  result.barometricPressure = hla::MaybeF32(getRand(0.0f, 50.0f));
  result.humidity = hla::MaybeF32(getRand(0.0f, 100.0f));
  result.temperature = getRand(-40.0f, 40.0f);
  result.visibilityRange = getRand(0.0f, 50000.0f);

  netn::HazeStruct haze;
  haze.density = getRand(0.0f, 1.0f);
  haze.type = netn::HazeTypeEnum32::fog;
  result.haze = haze;

  netn::PrecipitationStruct precipitation;
  precipitation.intensity = getRand(0.0f, 1.0f);
  precipitation.type = netn::PrecipitationTypeEnum32::rain;
  result.precipitation = precipitation;

  netn::WindStruct wind;
  wind.direction = getRand(0.0f, 360.0f);
  wind.horizontalSpeed = getRand(0.0f, 500.0f);
  wind.verticalSpeed = getRand(0.0f, 500.0f);
  result.wind = wind;
}

template <typename T>
struct MakeRandom
{
};

template <>
struct MakeRandom<netn::WeatherCondition>
{
  static netn::WeatherCondition make(const netn::GeoReferenceVariant& ref)
  {
    netn::WeatherCondition elem {};
    randomizeData(elem, ref);
    return elem;
  }
};

template <>
struct MakeRandom<netn::TroposphereLayerCondition>
{
  static netn::TroposphereLayerCondition make(const netn::GeoReferenceVariant& ref)
  {
    netn::TroposphereLayerCondition elem {};
    randomizeData(elem, ref);

    netn::CloudStruct cloud;
    cloud.coverage = getRand(0.0f, 1.0f);
    cloud.density = getRand(0.0f, 1.0f);
    cloud.type = netn::CloudTypeEnum32::cirrostratus;
    elem.cloud = cloud;

    return elem;
  }
};

template <>
struct MakeRandom<netn::WaterSurfaceCondition>
{
  static netn::WaterSurfaceCondition make(const netn::GeoReferenceVariant& ref)
  {
    netn::WaterSurfaceCondition elem {};
    randomizeData(elem, ref);

    netn::IceStruct ice;
    ice.coverage = getRand(0.0f, 100.0f);
    ice.thickness = getRand(0.0f, 3000.0f);
    ice.type = netn::IceTypeEnum16::ice;
    elem.ice = ice;

    netn::CurrentStruct current;
    current.direction = getRand(0.0f, 360.0f);
    current.speed = getRand(0.0f, 50.0f);
    elem.current = current;

    elem.salinity = hla::MaybeF32(getRand(0.0f, 20.0f));
    return elem;
  }
};

template <>
struct MakeRandom<netn::SubsurfaceLayerCondition>
{
  static netn::SubsurfaceLayerCondition make(const netn::GeoReferenceVariant& ref)
  {
    netn::SubsurfaceLayerCondition elem {};
    elem.geoReference = ref;

    netn::CurrentStruct current;
    current.direction = getRand(0.0f, 360.0f);
    current.speed = getRand(0.0f, 50.0f);
    elem.current = current;

    elem.salinity = hla::MaybeF32(getRand(0.0f, 20.0f));
    elem.temperature = getRand(-20.0f, 400.0f);
    elem.bottomType = netn::MaybeSedimentTypeEnum32(netn::SedimentTypeEnum32::mud);
    return elem;
  }
};

template <>
struct MakeRandom<netn::LandSurfaceCondition>
{
  static netn::LandSurfaceCondition make(const netn::GeoReferenceVariant& ref)
  {
    netn::LandSurfaceCondition elem {};
    randomizeData(elem, ref);
    elem.moisture = netn::MaybeSurfaceMoistureEnum16(netn::SurfaceMoistureEnum16::wet);
    elem.iceCondition = netn::MaybeRoadIceConditionEnum16(netn::RoadIceConditionEnum16::patches);
    return elem;
  }
};

}  // namespace weather_server

#endif  // SEN_WEATHER_SERVER_RANDOMIZE_H
