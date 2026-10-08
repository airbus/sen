// === quat_test.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// testing file
#include "constants.h"
#include "quat.h"
#include "utils.h"
#include "vec3.h"

// sen
#include "sen/core/base/numbers.h"

// gtest
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <cmath>

using namespace sen::util;  // NOLINT

constexpr f64 error = 5 * 1e-3;

/// @test
/// Constructs the default quaternion as the identity rotation, with a zero vector part and a
/// unit scalar part.
/// @requirements(SEN-1060)
TEST(QuaternionTest, initialization)
{
  Quat quaternion = Quat<f64>();

  // expected the value
  EXPECT_EQ(quaternion.getX(), 0);
  EXPECT_EQ(quaternion.getY(), 0);
  EXPECT_EQ(quaternion.getZ(), 0);
  EXPECT_EQ(quaternion.getW(), 1);
}

/// @test
/// Composes makeRotate rotations onto the current quaternion: a half turn about x yields the
/// pure (1, 0, 0, 0) quaternion, and a following quarter turn about y lands at
/// (0.707, 0, -0.707, 0).
/// @requirements(SEN-1060)
TEST(QuaternionTest, rotation)
{
  Quat quaternion = Quat<f64>();

  // expected the value
  EXPECT_EQ(quaternion.getX(), 0);
  EXPECT_EQ(quaternion.getY(), 0);
  EXPECT_EQ(quaternion.getZ(), 0);
  EXPECT_EQ(quaternion.getW(), 1);

  quaternion.makeRotate(pi, Vec3d {1.0, 0.0, 0.0});

  ASSERT_NEAR(quaternion.getX(), 1, error);
  ASSERT_NEAR(quaternion.getY(), 0, error);
  ASSERT_NEAR(quaternion.getZ(), 0, error);
  ASSERT_NEAR(quaternion.getW(), 0, error);

  quaternion.makeRotate(pi / 2.0, Vec3d {0.0, 1.0, 0.0});

  ASSERT_NEAR(quaternion.getX(), 0.707, error);
  ASSERT_NEAR(quaternion.getY(), 0, error);
  ASSERT_NEAR(quaternion.getZ(), -0.707, error);
  ASSERT_NEAR(quaternion.getW(), 0, error);
}

/// @test
/// Builds quaternions from yaw, pitch and bank angles, reads the angles back through
/// getRotateInEulerYPB, and composes a further 60 degree rotation about x onto a 30 degree bank,
/// reading back a 90 degree bank.
/// @requirements(SEN-1060)
TEST(QuaternionTest, eulerOperations)
{
  Quat quaternion = Quat<f64>(0, 0, toRad(30.0));

  // expected the value
  ASSERT_NEAR(quaternion.getX(), 0.2588, error);
  ASSERT_NEAR(quaternion.getY(), 0, error);
  ASSERT_NEAR(quaternion.getZ(), 0, error);
  ASSERT_NEAR(quaternion.getW(), 0.9659, error);

  auto eulerAngles = quaternion.getRotateInEulerYPB();

  ASSERT_NEAR(toDeg(eulerAngles.getX()), 0, error);
  ASSERT_NEAR(toDeg(eulerAngles.getY()), 0, error);
  ASSERT_NEAR(toDeg(eulerAngles.getZ()), 30.0, error);

  quaternion.makeRotate(toRad(60.0), 1, 0, 0);

  eulerAngles = quaternion.getRotateInEulerYPB();

  ASSERT_NEAR(toDeg(eulerAngles.getX()), 0, error);
  ASSERT_NEAR(toDeg(eulerAngles.getY()), 0, error);
  ASSERT_NEAR(toDeg(eulerAngles.getZ()), 90.0, error);

  quaternion = Quat<f64>(0, toRad(30.0), 0);

  // expected the value
  ASSERT_NEAR(quaternion.getX(), 0, error);
  ASSERT_NEAR(quaternion.getY(), 0.2588, error);
  ASSERT_NEAR(quaternion.getZ(), 0, error);
  ASSERT_NEAR(quaternion.getW(), 0.9659, error);
}

/// @test
/// Reports unit length and unit squared length for quaternions built from Euler angles.
/// @requirements(SEN-1060)
TEST(QuaternionTest, length)
{
  Quat quaternion = Quat<f64>(0, 0, toRad(15.0));

  // expected the value
  EXPECT_NEAR(quaternion.length(), 1.0, error);
  EXPECT_NEAR(quaternion.length2(), 1.0, error);

  quaternion = Quat<f64>(toRad(60.0), toRad(22.0), toRad(15.0));

  // expected the value
  EXPECT_NEAR(quaternion.length(), 1.0, error);
  EXPECT_NEAR(quaternion.length2(), 1.0, error);
}

/// @test
/// Conjugates a quaternion built from Euler angles, and the conjugate decomposes into the
/// reference Euler angles of the reversed rotation.
/// @requirements(SEN-1060)
TEST(QuaternionTest, conjugate)
{
  Quat quaternion = Quat<f64>(0, toRad(60.0), toRad(15.0));

  quaternion = quaternion.conj();

  // expected the value
  auto eulerAngles = quaternion.getRotateInEulerYPB();

  // expected the value
  ASSERT_NEAR(toDeg(eulerAngles.getX()), 24.146, error);
  ASSERT_NEAR(toDeg(eulerAngles.getY()), -56.774, error);
  ASSERT_NEAR(toDeg(eulerAngles.getZ()), -28.186, error);
}

/// @test
/// Inverts a unit quaternion, matching the conjugate: the inverse decomposes into the same
/// reference Euler angles as the conjugated rotation.
/// @requirements(SEN-1060)
TEST(QuaternionTest, inverse)
{
  Quat quaternion = Quat<f64>(0, toRad(60.0), toRad(15.0));

  quaternion = quaternion.inverse();

  // expected the value
  auto eulerAngles = quaternion.getRotateInEulerYPB();

  // expected the value
  ASSERT_NEAR(toDeg(eulerAngles.getX()), 24.146, error);
  ASSERT_NEAR(toDeg(eulerAngles.getY()), -56.774, error);
  ASSERT_NEAR(toDeg(eulerAngles.getZ()), -28.186, error);
}

/// @test
/// Interpolates spherically between two rotations about the same axis, landing at the
/// proportional angle: one tenth of the way from a 15 to a 55 degree bank gives 19 degrees, two
/// tenths give 23.
/// @requirements(SEN-1060)
TEST(QuaternionTest, slerp)
{
  Quat from = Quat<f64>(0, 0, toRad(15.0));
  Quat to = Quat<f64>(0, 0, toRad(55.0));

  Quat result = Quat<f64>(0, 0, 0);

  result.slerp(0.1, from, to);

  auto eulerAngles = result.getRotateInEulerYPB();

  // expected the value
  ASSERT_NEAR(toDeg(eulerAngles.getX()), 0, error);
  ASSERT_NEAR(toDeg(eulerAngles.getY()), 0, error);
  ASSERT_NEAR(toDeg(eulerAngles.getZ()), 19, error);

  result.slerp(0.2, from, to);

  eulerAngles = result.getRotateInEulerYPB();

  // expected the value
  ASSERT_NEAR(toDeg(eulerAngles.getX()), 0, error);
  ASSERT_NEAR(toDeg(eulerAngles.getY()), 0, error);
  ASSERT_NEAR(toDeg(eulerAngles.getZ()), 23, error);
}

/// @test
/// Computes the four component dot product of two quaternions, matching the hand computed
/// value.
TEST(QuaternionTest, dot_product)
{
  Quat quaternion1 = Quat<f64> {0.44, -0.34, 0.56, -0.8};
  Quat quaternion2 = Quat<f64> {0.71, -0.98, -0.54, -0.25};

  ASSERT_NEAR(quaternion1.dot(quaternion2), 0.5432, error);
}

/// @test
/// Extracts the angle and axis of a rotation: a zero axis builds the identity and reads back as
/// zero angle about the default z axis, and a 270 degree bank reads back wrapped as minus 90
/// degrees about x.
TEST(QuaternionTest, get_rotate)
{
  Quat quaternion = Quat<f64>(pi, Vec3d {0.0, 0.0, 0.0});
  double angle;
  Vec3d v3;

  quaternion.getRotate(angle, v3);

  ASSERT_NEAR(v3.getX(), 0.0, error);
  ASSERT_NEAR(v3.getY(), 0.0, error);
  ASSERT_NEAR(v3.getZ(), 1.0, error);
  ASSERT_NEAR(angle, 0.0, error);

  quaternion = Quat<f64>(Vec3<f64>(0, 0, toRad(270.0)));

  ASSERT_NEAR(quaternion.getX(), 0.7071, error);
  ASSERT_NEAR(quaternion.getY(), 0, error);
  ASSERT_NEAR(quaternion.getZ(), 0, error);
  ASSERT_NEAR(quaternion.getW(), -0.7071, error);

  quaternion.getRotate(angle, v3);

  ASSERT_NEAR(v3.getX(), 1.0, error);
  ASSERT_NEAR(v3.getY(), 0.0, error);
  ASSERT_NEAR(v3.getZ(), 0.0, error);
  ASSERT_NEAR(angle, -1.5708, error);
}

/// @test
/// Recovers Euler angles that rebuild the same rotation at and near a right angle pitch, where
/// yaw and bank turn about one axis: the rebuilt quaternion stays within 1e-6 radians of the
/// original and is free of NaN.
/// @requirements(SEN-1060)
TEST(QuaternionTest, eulerRecoveryAtThePitchLimit)
{
  for (const double pitchDeg: {-90.0, -89.999, -89.0, 89.0, 89.999, 90.0})
  {
    for (int yawDeg = -180; yawDeg <= 180; yawDeg += 30)
    {
      for (int bankDeg = -180; bankDeg <= 180; bankDeg += 45)
      {
        const Quatd built {toRad(yawDeg), toRad(pitchDeg), toRad(bankDeg)};
        const auto recovered = built.getRotateInEulerYPB();

        ASSERT_FALSE(std::isnan(recovered.getX()) || std::isnan(recovered.getY()) || std::isnan(recovered.getZ()));

        const Quatd rebuilt {recovered.getX(), recovered.getY(), recovered.getZ()};

        // The angles may differ where the pair is degenerate. The rotation they denote may not.
        EXPECT_LT(2.0 * std::acos(std::min(1.0, std::abs(built.dot(rebuilt)))), 1e-6)
          << "yaw " << yawDeg << " pitch " << pitchDeg << " bank " << bankDeg;
      }
    }
  }
}
