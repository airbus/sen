// === dead_reckoner_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/timestamp.h"
#include "sen/util/dr/algorithms.h"
#include "sen/util/dr/detail/dead_reckoner_base.h"
#include "sen/util/dr/detail/dead_reckoner_impl.h"

// implementation
#include "constants.h"
#include "utils.h"

// gtest
#include <gtest/gtest.h>

// std
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>

namespace sen::util
{

// timestamps used for the extrapolations of the tests
constexpr sen::TimeStamp initialTimeStamp {std::chrono::seconds(0)};
constexpr sen::TimeStamp currentTimeStamp {std::chrono::seconds(2)};

// test helper to check cache accessors of DeadReckonerBase
class TestDeadReckoner: public DeadReckonerBase
{
public:
  using DeadReckonerBase::DeadReckonerBase;
  using DeadReckonerBase::getCachedGeodeticSituation;
  using DeadReckonerBase::getCachedSituation;
  using DeadReckonerBase::isGeodeticSituationCached;
  using DeadReckonerBase::isSituationCached;
};
/// @test
/// Tests the position extrapolation using the FPW algorithm
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, drFpw)
{
  // situation to extrapolate
  Situation input {false, initialTimeStamp, {}, {}, {10, -20, 35}};

  const auto situation = drFpw(input, currentTimeStamp);

  EXPECT_NEAR(20, situation.worldLocation.x, 0.01);
  EXPECT_NEAR(-40, situation.worldLocation.y, 0.01);
  EXPECT_NEAR(70, situation.worldLocation.z, 0.01);
}

/// @test
/// Tests the position/orientation extrapolation using the RPW algorithm
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, drRpw)
{
  // situation to extrapolate
  Situation input {false, initialTimeStamp, {}, {toRad(45.0), 0, 0}, {-10, 10, 30}, {0, 0, toRad(-20.0)}};

  const auto situation = drRpw(input, currentTimeStamp);

  EXPECT_NEAR(-20, situation.worldLocation.x, 0.01);
  EXPECT_NEAR(20, situation.worldLocation.y, 0.01);
  EXPECT_NEAR(60, situation.worldLocation.z, 0.01);
  EXPECT_NEAR(5, toDeg(situation.orientation.psi), 0.01);
  EXPECT_NEAR(0, toDeg(situation.orientation.theta), 0.01);
  EXPECT_NEAR(0, toDeg(situation.orientation.phi), 0.01);
}

/// @test
/// Tests the position/orientation extrapolation using the RPW algorithm
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, drRvw)
{
  // situation to extrapolate
  Situation input {false, initialTimeStamp, {}, {toRad(45.0), 0, 0}, {-10, 10, 30}, {0, 0, toRad(-20.0)}, {2, 2, 2}};

  const auto situation = drRvw(input, currentTimeStamp);

  EXPECT_NEAR(-16, situation.worldLocation.x, 0.01);
  EXPECT_NEAR(24, situation.worldLocation.y, 0.01);
  EXPECT_NEAR(64, situation.worldLocation.z, 0.01);
  EXPECT_NEAR(5, toDeg(situation.orientation.psi), 0.01);
  EXPECT_NEAR(0, toDeg(situation.orientation.theta), 0.01);
  EXPECT_NEAR(0, toDeg(situation.orientation.phi), 0.01);
}

/// @test
/// Tests the position extrapolation using the FVW algorithm
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, drFvw)
{
  // situation to extrapolate
  Situation input {false, initialTimeStamp, {}, {toRad(45.0), 0, 0}, {-10, 10, 30}, {}, {-2, 2, 2}};

  const auto situation = drFvw(input, currentTimeStamp);

  EXPECT_NEAR(-24, situation.worldLocation.x, 0.01);
  EXPECT_NEAR(24, situation.worldLocation.y, 0.01);
  EXPECT_NEAR(64, situation.worldLocation.z, 0.01);
}

/// @test
/// Tests the position extrapolation using the FPB algorithm
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, drFpb)
{
  // situation to extrapolate
  Situation input {false, initialTimeStamp, {}, {toRad(90.0), 0, 0}, {-10, 10, 30}};

  const auto situation = drFpb(input, currentTimeStamp);

  EXPECT_NEAR(-20, situation.worldLocation.x, 0.01);
  EXPECT_NEAR(-20, situation.worldLocation.y, 0.01);
  EXPECT_NEAR(60, situation.worldLocation.z, 0.01);
}

/// @test
/// Tests the position extrapolation using the FVB algorithm
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, drFvb)
{
  // situation to extrapolate
  Situation input {false, initialTimeStamp, {}, {0, 0, toRad(90.0)}, {-10, 10, 30}, {}, {2, 2, 2}};

  const auto situation = drFvb(input, currentTimeStamp);

  EXPECT_NEAR(-16, situation.worldLocation.x, 0.01);
  EXPECT_NEAR(-64, situation.worldLocation.y, 0.01);
  EXPECT_NEAR(24, situation.worldLocation.z, 0.01);
}

/// @test
/// Tests the position/orientation extrapolation using the RPB algorithm
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, drRpb)
{
  // situation to extrapolate
  Situation input {false, initialTimeStamp, {}, {}, {10, 0, 0}, {0, toRad(90.0), 0}};

  const auto situation = drRpb(input, currentTimeStamp);

  EXPECT_NEAR(0, situation.worldLocation.x, 0.01);
  EXPECT_NEAR(0, situation.worldLocation.y, 0.01);
  EXPECT_NEAR(-(20 / pi) * 2, situation.worldLocation.z, 0.01);
  EXPECT_NEAR(180, toDeg(situation.orientation.psi), 0.01);
  EXPECT_NEAR(0, toDeg(situation.orientation.theta), 0.01);
  EXPECT_NEAR(180, toDeg(situation.orientation.phi), 0.01);
}

/// @test
/// Tests the position/orientation extrapolation using the RVB algorithm
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, drRvb)
{
  // situation to extrapolate
  Situation input {false, initialTimeStamp, {}, {}, {8, 0, 0}, {0, 0, toRad(45.0)}, {2, 0, 0}};

  const auto situation = drRvb(input, currentTimeStamp);

  EXPECT_NEAR(48 / pi - 32 / (pi * pi), situation.worldLocation.x, 0.01);
  EXPECT_NEAR(32 / pi + 32 / (pi * pi), situation.worldLocation.y, 0.01);
  EXPECT_NEAR(0, situation.worldLocation.z, 0.01);
  EXPECT_NEAR(90, toDeg(situation.orientation.psi), 0.01);
  EXPECT_NEAR(0, toDeg(situation.orientation.theta), 0.01);
  EXPECT_NEAR(0, toDeg(situation.orientation.phi), 0.01);
}

/// @test
/// Tests the transformation from orientation with respect to ECEF coordinates to orientation with respect to NED
/// coordinates
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, ecefToNedOrientation)
{
  // arrange first check
  const Orientation ecef1 {1.459474, 0.040305, -1.285481};
  const Orientation ned1 {1.623785, 0.000049, 0.991385};
  const GeodeticWorldLocation worldLocation1 {40.415009, -4.406782, 4529.685323};

  // arrange second check
  const Orientation ecef2 {0.561413, -0.260930, -2.480676};
  const Orientation ned2 {1.208138, 0.000109, -0.119408};
  const GeodeticWorldLocation worldLocation2 {43.366036, -72.432330, 5000.074299};

  const auto result1 = impl::ecefToNed(ecef1, worldLocation1);
  const auto result2 = impl::ecefToNed(ecef2, worldLocation2);

  // check expected euler angles with respect to ned
  EXPECT_NEAR(result1.psi, ned1.psi, 0.001);
  EXPECT_NEAR(result1.theta, ned1.theta, 0.001);
  EXPECT_NEAR(result1.phi, ned1.phi, 0.001);
  EXPECT_NEAR(result2.psi, ned2.psi, 0.001);
  EXPECT_NEAR(result2.theta, ned2.theta, 0.001);
  EXPECT_NEAR(result2.phi, ned2.phi, 0.001);
}

/// @test
/// Tests that situation cache contains the correct value after calling the function that populates it
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, situationCachePopulated)
{
  DrConfig drConfig = {};
  drConfig.smoothing = false;
  TestDeadReckoner dr(drConfig);

  const sen::TimeStamp t1 {std::chrono::seconds(10)};
  const sen::TimeStamp queryTime = t1 + std::chrono::seconds(2);
  const Situation input {false, t1, {100, 200, 300}, {}, {10, -20, 35}};
  dr.updateSituation(input);

  EXPECT_FALSE(dr.isSituationCached(queryTime));

  const auto result = dr.situation(queryTime);

  EXPECT_TRUE(dr.isSituationCached(queryTime));
  EXPECT_EQ(dr.getCachedSituation().worldLocation.x, result.worldLocation.x);
  EXPECT_EQ(dr.getCachedSituation().worldLocation.y, result.worldLocation.y);
  EXPECT_EQ(dr.getCachedSituation().worldLocation.z, result.worldLocation.z);
}

/// @test
/// Tests that situation cache is not valid for a different timestamp
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, situationCacheDifferentTimestamp)
{
  DrConfig drConfig = {};
  drConfig.smoothing = false;
  TestDeadReckoner dr(drConfig);

  const sen::TimeStamp t1 {std::chrono::seconds(10)};
  const sen::TimeStamp queryTime = t1 + std::chrono::seconds(2);
  const sen::TimeStamp queryTime2 = t1 + std::chrono::seconds(4);
  const Situation input {false, t1, {100, 200, 300}, {}, {10, -20, 35}};
  dr.updateSituation(input);

  [[maybe_unused]] const auto result = dr.situation(queryTime);

  EXPECT_TRUE(dr.isSituationCached(queryTime));
  EXPECT_FALSE(dr.isSituationCached(queryTime2));
}

/// @test
/// Tests that geodetic situation cache contains the correct value after calling the function that populates it
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, geodeticSituationCachePopulated)
{
  DrConfig drConfig = {};
  drConfig.smoothing = false;
  TestDeadReckoner dr(drConfig);

  const sen::TimeStamp t1 {std::chrono::seconds(10)};
  const sen::TimeStamp queryTime = t1 + std::chrono::seconds(2);
  const GeodeticSituation input {false, t1, {40.741895, -73.989308, 30}, {}, {10, -20, 35}};
  dr.updateGeodeticSituation(input);

  EXPECT_FALSE(dr.isGeodeticSituationCached(queryTime));

  const auto result = dr.geodeticSituation(queryTime);

  EXPECT_TRUE(dr.isGeodeticSituationCached(queryTime));
  EXPECT_EQ(dr.getCachedGeodeticSituation().worldLocation.latitude, result.worldLocation.latitude);
  EXPECT_EQ(dr.getCachedGeodeticSituation().worldLocation.longitude, result.worldLocation.longitude);
  EXPECT_EQ(dr.getCachedGeodeticSituation().worldLocation.altitude, result.worldLocation.altitude);
}

/// @test
/// Tests that geodetic situation cache is not valid for a different timestamp
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, geodeticSituationCacheDifferentTimestamp)
{
  DrConfig drConfig = {};
  drConfig.smoothing = false;
  TestDeadReckoner dr(drConfig);

  const sen::TimeStamp t1 {std::chrono::seconds(10)};
  const sen::TimeStamp queryTime = t1 + std::chrono::seconds(2);
  const sen::TimeStamp queryTime2 = t1 + std::chrono::seconds(4);
  const GeodeticSituation input {false, t1, {40.741895, -73.989308, 30}, {}, {10, -20, 35}};
  dr.updateGeodeticSituation(input);

  [[maybe_unused]] const auto result = dr.geodeticSituation(queryTime);

  EXPECT_TRUE(dr.isGeodeticSituationCached(queryTime));
  EXPECT_FALSE(dr.isGeodeticSituationCached(queryTime2));
}

/// @test
/// Tests that situation cache is invalidated when a new situation is updated
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, situationCacheInvalidatedOnUpdate)
{
  DrConfig drConfig = {};
  drConfig.smoothing = false;
  TestDeadReckoner dr(drConfig);

  const sen::TimeStamp t1 {std::chrono::seconds(10)};
  const sen::TimeStamp queryTime = t1 + std::chrono::seconds(2);
  const Situation input1 {false, t1, {100, 200, 300}, {}, {10, -20, 35}};
  dr.updateSituation(input1);
  [[maybe_unused]] const auto result = dr.situation(queryTime);

  EXPECT_TRUE(dr.isSituationCached(queryTime));

  const Situation input2 {false, t1, {100, 200, 300}, {}, {-20, 40, 50}};
  dr.updateSituation(input2);

  EXPECT_FALSE(dr.isSituationCached(queryTime));
}

/// @test
/// Tests that geodetic situation cache is invalidated when a new situation is updated
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, geodeticSituationCacheInvalidatedOnUpdate)
{
  DrConfig drConfig = {};
  drConfig.smoothing = false;
  TestDeadReckoner dr(drConfig);

  const sen::TimeStamp t1 {std::chrono::seconds(10)};
  const sen::TimeStamp queryTime = t1 + std::chrono::seconds(2);
  const GeodeticSituation input1 {false, t1, {40.741895, -73.989308, 30}, {}, {10, -20, 35}};
  dr.updateGeodeticSituation(input1);
  [[maybe_unused]] const auto result = dr.geodeticSituation(queryTime);

  EXPECT_TRUE(dr.isGeodeticSituationCached(queryTime));

  const GeodeticSituation input2 {false, t1, {-37.828098, 144.974496, 30}, {}, {-10, 20, -35}};
  dr.updateGeodeticSituation(input2);

  EXPECT_FALSE(dr.isGeodeticSituationCached(queryTime));
}

namespace
{

// The conversions as they read before the rotation was shared, so bit exactness is checked.
GeodeticSituation referenceGeodeticSituation(const Situation& value)
{
  const auto geoLocation = impl::toLla(value.worldLocation);
  return GeodeticSituation {value.isFrozen,
                            value.timeStamp,
                            geoLocation,
                            impl::ecefToNed(value.orientation, geoLocation),
                            impl::ecefToNed(value.velocityVector, geoLocation),
                            value.angularVelocity,
                            impl::ecefToNed(value.accelerationVector, geoLocation),
                            value.angularAcceleration};
}

Situation referenceSituation(const GeodeticSituation& value)
{
  return Situation {value.isFrozen,
                    value.timeStamp,
                    impl::toEcef(value.worldLocation),
                    impl::nedToEcef(value.orientation, value.worldLocation),
                    impl::nedToEcef(value.velocityVector, value.worldLocation),
                    value.angularVelocity,
                    impl::nedToEcef(value.accelerationVector, value.worldLocation),
                    value.angularAcceleration};
}

// Equator, two mid latitudes and both poles beside the date line, to take both branches in toEcef.
const GeodeticSituation geodeticSamples[] = {
  {false,
   initialTimeStamp,
   {0.0, 0.0, 0.0},
   {0.1, 0.2, 0.3},
   {10, -20, 35},
   {0.01, 0.02, 0.03},
   {1.0, -2.0, 0.5},
   {0.001, 0.002, 0.003}},
  {false,
   initialTimeStamp,
   {48.8566, 2.3522, 35.0},
   {1.2, -0.4, 2.9},
   {-120, 45, -8},
   {0.2, 0.0, -0.1},
   {-1.5, 0.25, 3.0},
   {0.03, -0.01, 0.0}},
  {false,
   initialTimeStamp,
   {-33.8688, 151.2093, 120.0},
   {-2.0, 0.9, -1.1},
   {300, 0, 12},
   {0.0, 0.5, 0.0},
   {0.0, 0.0, -9.81},
   {0.0, 0.2, 0.0}},
  {false,
   initialTimeStamp,
   {89.5, -179.5, 11000.0},
   {0.5, 0.5, 0.5},
   {5, 5, 5},
   {0.1, 0.1, 0.1},
   {1.0, 1.0, 1.0},
   {0.01, 0.01, 0.01}},
  {false,
   initialTimeStamp,
   {-89.9, 179.9, -50.0},
   {3.0, -1.5, 0.2},
   {-7, 3, 90},
   {-0.3, 0.0, 0.4},
   {2.0, -2.0, 0.0},
   {0.0, -0.05, 0.02}},
};

void expectSameOrientation(const Orientation& expected, const Orientation& actual)
{
  EXPECT_EQ(expected.psi.get(), actual.psi.get());
  EXPECT_EQ(expected.theta.get(), actual.theta.get());
  EXPECT_EQ(expected.phi.get(), actual.phi.get());
}

void expectSameSituation(const Situation& expected, const Situation& actual)
{
  EXPECT_EQ(expected.isFrozen, actual.isFrozen);
  EXPECT_EQ(expected.timeStamp, actual.timeStamp);
  EXPECT_EQ(expected.worldLocation.x.get(), actual.worldLocation.x.get());
  EXPECT_EQ(expected.worldLocation.y.get(), actual.worldLocation.y.get());
  EXPECT_EQ(expected.worldLocation.z.get(), actual.worldLocation.z.get());
  expectSameOrientation(expected.orientation, actual.orientation);
  EXPECT_EQ(expected.velocityVector.x.get(), actual.velocityVector.x.get());
  EXPECT_EQ(expected.velocityVector.y.get(), actual.velocityVector.y.get());
  EXPECT_EQ(expected.velocityVector.z.get(), actual.velocityVector.z.get());
  EXPECT_EQ(expected.accelerationVector.x.get(), actual.accelerationVector.x.get());
  EXPECT_EQ(expected.accelerationVector.y.get(), actual.accelerationVector.y.get());
  EXPECT_EQ(expected.accelerationVector.z.get(), actual.accelerationVector.z.get());
  EXPECT_EQ(expected.angularVelocity.x.get(), actual.angularVelocity.x.get());
  EXPECT_EQ(expected.angularVelocity.y.get(), actual.angularVelocity.y.get());
  EXPECT_EQ(expected.angularVelocity.z.get(), actual.angularVelocity.z.get());
  EXPECT_EQ(expected.angularAcceleration.x.get(), actual.angularAcceleration.x.get());
  EXPECT_EQ(expected.angularAcceleration.y.get(), actual.angularAcceleration.y.get());
  EXPECT_EQ(expected.angularAcceleration.z.get(), actual.angularAcceleration.z.get());
}

void expectSameGeodeticSituation(const GeodeticSituation& expected, const GeodeticSituation& actual)
{
  EXPECT_EQ(expected.isFrozen, actual.isFrozen);
  EXPECT_EQ(expected.timeStamp, actual.timeStamp);
  EXPECT_EQ(expected.worldLocation.latitude.get(), actual.worldLocation.latitude.get());
  EXPECT_EQ(expected.worldLocation.longitude.get(), actual.worldLocation.longitude.get());
  EXPECT_EQ(expected.worldLocation.altitude.get(), actual.worldLocation.altitude.get());
  expectSameOrientation(expected.orientation, actual.orientation);
  EXPECT_EQ(expected.velocityVector.x.get(), actual.velocityVector.x.get());
  EXPECT_EQ(expected.velocityVector.y.get(), actual.velocityVector.y.get());
  EXPECT_EQ(expected.velocityVector.z.get(), actual.velocityVector.z.get());
  EXPECT_EQ(expected.accelerationVector.x.get(), actual.accelerationVector.x.get());
  EXPECT_EQ(expected.accelerationVector.y.get(), actual.accelerationVector.y.get());
  EXPECT_EQ(expected.accelerationVector.z.get(), actual.accelerationVector.z.get());
  EXPECT_EQ(expected.angularVelocity.x.get(), actual.angularVelocity.x.get());
  EXPECT_EQ(expected.angularVelocity.y.get(), actual.angularVelocity.y.get());
  EXPECT_EQ(expected.angularVelocity.z.get(), actual.angularVelocity.z.get());
  EXPECT_EQ(expected.angularAcceleration.x.get(), actual.angularAcceleration.x.get());
  EXPECT_EQ(expected.angularAcceleration.y.get(), actual.angularAcceleration.y.get());
  EXPECT_EQ(expected.angularAcceleration.z.get(), actual.angularAcceleration.z.get());
}

}  // namespace

/// @test
/// Tests that sharing the rotation leaves every field of the geodetic conversion bit for bit equal
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, geodeticConversionUnchangedBySharedRotation)
{
  for (const auto& sample: geodeticSamples)
  {
    const auto ecefSample = referenceSituation(sample);
    expectSameGeodeticSituation(referenceGeodeticSituation(ecefSample), impl::toGeodeticSituation(ecefSample));
  }
}

/// @test
/// Tests the same for the conversion to ECEF, through both overloads
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, ecefConversionUnchangedBySharedRotation)
{
  for (const auto& sample: geodeticSamples)
  {
    const auto expected = referenceSituation(sample);
    expectSameSituation(expected, impl::toSituation(sample));
    expectSameSituation(
      expected,
      impl::toSituation(
        sample, impl::toEcef(sample.worldLocation), impl::nedToEcef(sample.orientation, sample.worldLocation)));
  }
}

/// @test
/// Tests that the angular acceleration of a geodetic situation survives the conversion to ECEF
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, angularAccelerationSurvivesEcefConversion)
{
  const GeodeticSituation input {
    false, initialTimeStamp, {40.741895, -73.989308, 30.0}, {}, {10, -20, 35}, {}, {}, {0.25, -0.5, 0.75}};

  const auto result = impl::toSituation(input);

  EXPECT_EQ(0.25, result.angularAcceleration.x.get());
  EXPECT_EQ(-0.5, result.angularAcceleration.y.get());
  EXPECT_EQ(0.75, result.angularAcceleration.z.get());
}

namespace
{

// The orientation conversions as they read before becoming a quaternion composition.
Orientation referenceEcefToNed(const Orientation& value, const GeodeticWorldLocation& latLonAlt)
{
  const auto [x0, y0, z0] = getNedTrihedron(latLonAlt);
  const Quatd inputOrientation {
    static_cast<double>(value.psi.get()), static_cast<double>(value.theta.get()), static_cast<double>(value.phi.get())};
  const auto xf = inputOrientation * Vec3d {1, 0, 0};
  const auto yf = inputOrientation * Vec3d {0, 1, 0};
  return eulerAnglesFromTrihedrons(x0, y0, z0, xf, yf);
}

Orientation referenceNedToEcef(const Orientation& value, const GeodeticWorldLocation& latLonAlt)
{
  const auto [x0, y0, z0] = getNedTrihedron(latLonAlt);
  const auto xf =
    Quatd {static_cast<double>(value.theta.get()), y0} * Quatd {static_cast<double>(value.psi.get()), z0} * x0;
  const auto yf =
    Quatd {static_cast<double>(value.phi.get()), x0} * Quatd {static_cast<double>(value.psi.get()), z0} * y0;
  return eulerAnglesFromTrihedrons(Vec3d {1, 0, 0}, Vec3d {0, 1, 0}, Vec3d {0, 0, 1}, xf, yf);
}

// Euler angles are degenerate at a right-angle pitch, so two correct answers can differ there. The
// rotation they denote must not: this is the angle between two of them.
double rotationGap(const Orientation& lhs, const Orientation& rhs)
{
  const Quatd a {
    static_cast<double>(lhs.psi.get()), static_cast<double>(lhs.theta.get()), static_cast<double>(lhs.phi.get())};
  const Quatd b {
    static_cast<double>(rhs.psi.get()), static_cast<double>(rhs.theta.get()), static_cast<double>(rhs.phi.get())};
  return 2.0 * std::acos(std::min(1.0, std::abs(a.dot(b))));
}

// A millimetre at a kilometre of lever arm, and sixteen times the last bit of the f32 storing it.
constexpr double orientationTolerance = 1e-6;

}  // namespace

/// @test
/// Tests that the quaternion composition reproduces the trihedron construction it replaced, away
/// from the pitch limit where yaw and bank cannot be separated
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, orientationConversionMatchesTrihedronConstruction)
{
  double worst = 0.0;

  for (int lat = -90; lat <= 90; lat += 15)
  {
    for (int lon = -180; lon <= 180; lon += 45)
    {
      for (int yaw = -180; yaw <= 180; yaw += 45)
      {
        for (const double pitch: {-85.0, -60.0, -30.0, 0.0, 30.0, 60.0, 85.0})
        {
          for (int bank = -180; bank <= 180; bank += 90)
          {
            const GeodeticWorldLocation position {static_cast<double>(lat), static_cast<double>(lon), 250.0};
            const Orientation input {static_cast<float>(yaw * M_PI / 180.0),
                                     static_cast<float>(pitch * M_PI / 180.0),
                                     static_cast<float>(bank * M_PI / 180.0)};

            const auto converted = impl::ecefToNed(input, position);
            ASSERT_FALSE(std::isnan(converted.psi.get()) || std::isnan(converted.theta.get()) ||
                         std::isnan(converted.phi.get()));
            worst = std::max(worst, rotationGap(referenceEcefToNed(input, position), converted));
          }
        }
      }
    }
  }

  EXPECT_LT(worst, orientationTolerance);
}

/// @test
/// Tests that converting an orientation to NED and back returns the rotation it started from
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, orientationConversionRoundTrips)
{
  double worst = 0.0;

  for (int lat = -90; lat <= 90; lat += 15)
  {
    for (int lon = -180; lon <= 180; lon += 45)
    {
      for (int yaw = -180; yaw <= 180; yaw += 45)
      {
        for (const double pitch: {-90.0, -89.99, -45.0, 0.0, 45.0, 89.99, 90.0})
        {
          for (int bank = -180; bank <= 180; bank += 90)
          {
            const GeodeticWorldLocation position {static_cast<double>(lat), static_cast<double>(lon), 0.0};
            const Orientation input {static_cast<float>(yaw * M_PI / 180.0),
                                     static_cast<float>(pitch * M_PI / 180.0),
                                     static_cast<float>(bank * M_PI / 180.0)};

            const auto back = impl::nedToEcef(impl::ecefToNed(input, position), position);
            ASSERT_FALSE(std::isnan(back.psi.get()) || std::isnan(back.theta.get()) || std::isnan(back.phi.get()));
            worst = std::max(worst, rotationGap(input, back));
          }
        }
      }
    }
  }

  EXPECT_LT(worst, orientationTolerance);
}

/// @test
/// Tests that extrapolating with no rotation to apply returns the orientation it was given
/// @requirements(SEN-1058)
TEST(DeadReckonerTest, orientationExtrapolationHoldsStillAtThePitchLimit)
{
  const sen::Duration step {std::chrono::milliseconds(16)};

  for (const double pitchDeg: {-90.0, -89.999, -45.0, 0.0, 45.0, 89.999, 90.0})
  {
    for (int yawDeg = -180; yawDeg <= 180; yawDeg += 45)
    {
      for (int bankDeg = -165; bankDeg <= 165; bankDeg += 35)
      {
        const Orientation input {static_cast<float>(yawDeg * M_PI / 180.0),
                                 static_cast<float>(pitchDeg * M_PI / 180.0),
                                 static_cast<float>(bankDeg * M_PI / 180.0)};

        const auto held = extrapolateOrientation(input, step, AngularVelocity {});

        ASSERT_FALSE(std::isnan(held.psi.get()) || std::isnan(held.theta.get()) || std::isnan(held.phi.get()))
          << "yaw " << yawDeg << " pitch " << pitchDeg << " bank " << bankDeg;
        EXPECT_LT(rotationGap(input, held), orientationTolerance)
          << "yaw " << yawDeg << " pitch " << pitchDeg << " bank " << bankDeg;
      }
    }
  }
}

}  // namespace sen::util
