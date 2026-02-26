// === coordinates_test.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "utils.h"

// sen
#include "sen/core/base/numbers.h"
#include "sen/util/dr/algorithms.h"
#include "sen/util/dr/detail/dead_reckoner_impl.h"

// implementation
#include "constants.h"
#include "vec3.h"

// gtest
#include <gtest/gtest.h>

namespace sen::util
{

constexpr f64 error = 1e-4;

/// @test
/// Tests the change from ECEF coordinates to geodetic coordinates
/// @requirements(SEN-1057)
TEST(Coordinates, EcefToGeodetic)
{
  // x > 0; y > 0; z > 0
  Location input {4592.0498392998425e3, 1862.9075560081942e3, 4001.918277954002e3};
  const auto result = impl::toLla(input);
  EXPECT_NEAR(39.1113730028, result.latitude, error);
  EXPECT_NEAR(22.0814617667, result.longitude, error);
  EXPECT_NEAR(0.0, result.altitude, error);
}

/// @test
/// Tests the change from geodetic coordinates to ECEF coordinates
/// @requirements(SEN-1057)
TEST(Coordinates, GeodeticToEcef)
{
  // lat > 0 long > 0 alt > 0
  GeodeticWorldLocation input {39.1113730028, 22.0814617667, 0.0};
  const auto result = impl::toEcef(input);
  EXPECT_NEAR(4592.0498392998425e3, result.x, error);
  EXPECT_NEAR(1862.9075560081942e3, result.y, error);
  EXPECT_NEAR(4001.918277954002e3, result.z, error);
}

/// @test
/// Tests the change from body coordinates to NED coordinates
/// @requirements(SEN-1057)
TEST(Coordinates, bodyToNed)
{
  // change of coordinates from Body coordinates to NED coordinates
  const Vec3d value {1, 0, 0};
  const Orientation orientationNed {halfPi, -halfPi, 0};

  const auto result = bodyToNed(value, orientationNed);

  EXPECT_NEAR(0, result.getX(), 0.001);
  EXPECT_NEAR(0, result.getY(), 0.001);
  EXPECT_NEAR(1, result.getZ(), 0.001);
}

/// @test
/// Tests the change of base of a vector from ECEF coordinates to NED coordinates
/// @requirements(SEN-1057)
TEST(Coordinates, ecefToNed)
{
  {
    const GeodeticWorldLocation lla {61.64, 30.70, 0};
    Vec3d ecefVector {530.2445, 492.1283, 396.3459};
    const Vec3d nedVector {-434.0403, 152.4451, -684.6964};
    const auto result = ecefToNed(ecefVector, lla);
    EXPECT_NEAR(nedVector.getX(), result.getX(), 1e-3);
    EXPECT_NEAR(nedVector.getY(), result.getY(), 1e-3);
    EXPECT_NEAR(nedVector.getZ(), result.getZ(), 1e-3);
  }
  {
    const GeodeticWorldLocation lla {-67.64, -70.70, 0};
    Vec3d ecefVector {-190.4573, 82.6247, -798.3350};
    const Vec3d nedVector {-434.0403, -152.4451, -684.6964};
    const auto result = ecefToNed(ecefVector, lla);
    EXPECT_NEAR(nedVector.getX(), result.getX(), 1e-3);
    EXPECT_NEAR(nedVector.getY(), result.getY(), 1e-3);
    EXPECT_NEAR(nedVector.getZ(), result.getZ(), 1e-3);
  }
}

/// @test
/// Tests the change of base of a vector from NED coordinates to ECEF coordinates
/// @requirements(SEN-1057)
TEST(Coordinates, nedToEcef)
{
  {
    const GeodeticWorldLocation lla {61.64, 30.70, 0};
    Vec3d ecefVector {530.2445, 492.1283, 396.3459};
    const Vec3d nedVector {-434.0403, 152.4451, -684.6964};
    const auto result = nedToEcef(nedVector, lla);
    EXPECT_NEAR(ecefVector.getX(), result.getX(), 1e-3);
    EXPECT_NEAR(ecefVector.getY(), result.getY(), 1e-3);
    EXPECT_NEAR(ecefVector.getZ(), result.getZ(), 1e-3);
  }
  {
    const GeodeticWorldLocation lla {-67.64, -70.70, 0};
    Vec3d ecefVector {-190.4573, 82.6247, -798.3350};
    const Vec3d nedVector {-434.0403, -152.4451, -684.6964};
    const auto result = nedToEcef(nedVector, lla);
    EXPECT_NEAR(ecefVector.getX(), result.getX(), 1e-3);
    EXPECT_NEAR(ecefVector.getY(), result.getY(), 1e-3);
    EXPECT_NEAR(ecefVector.getZ(), result.getZ(), 1e-3);
  }
}

}  // namespace sen::util
