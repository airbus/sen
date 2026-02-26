// === util.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// implementation
#include "util.h"

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/component_api.h"

// generated code
#include "rpr/rpr-base_v2.0.xml.h"

// std
#include <array>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>

namespace dummy_entities
{

rpr::SpatialFPStruct& updateSpatial(rpr::SpatialFPStruct& spatial, SpatialNoise& noise, sen::kernel::RunApi& api)
{
  const std::array<float64_t, 1U> t = {api.getTime().sinceEpoch().toSeconds()};
  spatial.worldLocation.x = noise[0](t) * 1000.0;
  spatial.worldLocation.y = noise[1](t) * 1000.0;
  spatial.worldLocation.z = noise[2](t) * 1000.0;
  spatial.velocityVector.xVelocity = noise[3](t) * 100.0;
  spatial.velocityVector.yVelocity = noise[4](t) * 100.0;
  spatial.velocityVector.zVelocity = noise[5](t) * 100.0;
  spatial.orientation.phi = noise[6](t);
  spatial.orientation.psi = noise[7](t);
  spatial.orientation.theta = noise[8](t);
  return spatial;
}

rpr::SpatialVariantStruct toFpwSpatial(const rpr::SpatialFPStruct& spatial)
{
  return rpr::SpatialVariantStruct {std::in_place_index<1>, spatial};
}

void seed(SpatialNoise& noise, const std::array<std::string, 9U>& labels)
{
  for (std::size_t i = 0U; i < noise.size(); ++i)
  {
    noise.at(i).seed(sen::hashCombine(sen::crc32(labels[i]), rand(), rand()));  // NOLINT
  }
}

sen::VarMap makePlatformArgs(const sen::VarMap& args)
{
  std::ignore = args;
  static auto entityType = sen::toVariant(rpr::EntityTypeStruct {0, 1, 2, 3, 4, 5, 6});
  auto entityId = sen::toVariant(rpr::EntityIdentifierStruct {{1, 2}, 3});
  return {{"entityType", entityType}, {"entityIdentifier", entityId}, {"alternateEntityType", entityType}};
}

}  // namespace dummy_entities
