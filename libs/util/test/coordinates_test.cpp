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
#include "sen/util/dr/detail/settable_dead_reckoner_impl.h"

// implementation
#include "constants.h"
#include "vec3.h"

// gtest
#include <gtest/gtest.h>

namespace sen::util
{

constexpr f64 absoluteError = 1e-4;

/// @test
/// Tests the change from ECEF coordinates to geodetic coordinates
/// @requirements(SEN-1057)
TEST(Coordinates, EcefToGeodetic)
{
  // x > 0; y > 0; z > 0
  Location input1 {4592.0498392998425e3, 1862.9075560081942e3, 4001.918277954002e3};
  const auto result1 = impl::toLla(input1);
  EXPECT_NEAR(39.1113730028, result1.latitude, absoluteError);
  EXPECT_NEAR(22.0814617667, result1.longitude, absoluteError);
  EXPECT_NEAR(0.0, result1.altitude, absoluteError);

  Location input2 {0.0, 0.0, 42.841311e3};
  const auto result2 = impl::toLla(input2);
  EXPECT_NEAR(90.0, result2.latitude, absoluteError);
  EXPECT_NEAR(0.0, result2.longitude, absoluteError);
  EXPECT_NEAR(-6313.9110032e3, result2.altitude, absoluteError);

  Location input3 {1000.0, 0.0, 1000.0};
  const auto result3 = impl::toLla(input3);
  EXPECT_NEAR(88.69300198935349, result3.latitude, absoluteError);
  EXPECT_NEAR(0.0, result3.longitude, absoluteError);
  EXPECT_NEAR(-6355.74090950095e3, result3.altitude, absoluteError);

  Location input4 {0.0, 1000.0, -1e-200};
  const auto result4 = impl::toLla(input4);
  EXPECT_NEAR(-89.890177274147902, result4.latitude, absoluteError);
  EXPECT_NEAR(90.0, result4.longitude, absoluteError);
  EXPECT_NEAR(-77411.095180346221e3, result4.altitude, absoluteError);

  Location input5 {2745.9126465e3, -5756.9253996e3, 0.0};
  const auto result5 = impl::toLla(input5);
  EXPECT_NEAR(0.0, result5.latitude, absoluteError);
  EXPECT_NEAR(-64.5, result5.longitude, absoluteError);
  EXPECT_NEAR(125.01396, result5.altitude, absoluteError);

  Location input6 {-0.0, 40000.0, 0.0};
  const auto result6 = impl::toLla(input6);
  EXPECT_NEAR(77.64090694398732, result6.latitude, absoluteError);
  EXPECT_NEAR(90.0, result6.longitude, absoluteError);
  EXPECT_NEAR(-27729.478813921083e3, result6.altitude, absoluteError);

  Location input7 {0.0, 0.0, 42.84131151330515e3};
  const auto result7 = impl::toLla(input7);
  EXPECT_NEAR(90.0, result7.latitude, absoluteError);
  EXPECT_NEAR(0.0, result7.longitude, absoluteError);
  EXPECT_NEAR(-6313.9110027e3, result7.altitude, absoluteError);
}

/// @test
/// Tests the change from geodetic coordinates to ECEF coordinates
/// @requirements(SEN-1057)
TEST(Coordinates, GeodeticToEcef)
{
  GeodeticWorldLocation greece {39.1113730028, 22.0814617667, 0.0};
  const auto ecefGreece = impl::toEcef(greece);
  EXPECT_NEAR(4592.0498392998425e3, ecefGreece.x, absoluteError);
  EXPECT_NEAR(1862.9075560081942e3, ecefGreece.y, absoluteError);
  EXPECT_NEAR(4001.918277954002e3, ecefGreece.z, absoluteError);

  GeodeticWorldLocation northPole {90.0, 180.0, 0.0};
  const auto ecefNorthPole = impl::toEcef(northPole);
  EXPECT_NEAR(0, ecefNorthPole.x, absoluteError);
  EXPECT_NEAR(0, ecefNorthPole.y, absoluteError);
  EXPECT_NEAR(6356.7523142e3, ecefNorthPole.z, absoluteError);

  GeodeticWorldLocation china {31.0, 90.0, 0.0};
  const auto ecefChina = impl::toEcef(china);
  EXPECT_NEAR(0, ecefChina.x, absoluteError);
  EXPECT_NEAR(5471.9911594326738e3, ecefChina.y, absoluteError);
  EXPECT_NEAR(3265.8935166539e3, ecefChina.z, absoluteError);
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

  EXPECT_NEAR(0, result.getX(), absoluteError);
  EXPECT_NEAR(0, result.getY(), absoluteError);
  EXPECT_NEAR(1, result.getZ(), absoluteError);
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
    EXPECT_NEAR(nedVector.getX(), result.getX(), absoluteError);
    EXPECT_NEAR(nedVector.getY(), result.getY(), absoluteError);
    EXPECT_NEAR(nedVector.getZ(), result.getZ(), absoluteError);
  }
  {
    const GeodeticWorldLocation lla {-67.64, -70.70, 0};
    Vec3d ecefVector {-190.4573, 82.6247, -798.3350};
    const Vec3d nedVector {-434.0403, -152.4451, -684.6964};
    const auto result = ecefToNed(ecefVector, lla);
    EXPECT_NEAR(nedVector.getX(), result.getX(), absoluteError);
    EXPECT_NEAR(nedVector.getY(), result.getY(), absoluteError);
    EXPECT_NEAR(nedVector.getZ(), result.getZ(), absoluteError);
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
    EXPECT_NEAR(ecefVector.getX(), result.getX(), absoluteError);
    EXPECT_NEAR(ecefVector.getY(), result.getY(), absoluteError);
    EXPECT_NEAR(ecefVector.getZ(), result.getZ(), absoluteError);
  }
  {
    const GeodeticWorldLocation lla {-67.64, -70.70, 0};
    Vec3d ecefVector {-190.4573, 82.6247, -798.3350};
    const Vec3d nedVector {-434.0403, -152.4451, -684.6964};
    const auto result = nedToEcef(nedVector, lla);
    EXPECT_NEAR(ecefVector.getX(), result.getX(), absoluteError);
    EXPECT_NEAR(ecefVector.getY(), result.getY(), absoluteError);
    EXPECT_NEAR(ecefVector.getZ(), result.getZ(), absoluteError);
  }
}

/// @test
/// Tests the change of velocity, acceleration and angular acceleration vectors from body coordinates to NED coordinates
/// and angular velocity from body to NED coordinates and vice versa
TEST(Coordinates, bodyToNedNonDefault)
{
  const Velocity velocity {1.0f, 0.0f, 0.0f};
  const AngularVelocity angularVelocity {0.0f, 1.0f, 0.0f};
  const Acceleration acceleration {0.0f, 0.0f, 1.0f};
  const AngularAcceleration angularAcceleration {1.0f, 0.0f, 1.0f};
  const Orientation orientationNed {halfPi, -halfPi, 0};

  const auto nedVelocity = impl::bodyToNed(velocity, orientationNed);

  EXPECT_NEAR(0, nedVelocity.x, absoluteError);
  EXPECT_NEAR(0, nedVelocity.y, absoluteError);
  EXPECT_NEAR(1, nedVelocity.z, absoluteError);

  const auto nedAngularVelocity = impl::bodyToNed(angularVelocity, orientationNed);

  EXPECT_NEAR(-1, nedAngularVelocity.x, absoluteError);
  EXPECT_NEAR(0, nedAngularVelocity.y, absoluteError);
  EXPECT_NEAR(0, nedAngularVelocity.z, absoluteError);

  const auto bodyAngularVelocity = impl::nedToBody(nedAngularVelocity, orientationNed);

  EXPECT_NEAR(0, bodyAngularVelocity.x, absoluteError);
  EXPECT_NEAR(1, bodyAngularVelocity.y, absoluteError);
  EXPECT_NEAR(0, bodyAngularVelocity.z, absoluteError);

  const auto nedAcceleration = impl::bodyToNed(acceleration, orientationNed);

  EXPECT_NEAR(0, nedAcceleration.x, absoluteError);
  EXPECT_NEAR(-1, nedAcceleration.y, absoluteError);
  EXPECT_NEAR(0, nedAcceleration.z, absoluteError);

  const auto nedAngularAcceleration = impl::bodyToNed(angularAcceleration, orientationNed);

  EXPECT_NEAR(0, nedAngularAcceleration.x, absoluteError);
  EXPECT_NEAR(-1, nedAngularAcceleration.y, absoluteError);
  EXPECT_NEAR(1, nedAngularAcceleration.z, absoluteError);
}

}  // namespace sen::util
