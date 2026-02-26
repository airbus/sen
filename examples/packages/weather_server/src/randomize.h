// === randomize.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_WEATHER_SERVER_RANDOMIZE_H
#define SEN_WEATHER_SERVER_RANDOMIZE_H

// generated code
#include "netn/netn-metoc.xml.h"

// sen
#include "sen/core/base/numbers.h"

namespace weather_server  // NOLINT
{

template <typename T = float32_t>
[[nodiscard]] T getRand(T low, T high) noexcept
{
  return low + static_cast<T>(rand()) / (static_cast<T>(static_cast<T>(RAND_MAX) / (high - low)));  // NOLINT
}

template <typename T>
void randomizeData(T& result, const netn::GeoReferenceVariant& ref)
{
  result.geoReference = ref;
  result.barometricPressure = getRand(0.0f, 50.0f);
  result.humidity = getRand(0.0f, 100.0f);
  result.temperature = getRand(-40.0f, 40.0f);
  result.visibilityRange = getRand(0.0f, 50000.0f);
  result.haze.density = getRand(0.0f, 1.0f);
  result.haze.type = netn::HazeTypeEnum32::fog;
  result.precipitation.intensity = getRand(0.0f, 1.0f);
  result.precipitation.type = netn::PrecipitationTypeEnum32::rain;
  result.wind.direction = getRand(0.0f, 360.0f);
  result.wind.horizontalSpeed = getRand(0.0f, 500.0f);
  result.wind.verticalSpeed = getRand(0.0f, 500.0f);
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
    elem.cloud.coverage = getRand(0.0f, 1.0f);
    elem.cloud.density = getRand(0.0f, 1.0f);
    elem.cloud.type = netn::CloudTypeEnum32::cirrostratus;
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
    elem.ice.coverage = getRand(0.0f, 100.0f);
    elem.ice.thickness = getRand(0.0f, 3000.0f);
    elem.ice.type = netn::IceTypeEnum16::ice;
    elem.current.direction = getRand(0.0f, 360.0f);
    elem.current.speed = getRand(0.0f, 50.0f);
    elem.salinity = getRand(0.0f, 20.0f);
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
    elem.current.direction = getRand(0.0f, 360.0f);
    elem.current.speed = getRand(0.0f, 50.0f);
    elem.salinity = getRand(0.0f, 20.0f);
    elem.temperature = getRand(-20.0f, 400.0f);
    elem.bottomType = netn::SedimentTypeEnum32::mud;
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
    elem.moisture = netn::SurfaceMoistureEnum16::wet;
    elem.iceCondition = netn::RoadIceConditionEnum16::patches;
    return elem;
  }
};

}  // namespace weather_server

#endif  // SEN_WEATHER_SERVER_RANDOMIZE_H
