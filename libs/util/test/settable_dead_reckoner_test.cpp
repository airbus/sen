// === settable_dead_reckoner_test.cpp =================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/timestamp.h"
#include "sen/util/dr/settable_dead_reckoner.h"

// gtest
#include <gtest/gtest.h>

// std
#include <chrono>
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

  bool operator==(const TestLocation& o) const noexcept { return x == o.x && y == o.y && z == o.z; }
};

struct TestOrientation
{
  f32 psi {};
  f32 theta {};
  f32 phi {};

  bool operator==(const TestOrientation& o) const noexcept { return psi == o.psi && theta == o.theta && phi == o.phi; }
};

struct TestVelocity
{
  f32 xVelocity {};
  f32 yVelocity {};
  f32 zVelocity {};

  bool operator==(const TestVelocity& o) const noexcept
  {
    return xVelocity == o.xVelocity && yVelocity == o.yVelocity && zVelocity == o.zVelocity;
  }
};

struct TestAcceleration
{
  f32 xAcceleration {};
  f32 yAcceleration {};
  f32 zAcceleration {};

  bool operator==(const TestAcceleration& o) const noexcept
  {
    return xAcceleration == o.xAcceleration && yAcceleration == o.yAcceleration && zAcceleration == o.zAcceleration;
  }
};

struct TestAngularVelocity
{
  f32 xAngularVelocity {};
  f32 yAngularVelocity {};
  f32 zAngularVelocity {};

  bool operator==(const TestAngularVelocity& o) const noexcept
  {
    return xAngularVelocity == o.xAngularVelocity && yAngularVelocity == o.yAngularVelocity &&
           zAngularVelocity == o.zAngularVelocity;
  }
};

// Member order follows the order SettableDeadReckoner aggregate-initialises these in. Nine variant
// positions are built from five types: each body-referenced algorithm reuses its world counterpart.
struct TestStaticSpatial
{
  TestLocation worldLocation {};
  bool isFrozen {};
  TestOrientation orientation {};

  bool operator==(const TestStaticSpatial& o) const noexcept
  {
    return worldLocation == o.worldLocation && isFrozen == o.isFrozen && orientation == o.orientation;
  }
  bool operator!=(const TestStaticSpatial& o) const noexcept { return !(*this == o); }
};

struct TestFpsSpatial
{
  TestLocation worldLocation {};
  bool isFrozen {};
  TestOrientation orientation {};
  TestVelocity velocityVector {};

  bool operator==(const TestFpsSpatial& o) const noexcept
  {
    return worldLocation == o.worldLocation && isFrozen == o.isFrozen && orientation == o.orientation &&
           velocityVector == o.velocityVector;
  }
  bool operator!=(const TestFpsSpatial& o) const noexcept { return !(*this == o); }
};

struct TestRpsSpatial
{
  TestLocation worldLocation {};
  bool isFrozen {};
  TestOrientation orientation {};
  TestVelocity velocityVector {};
  TestAngularVelocity angularVelocity {};

  bool operator==(const TestRpsSpatial& o) const noexcept
  {
    return worldLocation == o.worldLocation && isFrozen == o.isFrozen && orientation == o.orientation &&
           velocityVector == o.velocityVector && angularVelocity == o.angularVelocity;
  }
  bool operator!=(const TestRpsSpatial& o) const noexcept { return !(*this == o); }
};

struct TestRvsSpatial
{
  TestLocation worldLocation {};
  bool isFrozen {};
  TestOrientation orientation {};
  TestVelocity velocityVector {};
  TestAcceleration accelerationVector {};
  TestAngularVelocity angularVelocity {};

  bool operator==(const TestRvsSpatial& o) const noexcept
  {
    return worldLocation == o.worldLocation && isFrozen == o.isFrozen && orientation == o.orientation &&
           velocityVector == o.velocityVector && accelerationVector == o.accelerationVector &&
           angularVelocity == o.angularVelocity;
  }
  bool operator!=(const TestRvsSpatial& o) const noexcept { return !(*this == o); }
};

struct TestFvsSpatial
{
  TestLocation worldLocation {};
  bool isFrozen {};
  TestOrientation orientation {};
  TestVelocity velocityVector {};
  TestAcceleration accelerationVector {};

  bool operator==(const TestFvsSpatial& o) const noexcept
  {
    return worldLocation == o.worldLocation && isFrozen == o.isFrozen && orientation == o.orientation &&
           velocityVector == o.velocityVector && accelerationVector == o.accelerationVector;
  }
  bool operator!=(const TestFvsSpatial& o) const noexcept { return !(*this == o); }
};

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
  TestSpatialVariant spatial_ {};
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
  sen::TimeStamp commitTime_ {};
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

}  // namespace sen::util
