// === vec3_test.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// implementation
#include "vec3.h"

// sen
#include "sen/core/base/numbers.h"

// gtest
#include <gtest/gtest.h>

namespace sen::util
{

constexpr f64 error = 1e-4;

/// @test
/// Tests Vec3 initialization with values and without them
TEST(Vec3, initialization)
{
  const Vec3 defaultVec3 = Vec3<f32> {};

  // expected the value
  EXPECT_EQ(defaultVec3.getX(), 0);
  EXPECT_EQ(defaultVec3.getY(), 0);
  EXPECT_EQ(defaultVec3.getZ(), 0);

  const Vec3 valueVec3 = Vec3<f64> {35.5, 68.21, 59.55};

  // expected the value
  EXPECT_EQ(valueVec3.getX(), 35.5);
  EXPECT_EQ(valueVec3.getY(), 68.21);
  EXPECT_EQ(valueVec3.getZ(), 59.55);
}

/// @test
/// Tests Vec3 pointer method
TEST(Vec3, pointer)
{
  Vec3 vec3NoConst = Vec3<f64> {35.65, 62.59, 20.79};
  const Vec3 vec3Const = Vec3<f64> {73.57, 30.25, 38.95};

  double* vec3NoConstPtr = vec3NoConst.ptr();

  EXPECT_EQ(vec3NoConstPtr[0], 35.65);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  EXPECT_EQ(vec3NoConstPtr[1], 62.59);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  EXPECT_EQ(vec3NoConstPtr[2], 20.79);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  vec3NoConstPtr[0] = 0;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  EXPECT_EQ(vec3NoConst.getX(), 0);
  EXPECT_EQ(vec3NoConst.getY(), 62.59);
  EXPECT_EQ(vec3NoConst.getZ(), 20.79);

  const double* vec3ConstPtr = vec3Const.ptr();

  EXPECT_EQ(vec3ConstPtr[0], 73.57);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  EXPECT_EQ(vec3ConstPtr[1], 30.25);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  EXPECT_EQ(vec3ConstPtr[2], 38.95);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

/// @test
/// Tests Vec3 set method
TEST(Vec3, set)
{
  Vec3 vec3 = Vec3<f64> {};

  vec3.set(45.66, 25.44, 82.78);

  // expected the value
  EXPECT_EQ(vec3.getX(), 45.66);
  EXPECT_EQ(vec3.getY(), 25.44);
  EXPECT_EQ(vec3.getZ(), 82.78);

  vec3.setX(65.31);

  EXPECT_EQ(vec3.getX(), 65.31);

  vec3.setY(56.66);

  EXPECT_EQ(vec3.getY(), 56.66);

  vec3.setZ(85.24);

  EXPECT_EQ(vec3.getZ(), 85.24);

  Vec3d setVec3 {};

  setVec3.set(vec3);

  // expected the value
  EXPECT_EQ(setVec3.getX(), 65.31);
  EXPECT_EQ(setVec3.getY(), 56.66);
  EXPECT_EQ(setVec3.getZ(), 85.24);
}

/// @test
/// Tests Vec3 equal to and not equal to operators
TEST(Vec3, equality)
{
  Vec3d vec3L {19.7, 3.89, 27.15};
  Vec3d vec3R {28.28, 10.62, 63.08};

  EXPECT_TRUE(vec3L == vec3L);
  EXPECT_FALSE(vec3L != vec3L);

  EXPECT_FALSE(vec3L == vec3R);
  EXPECT_TRUE(vec3L != vec3R);
}

/// @test
/// Tests Vec3 less than operator
TEST(Vec3, comparison)
{
  Vec3 vec3 = Vec3<f64> {43.34, 8.02, 58.46};
  Vec3 vec3First = Vec3<f64> {72.49, 88.26, 30.06};
  Vec3 vec3Second = Vec3<f64> {23.42, 13.34, 7.96};
  Vec3 vec3Third = Vec3<f64> {43.34, 37.62, 85.58};
  Vec3 vec3Forth = Vec3<f64> {43.34, 2.8, 27.3};
  Vec3 vec3Fifth = Vec3<f64> {43.34, 8.02, 76.74};

  EXPECT_TRUE(vec3 < vec3First);
  EXPECT_FALSE(vec3 < vec3Second);
  EXPECT_TRUE(vec3 < vec3Third);
  EXPECT_FALSE(vec3 < vec3Forth);
  EXPECT_TRUE(vec3 < vec3Fifth);
}

/// @test
/// Tests Vec3 dot product and scalar multiplication methods
TEST(Vec3, multiplication)
{
  Vec3d vec3L {2, 8, 3};
  Vec3d vec3R {9, 1, 5};

  EXPECT_EQ(vec3L * vec3R, 41);

  Vec3d crossMultiply = vec3L ^ vec3R;

  EXPECT_EQ(crossMultiply.getX(), 37);
  EXPECT_EQ(crossMultiply.getY(), 17);
  EXPECT_EQ(crossMultiply.getZ(), -70);

  Vec3d dotProduct = vec3L * 2;

  EXPECT_EQ(dotProduct.getX(), 2 * 2);
  EXPECT_EQ(dotProduct.getY(), 8 * 2);
  EXPECT_EQ(dotProduct.getZ(), 3 * 2);

  vec3R *= 3;

  EXPECT_EQ(vec3R.getX(), 9 * 3);
  EXPECT_EQ(vec3R.getY(), 1 * 3);
  EXPECT_EQ(vec3R.getZ(), 5 * 3);
}

/// @test
/// Tests Vec3 division methods
TEST(Vec3, division)
{
  Vec3d vec3L {16, 24, 12};

  Vec3d vec3R = vec3L / 4;

  EXPECT_EQ(vec3R.getX(), 16 / 4);
  EXPECT_EQ(vec3R.getY(), 24 / 4);
  EXPECT_EQ(vec3R.getZ(), 12 / 4);

  vec3L /= 2;

  EXPECT_EQ(vec3L.getX(), 16 / 2);
  EXPECT_EQ(vec3L.getY(), 24 / 2);
  EXPECT_EQ(vec3L.getZ(), 12 / 2);
}

/// @test
/// Tests Vec3 addition methods
TEST(Vec3, addition)
{
  Vec3 vec3 = Vec3<f64> {63.16, 6.05, 37} + Vec3<f64> {16.31, 49.9, 54.24};

  EXPECT_NEAR(vec3.getX(), 63.16 + 16.31, error);
  EXPECT_NEAR(vec3.getY(), 6.05 + 49.9, error);
  EXPECT_NEAR(vec3.getZ(), 37 + 54.24, error);

  vec3 += Vec3<f64> {19.8, 3.24, 15.16};

  EXPECT_NEAR(vec3.getX(), 79.47 + 19.8, error);
  EXPECT_NEAR(vec3.getY(), 55.95 + 3.24, error);
  EXPECT_NEAR(vec3.getZ(), 91.24 + 15.16, error);
}

/// @test
/// Tests Vec3 subtraction methods
TEST(Vec3, subtraction)
{
  Vec3 vec3 = Vec3<f64> {28.05, 36.79, 5.7} - Vec3<f64> {18.36, 53.78, 86.77};

  EXPECT_NEAR(vec3.getX(), 28.05 - 18.36, error);
  EXPECT_NEAR(vec3.getY(), 36.79 - 53.78, error);
  EXPECT_NEAR(vec3.getZ(), 5.7 - 86.77, error);

  vec3 -= Vec3<f64> {6.36, 30.61, 11.18};

  EXPECT_NEAR(vec3.getX(), 9.69 - 6.36, error);
  EXPECT_NEAR(vec3.getY(), -16.99 - 30.61, error);
  EXPECT_NEAR(vec3.getZ(), -81.07 - 11.18, error);

  vec3 = -vec3;

  EXPECT_NEAR(vec3.getX(), -3.33, error);
  EXPECT_NEAR(vec3.getY(), 47.6, error);
  EXPECT_NEAR(vec3.getZ(), 92.25, error);
}

/// @test
/// Tests Vec3 length methods
TEST(Vec3, length)
{
  Vec3d vec3 {6, 2, 3};

  EXPECT_EQ(vec3.length(), 7);
  EXPECT_EQ(vec3.length2(), 49);
}

/// @test
/// Tests Vec3 normalization method
TEST(Vec3, normalization)
{
  Vec3d vec3 {4, 0, 3};

  EXPECT_DOUBLE_EQ(vec3.normalize(), 5);

  EXPECT_DOUBLE_EQ(vec3.getX(), 0.8);
  EXPECT_DOUBLE_EQ(vec3.getY(), 0.0);
  EXPECT_DOUBLE_EQ(vec3.getZ(), 0.6);
}

}  // namespace sen::util
