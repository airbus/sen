// === util.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_TEST_UTIL_DUMMY_ENTITIES_SRC_UTIL_H
#define SEN_TEST_UTIL_DUMMY_ENTITIES_SRC_UTIL_H

// generated code
#include "rpr/rpr-physical_v2.0.xml.h"

// sen
#include "sen/core/base/gradient_noise.h"
#include "sen/core/base/hash32.h"
#include "sen/kernel/component_api.h"

namespace dummy_entities
{

using SpatialNoise = std::array<sen::GradientNoise<float64_t, 1U>, 9U>;

rpr::SpatialFPStruct& updateSpatial(rpr::SpatialFPStruct& spatial, SpatialNoise& noise, sen::kernel::RunApi& api);

rpr::SpatialVariantStruct toFpwSpatial(const rpr::SpatialFPStruct& spatial);

void seed(SpatialNoise& noise, const std::array<std::string, 9U>& labels);

sen::VarMap makePlatformArgs(const sen::VarMap& args);

}  // namespace dummy_entities

#endif  // SEN_TEST_UTIL_DUMMY_ENTITIES_SRC_UTIL_H
