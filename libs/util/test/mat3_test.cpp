// === mat3_test.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// implementation
#include "mat3.h"
#include "vec3.h"

// gtest
#include <gtest/gtest.h>

// sen
#include "sen/core/base/numbers.h"

namespace sen::util
{

/// @test
/// Check creation of a matrix
/// @requirements(SEN-1059)
TEST(Matrix, Creation)
{
  Mat3f mat {};
  mat(1, 2) = 4.66;
  EXPECT_FLOAT_EQ(4.66, mat(1, 2));
  EXPECT_FLOAT_EQ(0, mat(0, 0));
}

/// @test
/// Check initialization of a matrix
/// @requirements(SEN-1059)
TEST(Matrix, InitializerList)
{
  Mat3f mat {{3, 5, 4}, {2, 2, 2}, {1, 3, 4}};
  EXPECT_FLOAT_EQ(3, mat(0, 0));
  EXPECT_FLOAT_EQ(5, mat(0, 1));
  EXPECT_FLOAT_EQ(4, mat(0, 2));
  EXPECT_FLOAT_EQ(2, mat(1, 0));
  EXPECT_FLOAT_EQ(2, mat(1, 1));
  EXPECT_FLOAT_EQ(2, mat(1, 2));
  EXPECT_FLOAT_EQ(1, mat(2, 0));
  EXPECT_FLOAT_EQ(3, mat(2, 1));
  EXPECT_FLOAT_EQ(4, mat(2, 2));
}

/// @test
/// Check the copy of a matrix
/// @requirements(SEN-1059)
TEST(Matrix, copyMatrix)
{
  Mat3f mat1 {{3, 5, 4}, {2, 2, 2}, {1, 3, 4}};

  auto mat2 = mat1;

  EXPECT_FLOAT_EQ(2, mat2(1, 1));
}

/// @test
/// Check the correct creation of an identity matrix
/// @requirements(SEN-1059)
TEST(Matrix, identity)
{
  auto mat = Mat3f::makeIdentity();
  Mat3f expectation {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

  for (u32 i = 0; i < 3U; ++i)
  {
    for (u32 j = 0; j < 3U; ++j)
    {
      EXPECT_EQ(expectation(i, j), mat(i, j));
    }
  }
}

/// @test
/// Check transpose function for a matrix
/// @requirements(SEN-1059)
TEST(Matrix, transpose)
{
  Mat3f mat {{2, 3, 4}, {4, 5, 6}, {88, 6, 6}};
  Mat3f expectation {{2, 4, 88}, {3, 5, 6}, {4, 6, 6}};

  Mat3f result {};
  result.transpose(mat);

  for (u32 i = 0; i < 3U; ++i)
  {
    for (u32 j = 0; j < 3U; ++j)
    {
      EXPECT_EQ(expectation(i, j), result(i, j));
    }
  }

  result.transpose(result);

  for (u32 i = 0; i < 3U; ++i)
  {
    for (u32 j = 0; j < 3U; ++j)
    {
      EXPECT_EQ(mat(i, j), result(i, j));
    }
  }
}

/// @test
/// Check multiplication function for two given matrices storing the result in the current instance
/// @requirements(SEN-1059)
TEST(Matrix, mult)
{
  Mat3f result {};
  Mat3f lhs {{1, 2, 3}, {3, 2, 1}, {1, 2, 3}};
  Mat3f rhs {{4, 5, 6}, {6, 5, 4}, {4, 6, 5}};

  Mat3f expectation {{28, 33, 29}, {28, 31, 31}, {28, 33, 29}};

  result.mult(lhs, rhs);

  for (u32 i = 0; i < 3U; ++i)
  {
    for (u32 j = 0; j < 3U; ++j)
    {
      EXPECT_EQ(expectation(i, j), result(i, j));
    }
  }
}

/// @test
/// Check pre-multiplication function for a given matrix to the current instance
/// @requirements(SEN-1059)
TEST(Matrix, preMult)
{
  Mat3f result {{4, 5, 6}, {6, 5, 4}, {4, 6, 5}};
  Mat3f other {{1, 2, 3}, {3, 2, 1}, {1, 2, 3}};

  Mat3f expectation {{28, 33, 29}, {28, 31, 31}, {28, 33, 29}};

  result.preMult(other);

  for (u32 i = 0; i < 3U; ++i)
  {
    for (u32 j = 0; j < 3U; ++j)
    {
      EXPECT_EQ(expectation(i, j), result(i, j));
    }
  }
}

/// @test
/// Check post-multiplication function for a given matrix to the current instance
/// @requirements(SEN-1059)
TEST(Matrix, postMult)
{
  Mat3f result {{1, 2, 3}, {3, 2, 1}, {1, 2, 3}};
  Mat3f other {{4, 5, 6}, {6, 5, 4}, {4, 6, 5}};

  Mat3f expectation {{28, 33, 29}, {28, 31, 31}, {28, 33, 29}};

  result.postMult(other);

  for (u32 i = 0; i < 3U; ++i)
  {
    for (u32 j = 0; j < 3U; ++j)
    {
      EXPECT_EQ(expectation(i, j), result(i, j));
    }
  }
}

/// @test
/// Check post-multiplication function for a given matrix and store the result in a vector
/// @requirements(SEN-1059)
TEST(Matrix, postMultVec)
{
  Mat3f mat {{1, 2, 3}, {3, 2, 1}, {1, 2, 3}};
  Vec3f other {4.6, 55, 6.7};

  Vec3f expectation {134.7, 130.5, 134.7};

  auto result = mat.postMult(other);

  EXPECT_EQ(expectation, result);
}

/// @test
/// Check pre-multiplication function for a given matrix and store the result in a vector
/// @requirements(SEN-1059)
TEST(Matrix, preMultVec)
{
  Mat3f mat {{1, 2, 3}, {3, 2, 1}, {1, 2, 3}};
  Vec3f other {4.6, 55, 6.7};

  Vec3f expectation {176.3, 132.6, 88.9};

  auto result = mat.preMult(other);

  EXPECT_FLOAT_EQ(result.getX(), expectation.getX());
  EXPECT_FLOAT_EQ(result.getY(), expectation.getY());
  EXPECT_FLOAT_EQ(result.getZ(), expectation.getZ());
}

}  // namespace sen::util
