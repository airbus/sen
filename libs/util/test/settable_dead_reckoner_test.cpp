// === settable_dead_reckoner_test.cpp =================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
#include "sen/util/dr/algorithms.h"
#include "sen/util/dr/dead_reckoner.h"
#include "sen/util/dr/dead_reckoner_base.h"
#include "sen/util/dr/detail/dead_reckoner_impl.h"
#include "sen/util/dr/settable_dead_reckoner.h"

// gtest
#include <gtest/gtest.h>

// std
#include <chrono>
#include <cstddef>
#include <utility>
#include <variant>

namespace sen::util
{

// The templates read their RPR counterparts by member name, so a stand-in carries the names and not
// only the shape.
struct TestLocation
{
  f64 x {};
  f64 y {};
  f64 z {};
};

struct TestOrientation
{
  f32 psi {};
  f32 theta {};
  f32 phi {};
};

struct TestVelocity
{
  f32 xVelocity {};
  f32 yVelocity {};
  f32 zVelocity {};
};

struct TestAcceleration
{
  f32 xAcceleration {};
  f32 yAcceleration {};
  f32 zAcceleration {};
};

struct TestAngularVelocity
{
  f32 xAngularVelocity {};
  f32 yAngularVelocity {};
  f32 zAngularVelocity {};
};

// Member order follows the order SettableDeadReckoner aggregate-initialises these in. Nine variant
// positions are built from five types: each body-referenced algorithm reuses its world counterpart.
struct TestStaticSpatial
{
  TestLocation worldLocation {};
  bool isFrozen {};
  TestOrientation orientation {};
};

struct TestFpsSpatial
{
  TestLocation worldLocation {};
  bool isFrozen {};
  TestOrientation orientation {};
  TestVelocity velocityVector {};
};

struct TestRpsSpatial
{
  TestLocation worldLocation {};
  bool isFrozen {};
  TestOrientation orientation {};
  TestVelocity velocityVector {};
  TestAngularVelocity angularVelocity {};
};

struct TestRvsSpatial
{
  TestLocation worldLocation {};
  bool isFrozen {};
  TestOrientation orientation {};
  TestVelocity velocityVector {};
  TestAcceleration accelerationVector {};
  TestAngularVelocity angularVelocity {};
};

struct TestFvsSpatial
{
  TestLocation worldLocation {};
  bool isFrozen {};
  TestOrientation orientation {};
  TestVelocity velocityVector {};
  TestAcceleration accelerationVector {};
};

// Free rather than members, so the stand-ins stay pure data like the RPR types they replace.
[[nodiscard]] inline bool operator==(const TestLocation& a, const TestLocation& b) noexcept
{
  return a.x == b.x && a.y == b.y && a.z == b.z;
}
[[nodiscard]] inline bool operator!=(const TestLocation& a, const TestLocation& b) noexcept { return !(a == b); }

[[nodiscard]] inline bool operator==(const TestOrientation& a, const TestOrientation& b) noexcept
{
  return a.psi == b.psi && a.theta == b.theta && a.phi == b.phi;
}
[[nodiscard]] inline bool operator!=(const TestOrientation& a, const TestOrientation& b) noexcept { return !(a == b); }

[[nodiscard]] inline bool operator==(const TestVelocity& a, const TestVelocity& b) noexcept
{
  return a.xVelocity == b.xVelocity && a.yVelocity == b.yVelocity && a.zVelocity == b.zVelocity;
}
[[nodiscard]] inline bool operator!=(const TestVelocity& a, const TestVelocity& b) noexcept { return !(a == b); }

[[nodiscard]] inline bool operator==(const TestAcceleration& a, const TestAcceleration& b) noexcept
{
  return a.xAcceleration == b.xAcceleration && a.yAcceleration == b.yAcceleration && a.zAcceleration == b.zAcceleration;
}
[[nodiscard]] inline bool operator!=(const TestAcceleration& a, const TestAcceleration& b) noexcept
{
  return !(a == b);
}

[[nodiscard]] inline bool operator==(const TestAngularVelocity& a, const TestAngularVelocity& b) noexcept
{
  return a.xAngularVelocity == b.xAngularVelocity && a.yAngularVelocity == b.yAngularVelocity &&
         a.zAngularVelocity == b.zAngularVelocity;
}
[[nodiscard]] inline bool operator!=(const TestAngularVelocity& a, const TestAngularVelocity& b) noexcept
{
  return !(a == b);
}

[[nodiscard]] inline bool operator==(const TestStaticSpatial& a, const TestStaticSpatial& b) noexcept
{
  return a.worldLocation == b.worldLocation && a.isFrozen == b.isFrozen && a.orientation == b.orientation;
}
[[nodiscard]] inline bool operator!=(const TestStaticSpatial& a, const TestStaticSpatial& b) noexcept
{
  return !(a == b);
}

[[nodiscard]] inline bool operator==(const TestFpsSpatial& a, const TestFpsSpatial& b) noexcept
{
  return a.worldLocation == b.worldLocation && a.isFrozen == b.isFrozen && a.orientation == b.orientation &&
         a.velocityVector == b.velocityVector;
}
[[nodiscard]] inline bool operator!=(const TestFpsSpatial& a, const TestFpsSpatial& b) noexcept { return !(a == b); }

[[nodiscard]] inline bool operator==(const TestRpsSpatial& a, const TestRpsSpatial& b) noexcept
{
  return a.worldLocation == b.worldLocation && a.isFrozen == b.isFrozen && a.orientation == b.orientation &&
         a.velocityVector == b.velocityVector && a.angularVelocity == b.angularVelocity;
}
[[nodiscard]] inline bool operator!=(const TestRpsSpatial& a, const TestRpsSpatial& b) noexcept { return !(a == b); }

[[nodiscard]] inline bool operator==(const TestRvsSpatial& a, const TestRvsSpatial& b) noexcept
{
  return a.worldLocation == b.worldLocation && a.isFrozen == b.isFrozen && a.orientation == b.orientation &&
         a.velocityVector == b.velocityVector && a.accelerationVector == b.accelerationVector &&
         a.angularVelocity == b.angularVelocity;
}
[[nodiscard]] inline bool operator!=(const TestRvsSpatial& a, const TestRvsSpatial& b) noexcept { return !(a == b); }

[[nodiscard]] inline bool operator==(const TestFvsSpatial& a, const TestFvsSpatial& b) noexcept
{
  return a.worldLocation == b.worldLocation && a.isFrozen == b.isFrozen && a.orientation == b.orientation &&
         a.velocityVector == b.velocityVector && a.accelerationVector == b.accelerationVector;
}
[[nodiscard]] inline bool operator!=(const TestFvsSpatial& a, const TestFvsSpatial& b) noexcept { return !(a == b); }

using TestSpatialVariant = std::variant<TestStaticSpatial,
                                        TestFpsSpatial,
                                        TestRpsSpatial,
                                        TestRvsSpatial,
                                        TestFvsSpatial,
                                        TestFpsSpatial,
                                        TestRpsSpatial,
                                        TestRvsSpatial,
                                        TestFvsSpatial>;

/// Stands in for a generated RPR entity. Counts writes, since some tests are about whether one happens.
class TestEntity
{
public:
  [[nodiscard]] const TestSpatialVariant& getSpatial() const noexcept { return spatial_; }
  [[nodiscard]] const TestSpatialVariant& getNextSpatial() const noexcept { return spatial_; }

  void setNextSpatial(const TestSpatialVariant& value)
  {
    spatial_ = value;
    ++writes_;
  }

  [[nodiscard]] int writes() const noexcept { return writes_; }

  /// Seeds the spatial without counting as a write.
  void seed(const TestSpatialVariant& value) { spatial_ = value; }

private:
  TestSpatialVariant spatial_;
  int writes_ {0};
};

namespace
{

constexpr sen::TimeStamp t0 {std::chrono::seconds(0)};
constexpr sen::TimeStamp t1 {std::chrono::seconds(1)};

/// A situation far enough from the extrapolation to always exceed the threshold, so the tests are
/// about which spatial gets written rather than about whether one does.
Situation situationAt(f64 x, Velocity velocity, AngularVelocity omega, Acceleration acceleration)
{
  Situation value {};
  value.timeStamp = t1;
  value.worldLocation = {x, 0.0, 0.0};
  value.velocityVector = velocity;
  value.angularVelocity = omega;
  value.accelerationVector = acceleration;
  return value;
}

}  // namespace

/// @test
/// An entity turning on the spot must not be written as a static spatial, which carries no angular
/// velocity and would silently drop the rotation.
/// @requirements(SEN-1058)
TEST(SettableDeadReckonerTest, rotatingInPlaceIsNotWrittenAsStatic)
{
  TestEntity entity;
  SettableDeadReckoner<TestEntity> reckoner {entity};

  const auto wrote = reckoner.setSpatial(situationAt(1000.0, {}, {0.0F, 0.0F, 0.5F}, {}));

  ASSERT_TRUE(wrote);
  EXPECT_NE(entity.getSpatial().index(), static_cast<size_t>(SpatialAlgorithm::drStatic));
}

/// @test
/// The rotation of an entity turning on the spot survives the write.
/// @requirements(SEN-1058)
TEST(SettableDeadReckonerTest, rotatingInPlaceKeepsItsAngularVelocity)
{
  TestEntity entity;
  SettableDeadReckoner<TestEntity> reckoner {entity};

  reckoner.setSpatial(situationAt(1000.0, {}, {0.0F, 0.0F, 0.5F}, {}));

  ASSERT_EQ(entity.getSpatial().index(), static_cast<size_t>(SpatialAlgorithm::drRPW));
  EXPECT_FLOAT_EQ(std::get<2>(entity.getSpatial()).angularVelocity.zAngularVelocity, 0.5F);
}

/// @test
/// An entity that is neither moving, accelerating nor rotating is still written as static.
/// @requirements(SEN-1058)
TEST(SettableDeadReckonerTest, motionlessEntityIsStillWrittenAsStatic)
{
  TestEntity entity;
  SettableDeadReckoner<TestEntity> reckoner {entity};

  reckoner.setSpatial(situationAt(1000.0, {}, {}, {}));

  EXPECT_EQ(entity.getSpatial().index(), static_cast<size_t>(SpatialAlgorithm::drStatic));
}

/// @test
/// setFrozen publishes when it changes the frozen state.
/// @requirements(SEN-1058)
TEST(SettableDeadReckonerTest, setFrozenWritesWhenTheStateChanges)
{
  TestEntity entity;
  SettableDeadReckoner<TestEntity> reckoner {entity};

  reckoner.setFrozen(t0, true);

  EXPECT_EQ(entity.writes(), 1);
}

/// @test
/// setFrozen does not publish when the frozen state already matches: writing costs a publication.
/// @requirements(SEN-1058)
TEST(SettableDeadReckonerTest, setFrozenDoesNotWriteWhenTheStateAlreadyMatches)
{
  TestEntity entity;
  SettableDeadReckoner<TestEntity> reckoner {entity};

  reckoner.setFrozen(t0, false);

  EXPECT_EQ(entity.writes(), 0);
}

/// @test
/// A second setFrozen with the same value does not publish again.
/// @requirements(SEN-1058)
TEST(SettableDeadReckonerTest, setFrozenIsIdempotent)
{
  TestEntity entity;
  SettableDeadReckoner<TestEntity> reckoner {entity};

  reckoner.setFrozen(t0, true);
  reckoner.setFrozen(t1, true);

  EXPECT_EQ(entity.writes(), 1);
}

//---------------------------------------------------------------------------------------------------------------------
// Extrapolation origin
//---------------------------------------------------------------------------------------------------------------------

namespace
{
constexpr sen::TimeStamp t3 {std::chrono::seconds(3)};
}  // namespace

/// The commit instant is reached through asObject() on a generated RPR type. The dead reckoner only
/// needs that expression to be valid, so this stands in for it without being a Sen object.
class TestCommitSource
{
public:
  [[nodiscard]] sen::TimeStamp getLastCommitTime() const noexcept { return commitTime_; }
  void setCommitTime(sen::TimeStamp value) noexcept { commitTime_ = value; }

private:
  sen::TimeStamp commitTime_;
};

/// Stands in for a generated RPR entity that reports when its data was committed.
class TestEntityWithCommitTime: public TestEntity
{
public:
  [[nodiscard]] const TestCommitSource& asObject() const noexcept { return source_; }
  [[nodiscard]] TestCommitSource& commitSource() noexcept { return source_; }

private:
  TestCommitSource source_ {};
};

namespace
{

/// A spatial moving along x at ten metres per second, tagged as the world-referenced constant
/// velocity algorithm.
TestSpatialVariant movingAlongX()
{
  return TestSpatialVariant {std::in_place_index<static_cast<size_t>(SpatialAlgorithm::drFPW)>,
                             TestFpsSpatial {{6378137.0, 0.0, 0.0}, false, {}, {10.0F, 0.0F, 0.0F}}};
}

}  // namespace

/// @test
/// The commit instant is detected and never required: a stand-in without one still compiles and is
/// simply treated as unable to report it.
/// @requirements(SEN-1058)
TEST(DeadReckonerOriginTest, theCommitInstantIsDetectedAndNotRequired)
{
  EXPECT_FALSE(impl::HasCommitTime<TestEntity>::value);
  EXPECT_TRUE(impl::HasCommitTime<TestEntityWithCommitTime>::value);
}

/// @test
/// When the object reports when its data was committed, the extrapolation is measured from that
/// instant rather than from the instant a consumer first read it.
/// @requirements(SEN-1058)
TEST(DeadReckonerOriginTest, theOriginIsTheInstantTheProducerCommitted)
{
  TestEntityWithCommitTime entity;
  entity.seed(movingAlongX());
  entity.commitSource().setCommitTime(t1);

  DrConfig config {};
  config.smoothing = false;
  DeadReckoner<TestEntityWithCommitTime> reckoner {entity, config};

  const auto situation = reckoner.situation(t3);

  // Two seconds at ten metres per second, measured from the commit and not from this first read.
  EXPECT_NEAR(static_cast<f64>(situation.worldLocation.x), 6378157.0, 0.5);
}

/// @test
/// Without a reported commit instant the origin stays the instant of the first read, so a stand-in
/// keeps the behaviour it had before.
/// @requirements(SEN-1058)
TEST(DeadReckonerOriginTest, withoutACommitInstantTheFirstReadIsTheOrigin)
{
  TestEntity entity;
  entity.seed(movingAlongX());

  DrConfig config {};
  config.smoothing = false;
  DeadReckoner<TestEntity> reckoner {entity, config};

  const auto situation = reckoner.situation(t3);

  EXPECT_NEAR(static_cast<f64>(situation.worldLocation.x), 6378137.0, 0.5);
}

/// @test
/// An object that has never been committed reports the epoch, which is not a usable origin.
/// @requirements(SEN-1058)
TEST(DeadReckonerOriginTest, anObjectThatHasNeverCommittedFallsBackToTheReadInstant)
{
  TestEntityWithCommitTime entity;
  entity.seed(movingAlongX());

  DrConfig config {};
  config.smoothing = false;
  DeadReckoner<TestEntityWithCommitTime> reckoner {entity, config};

  const auto situation = reckoner.situation(t3);

  EXPECT_NEAR(static_cast<f64>(situation.worldLocation.x), 6378137.0, 0.5);
}

/// @test
/// The switch restores the old origin for a producer whose clock is not the caller's.
/// @requirements(SEN-1058)
TEST(DeadReckonerOriginTest, theSwitchRestoresTheReadInstantAsOrigin)
{
  TestEntityWithCommitTime entity;
  entity.seed(movingAlongX());
  entity.commitSource().setCommitTime(t1);

  DrConfig config {};
  config.smoothing = false;
  config.useCommitTimeAsOrigin = false;
  DeadReckoner<TestEntityWithCommitTime> reckoner {entity, config};

  const auto situation = reckoner.situation(t3);

  EXPECT_NEAR(static_cast<f64>(situation.worldLocation.x), 6378137.0, 0.5);
}

//---------------------------------------------------------------------------------------------------------------------
// Configured algorithm
//---------------------------------------------------------------------------------------------------------------------

namespace
{

constexpr sen::TimeStamp tHalf {std::chrono::milliseconds(500)};
constexpr sen::TimeStamp tSixTenths {std::chrono::milliseconds(600)};

Situation movingAndAccelerating()
{
  Situation value {};
  value.timeStamp = t0;
  value.worldLocation = {6378137.0, 0.0, 0.0};
  value.velocityVector = {10.0F, 20.0F, 0.0F};
  value.accelerationVector = {1.0F, 2.0F, 0.0F};
  value.angularVelocity = {0.1F, 0.0F, 0.0F};
  return value;
}

}  // namespace

/// @test
/// The configured algorithm defaults to the one the class used before it was configurable.
/// @requirements(SEN-1058)
TEST(DeadReckonerAlgorithmTest, theDefaultIsRvw)
{
  DrConfig config {};
  config.smoothing = false;
  DeadReckonerBase reckoner {config};
  reckoner.updateSituation(movingAndAccelerating());

  const auto expected = drRvw(movingAndAccelerating(), t1);

  EXPECT_DOUBLE_EQ(static_cast<f64>(reckoner.situation(t1).worldLocation.x),
                   static_cast<f64>(expected.worldLocation.x));
}

/// @test
/// Naming another algorithm makes the class use it.
/// @requirements(SEN-1058)
TEST(DeadReckonerAlgorithmTest, theConfiguredAlgorithmIsTheOneApplied)
{
  DrConfig config {};
  config.smoothing = false;
  config.algorithm = SpatialAlgorithm::drFPW;
  DeadReckonerBase reckoner {config};
  reckoner.updateSituation(movingAndAccelerating());

  const auto sameOrientation = [](const Orientation& a, const Orientation& b)
  { return a.psi == b.psi && a.theta == b.theta && a.phi == b.phi; };

  const auto fpw = drFpw(movingAndAccelerating(), t1);
  const auto rvw = drRvw(movingAndAccelerating(), t1);
  const auto got = reckoner.situation(t1);

  // FPW holds the orientation and RVW turns it by the angular velocity. That is what separates the
  // two; the position is not, since both extrapolate it with the same function.
  EXPECT_TRUE(sameOrientation(got.orientation, fpw.orientation));
  EXPECT_FALSE(sameOrientation(fpw.orientation, rvw.orientation));
}

/// @test
/// Smoothing reaches a world referenced algorithm. The first query resets the smoothed solution
/// because it is too far from the default, so the divergence shows on the second.
/// @requirements(SEN-1058)
TEST(DeadReckonerAlgorithmTest, aWorldAlgorithmIsSmoothed)
{
  DrConfig config {};
  DeadReckonerBase reckoner {config};
  reckoner.updateSituation(movingAndAccelerating());

  reckoner.situation(tHalf);
  const auto got = static_cast<f64>(reckoner.situation(tSixTenths).worldLocation.y);
  const auto raw = static_cast<f64>(drRvw(movingAndAccelerating(), tSixTenths).worldLocation.y);

  EXPECT_NE(got, raw);
}

/// @test
/// A body referenced algorithm is not smoothed, matching the classes that take an RPR Spatial.
/// @requirements(SEN-1058)
TEST(DeadReckonerAlgorithmTest, aBodyAlgorithmIsNotSmoothed)
{
  DrConfig config {};
  config.algorithm = SpatialAlgorithm::drRVB;
  DeadReckonerBase reckoner {config};
  reckoner.updateSituation(movingAndAccelerating());

  reckoner.situation(tHalf);
  const auto got = static_cast<f64>(reckoner.situation(tSixTenths).worldLocation.y);
  const auto raw = static_cast<f64>(drRvb(movingAndAccelerating(), tSixTenths).worldLocation.y);

  EXPECT_DOUBLE_EQ(got, raw);
}

//---------------------------------------------------------------------------------------------------------------------
// Reference frame
//---------------------------------------------------------------------------------------------------------------------

namespace
{

// At the origin with no rotation the body and world conversions coincide, so a test placed there
// passes either way. This sits off the equator and the meridian, with an attitude.
constexpr TestLocation offAxisLocation {3189068.0, 3189068.0, 4487348.0};
constexpr TestOrientation offAxisOrientation {0.5F, 0.3F, 0.2F};

TestSpatialVariant offAxisWorldReferenced()
{
  return TestSpatialVariant {
    std::in_place_index<static_cast<size_t>(SpatialAlgorithm::drRVW)>,
    TestRvsSpatial {offAxisLocation, false, offAxisOrientation, {10.0F, 20.0F, 30.0F}, {}, {}}};
}

TestSpatialVariant offAxisBodyReferenced()
{
  return TestSpatialVariant {
    std::in_place_index<static_cast<size_t>(SpatialAlgorithm::drRVB)>,
    TestRvsSpatial {offAxisLocation, false, offAxisOrientation, {10.0F, 20.0F, 30.0F}, {}, {}}};
}

}  // namespace

/// @test
/// A reckoner whose object changes algorithm reads it as one built on that algorithm would. The two
/// frames convert velocity differently, so getting this wrong is visible there.
/// @requirements(SEN-1058)
TEST(DeadReckonerFrameTest, theFrameFollowsTheSpatial)
{
  DrConfig config {};
  config.smoothing = false;

  TestEntity changed;
  changed.seed(offAxisWorldReferenced());
  DeadReckoner<TestEntity> overChanged {changed, config};
  overChanged.geodeticSituation(tHalf);
  changed.seed(offAxisBodyReferenced());

  TestEntity fresh;
  fresh.seed(offAxisBodyReferenced());
  DeadReckoner<TestEntity> overFresh {fresh, config};

  const auto changedResult = overChanged.geodeticSituation(tSixTenths);
  const auto freshResult = overFresh.geodeticSituation(tSixTenths);

  // A frame error shows up as metres per second, so the tolerance only has to exclude float noise.
  EXPECT_NEAR(static_cast<f64>(changedResult.velocityVector.x), static_cast<f64>(freshResult.velocityVector.x), 1e-3);
  EXPECT_NEAR(static_cast<f64>(changedResult.velocityVector.y), static_cast<f64>(freshResult.velocityVector.y), 1e-3);
  EXPECT_NEAR(static_cast<f64>(changedResult.velocityVector.z), static_cast<f64>(freshResult.velocityVector.z), 1e-3);
}

}  // namespace sen::util
