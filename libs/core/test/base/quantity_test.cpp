// === quantity_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/base/quantity.h"
#include "sen/util/dr/algorithms.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>

SEN_RANGED_QUANTITY(MinAboveZero, float, 10.0f, 20.0f)

SEN_RANGED_QUANTITY(MaxBelowZero, float, -20.0f, -10.0f)

SEN_RANGED_QUANTITY(MyQuantity, float32_t, -15.0f, 15.0f)

SEN_RANGED_QUANTITY(MySmallQuantity, float64_t, 0.0055f, 0.0056f)

SEN_RANGED_QUANTITY(MyNegativeSmallQuantity, float32_t, -20.001f, -20.00001f)

SEN_RANGED_QUANTITY(MyMatchingBoundsQuantity, float32_t, -11.15f, -11.15f)

SEN_RANGED_QUANTITY(MyCharsQuantity, char, 'A', 'Z')

SEN_NON_RANGED_QUANTITY(MyNonRangedQuantity, float32_t)

/// @test
/// Check quantity default constructor
/// @requirements(SEN-355)
TEST(RangeChecked, defaultConstuction)
{
  constexpr MyQuantity value;
  EXPECT_EQ(0.0f, value.get());
}

/// @test
/// Check quantity default constructor when zero is out of bounds
/// @requirements(SEN-355)
TEST(RangeChecked, defaultConstuctionOutOfBounds)
{
  const MinAboveZero valueMin;
  EXPECT_EQ(10.0f, valueMin.get());

  const MaxBelowZero valueMax;
  EXPECT_EQ(-20.0f, valueMax.get());
}

/// @test
/// Check a valid value for a specific quantity range
/// @requirements(SEN-355)
TEST(RangeChecked, validValue)
{
  MyQuantity value;
  EXPECT_NO_THROW(value = 2.0f);
  EXPECT_EQ(2.0f, value.get());
}

/// @test
/// Check invalid values for a specific quantity range and verify strong exception guarantee
/// @requirements(SEN-355)
TEST(RangeChecked, invalidValue)
{
  MyQuantity value;

  constexpr float32_t tooHigh = 20.0f;
  constexpr float32_t tooLow = -20.0f;

  EXPECT_ANY_THROW(value = tooHigh);
  EXPECT_EQ(0.0f, value.get());

  EXPECT_ANY_THROW(value.set(tooHigh));
  EXPECT_EQ(0.0f, value.get());

  EXPECT_ANY_THROW(value = tooLow);
  EXPECT_EQ(0.0f, value.get());
}

/// @test
/// Check basic comparisons (==, !=, <, <=, >, >=) between quantity and literals
/// @requirements(SEN-355)
TEST(RangeChecked, comparison)
{
  const MyQuantity value = 4.0f;

  EXPECT_EQ(4.0f, value);
  EXPECT_NE(4.1f, value);
  EXPECT_GT(5.0f, value);
  EXPECT_LT(3.0f, value);
  EXPECT_GE(5.0f, value);
  EXPECT_GE(4.0f, value);

  EXPECT_TRUE(value == 4.0f);
  EXPECT_TRUE(value != 4.1f);
  EXPECT_TRUE(value < 5.0f);
  EXPECT_TRUE(value <= 5.0f);
  EXPECT_TRUE(value <= 4.0f);
  EXPECT_TRUE(value > 3.0f);
  EXPECT_TRUE(value >= 3.0f);
  EXPECT_TRUE(value >= 4.0f);
}

/// @test
/// Check getters and setters
/// @requirements(SEN-355)
TEST(RangeChecked, gettersAndSetters)
{
  MyQuantity value;

  value.set(5.0f);
  EXPECT_EQ(5.0f, value.get());

  value = 10.0f;
  EXPECT_EQ(10.0f, value.get());
}

/// @test
/// Check validity flags and contextual boolean conversion
/// @requirements(SEN-355)
TEST(RangeChecked, validity)
{
  MyQuantity value;
  EXPECT_TRUE(value.isValid());
  EXPECT_TRUE(value);

  value.setValid(false);
  EXPECT_FALSE(value.isValid());
  EXPECT_FALSE(value);
}

/// @test
/// Check conversion of a quantity type to smaller memory types
/// @requirements(SEN-355)
TEST(RangeChecked, conversion)
{
  const MyQuantity value = 4.0f;

  const float64_t x = value;
  int32_t y = value;  // NOLINT as we want to test the conversion
  const float32_t z = value;

  EXPECT_EQ(x, 4.0);
  EXPECT_EQ(y, 4);
  EXPECT_EQ(z, 4.0f);
}

/// @test
/// Check quantities with small bounds (floating precision check)
/// @requirements(SEN-355)
TEST(RangeChecked, SmallBounds)
{
  MySmallQuantity value = 0.0055f;

  EXPECT_FLOAT_EQ(0.0055f, value.get());
  EXPECT_NO_THROW(value = 0.0055556f);
  EXPECT_FLOAT_EQ(0.0055556f, value.get());
  EXPECT_NO_THROW(value = 0.00560000f);
  EXPECT_FLOAT_EQ(0.00560000f, value.get());

  constexpr float64_t tooHigh = 0.00560001f;
  EXPECT_ANY_THROW(value = tooHigh);
  EXPECT_FLOAT_EQ(0.00560000f, value.get());
}

/// @test
/// Check conversion between floating types
/// @requirements(SEN-355)
TEST(RangeChecked, floatConversion)
{
  const MySmallQuantity value = 0.005511111111111f;

  float32_t x = value;  // NOLINT as we want to test the conversion

  EXPECT_FLOAT_EQ(x, 0.005511111111111f);
}

/// @test
/// Check quantities with negative bounds
/// @requirements(SEN-355)
TEST(RangeChecked, NegativeBoundaries)
{
  MyNegativeSmallQuantity value;

  EXPECT_ANY_THROW(value = -20.1f);
  EXPECT_NO_THROW(value = -20.001f);
  EXPECT_EQ(-20.001f, value.get());
  EXPECT_ANY_THROW(value = -20.000001f);
}

/// @test
/// Check quantities with matching bounds
/// @requirements(SEN-355)
TEST(RangeChecked, matchingBounds)
{
  MyMatchingBoundsQuantity value = -11.15f;

  EXPECT_TRUE(value == -11.15f);

  const float64_t x = value;

  EXPECT_EQ(x, -11.15f);
  EXPECT_NO_THROW(value = x);
}

/// @test
/// Check char-type quantity
/// @requirements(SEN-355)
TEST(RangeChecked, charValues)
{
  MyCharsQuantity value;

  EXPECT_NO_THROW(value = 'A');
  EXPECT_EQ('A', value.get());

  EXPECT_NO_THROW(value = 'N');
  EXPECT_NO_THROW(value = 'Z');

  EXPECT_ANY_THROW(value = 'a');
  EXPECT_ANY_THROW(value = 'd');
  EXPECT_ANY_THROW(value = '3');
  EXPECT_ANY_THROW(value = 'z');
  EXPECT_ANY_THROW(value = '_');
  EXPECT_ANY_THROW(value = '\0');
  EXPECT_ANY_THROW(value = '\n');
}

/// @test
/// Check conversion from quantity char type
/// @requirements(SEN-355)
TEST(RangeChecked, charConversion)
{
  const MyCharsQuantity value = 'E';

  int32_t x = value;  // NOLINT as we want to test the conversion
  const float32_t y = value;
  char z = y;  // NOLINT

  EXPECT_EQ(x, 69);
  EXPECT_EQ(y, 69.0f);
  EXPECT_EQ(z, 'E');
}

/// @test
/// Check non ranged quantity behavior
/// @requirements(SEN-355)
TEST(NonRangeChecked, basicOperations)
{
  MyNonRangedQuantity value;
  EXPECT_EQ(0.0f, value.get());

  EXPECT_NO_THROW(value = 1000.0f);
  EXPECT_EQ(1000.0f, value.get());

  EXPECT_NO_THROW(value.set(-1000.0f));
  EXPECT_EQ(-1000.0f, value.get());
}

/// @test
/// Check specific quantities
/// @requirements(SEN-355)
TEST(UtilQuantities, LibraryTypesCoverage)
{
  sen::util::LatitudeDegrees lat {45.0};
  EXPECT_EQ(45.0, lat.get());
  EXPECT_ANY_THROW(lat.set(91.0));
  EXPECT_ANY_THROW(lat.set(-91.0));

  sen::util::LongitudeDegrees lon {120.0};
  EXPECT_EQ(120.0, lon.get());
  EXPECT_ANY_THROW(lon.set(181.0));
  EXPECT_ANY_THROW(lon.set(-181.0));

  sen::util::VelocityMetersPerSecond vel {100.0};
  EXPECT_EQ(100.0f, vel.get());

  sen::util::AccelerationMetersPerSecondSquared acc {9.8};
  EXPECT_EQ(9.8f, acc.get());

  sen::util::AngularVelocityRadiansPerSecond angVel {1.5};
  EXPECT_EQ(1.5f, angVel.get());

  sen::util::AngularAccelerationRadiansPerSecondSquared angAcc {0.5};
  EXPECT_EQ(0.5f, angAcc.get());

  sen::util::AngleRadians angle {90.0};
  EXPECT_EQ(90.0f, angle.get());

  sen::util::LengthMeters len {1000.0};
  EXPECT_EQ(1000.0, len.get());
}
