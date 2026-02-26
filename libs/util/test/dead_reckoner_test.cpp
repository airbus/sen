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

}  // namespace sen::util
