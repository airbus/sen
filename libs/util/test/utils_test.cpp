// === utils_test.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "utils.h"

// sen
#include "sen/util/dr/algorithms.h"

// gtest
#include <gtest/gtest.h>

// sen
#include "sen/util/dr/algorithms.h"

namespace sen::util
{

/// @test
/// Tests the computeWgs84Trajectory helper
/// @requirements(SEN-1057)
TEST(UtilsTest, computeWgs84Trajectory)
{
  const GeodeticWorldLocation start1 {50.0663888889, -5.71472222222, 0.0};
  const GeodeticWorldLocation end1 {58.6438888889, -3.07, 0.0};

  const GeodeticWorldLocation start2 {-20.0663888889, 12.7147222222, 0.0};
  const GeodeticWorldLocation end2 {29.6438888889, -120.07, 0.0};

  const auto [surfaceDistance1, heading1, pitch1] = haversine(start1, end1);
  const auto [surfaceDistance2, heading2, pitch2] = haversine(start2, end2);

  EXPECT_NEAR(968900, surfaceDistance1, 50.0);
  EXPECT_NEAR(0.1591691796446381, heading1, 0.001);
  EXPECT_NEAR(15167352, surfaceDistance2, 50.0);
  EXPECT_NEAR(-1.18101582344964, heading2, 0.001);
}

}  // namespace sen::util
