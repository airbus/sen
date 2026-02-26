// === quat_test.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"

// testing file
#include "constants.h"
#include "quat.h"
#include "utils.h"
#include "vec3.h"

// sen
#include "sen/core/base/numbers.h"

// gtest
#include <gtest/gtest.h>

using namespace sen::util;  // NOLINT

/// @test
/// Check quaternion object initialization
/// @requirements(SEN-1060)
TEST(QuaternionTest, initialization)
{
  Quatd quaternion {};

  // expected the value
  EXPECT_EQ(quaternion.getX(), 0);
  EXPECT_EQ(quaternion.getY(), 0);
  EXPECT_EQ(quaternion.getZ(), 0);
  EXPECT_EQ(quaternion.getW(), 1);
}

/// @test
/// Check quaternion rotation function
/// @requirements(SEN-1060)
TEST(QuaternionTest, rotation)
{
  Quatd quaternion {};

  // expected the value
  EXPECT_EQ(quaternion.getX(), 0);
  EXPECT_EQ(quaternion.getY(), 0);
  EXPECT_EQ(quaternion.getZ(), 0);
  EXPECT_EQ(quaternion.getW(), 1);

  quaternion.makeRotate(pi, Vec3d {1.0f, 0.0f, 0.0f});

  ASSERT_NEAR(quaternion.getX(), 1, 0.005);
  ASSERT_NEAR(quaternion.getY(), 0, 0.005);
  ASSERT_NEAR(quaternion.getZ(), 0, 0.005);
  ASSERT_NEAR(quaternion.getW(), 0, 0.005);

  quaternion.makeRotate(pi / 2.0f, Vec3d {0.0f, 1.0f, 0.0f});

  ASSERT_NEAR(quaternion.getX(), 0.707, 0.005);
  ASSERT_NEAR(quaternion.getY(), 0, 0.005);
  ASSERT_NEAR(quaternion.getZ(), -0.707, 0.005);
  ASSERT_NEAR(quaternion.getW(), 0, 0.005);
}

/// @test
/// Check quaternion euler operation functions
/// @requirements(SEN-1060)
TEST(QuaternionTest, eulerOperations)
{
  Quat quaternion = Quat<f64>(0, 0, toRad(30.0));

  // expected the value
  ASSERT_NEAR(quaternion.getX(), 0.2588, 0.005);
  ASSERT_NEAR(quaternion.getY(), 0, 0.005);
  ASSERT_NEAR(quaternion.getZ(), 0, 0.005);
  ASSERT_NEAR(quaternion.getW(), 0.9659, 0.005);

  auto eulerAngles = quaternion.getRotateInEulerYPB();

  ASSERT_NEAR(toDeg(eulerAngles.getX()), 0, 0.005);
  ASSERT_NEAR(toDeg(eulerAngles.getY()), 0, 0.005);
  ASSERT_NEAR(toDeg(eulerAngles.getZ()), 30.0, 0.005);

  quaternion.makeRotate(toRad(60.0), 1, 0, 0);

  eulerAngles = quaternion.getRotateInEulerYPB();

  ASSERT_NEAR(toDeg(eulerAngles.getX()), 0, 0.005);
  ASSERT_NEAR(toDeg(eulerAngles.getY()), 0, 0.005);
  ASSERT_NEAR(toDeg(eulerAngles.getZ()), 90.0, 0.005);

  quaternion = Quat<f64>(0, toRad(30.0), 0);

  // expected the value
  ASSERT_NEAR(quaternion.getX(), 0, 0.005);
  ASSERT_NEAR(quaternion.getY(), 0.2588, 0.005);
  ASSERT_NEAR(quaternion.getZ(), 0, 0.005);
  ASSERT_NEAR(quaternion.getW(), 0.9659, 0.005);
}

/// @test
/// Check quaternion length function
/// @requirements(SEN-1060)
TEST(QuaternionTest, length)
{
  Quat quaternion = Quat<f64>(0, 0, toRad(15.0));

  // expected the value
  EXPECT_NEAR(quaternion.length(), 1.0, 0.00001);
  EXPECT_NEAR(quaternion.length2(), 1.0, 0.00001);

  quaternion = Quat<f64>(toRad(60.0), toRad(22.0), toRad(15.0));

  // expected the value
  EXPECT_NEAR(quaternion.length(), 1.0, 0.00001);
  EXPECT_NEAR(quaternion.length2(), 1.0, 0.00001);
}

/// @test
/// Check quaternion conjugate function
/// @requirements(SEN-1060)
TEST(QuaternionTest, conjugate)
{
  Quat quaternion = Quat<f64>(0, toRad(60.0), toRad(15.0));

  quaternion = quaternion.conj();

  // expected the value
  auto eulerAngles = quaternion.getRotateInEulerYPB();

  // expected the value
  ASSERT_NEAR(toDeg(eulerAngles.getX()), 24.146, 0.001);
  ASSERT_NEAR(toDeg(eulerAngles.getY()), -56.774, 0.001);
  ASSERT_NEAR(toDeg(eulerAngles.getZ()), -28.186, 0.001);
}

/// @test
/// Check quaternion inverse function
/// @requirements(SEN-1060)
TEST(QuaternionTest, inverse)
{
  Quat quaternion = Quat<f64>(0, toRad(60.0), toRad(15.0));

  quaternion = quaternion.inverse();

  // expected the value
  auto eulerAngles = quaternion.getRotateInEulerYPB();

  // expected the value
  ASSERT_NEAR(toDeg(eulerAngles.getX()), 24.146, 0.001);
  ASSERT_NEAR(toDeg(eulerAngles.getY()), -56.774, 0.001);
  ASSERT_NEAR(toDeg(eulerAngles.getZ()), -28.186, 0.001);
}

/// @test
/// Check quaternion slerp function
/// @requirements(SEN-1060)
TEST(QuaternionTest, slerp)
{
  Quat from = Quat<f64>(0, 0, toRad(15.0));
  Quat to = Quat<f64>(0, 0, toRad(55.0));

  Quat result = Quat<f64>(0, 0, 0);

  result.slerp(0.1, from, to);

  auto eulerAngles = result.getRotateInEulerYPB();

  // expected the value
  ASSERT_NEAR(toDeg(eulerAngles.getX()), 0, 0.001);
  ASSERT_NEAR(toDeg(eulerAngles.getY()), 0, 0.001);
  ASSERT_NEAR(toDeg(eulerAngles.getZ()), 19, 0.001);

  result.slerp(0.2, from, to);

  eulerAngles = result.getRotateInEulerYPB();

  // expected the value
  ASSERT_NEAR(toDeg(eulerAngles.getX()), 0, 0.001);
  ASSERT_NEAR(toDeg(eulerAngles.getY()), 0, 0.001);
  ASSERT_NEAR(toDeg(eulerAngles.getZ()), 23, 0.001);
}
