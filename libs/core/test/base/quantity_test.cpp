// === quantity_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/base/quantity.h"

// google test
#include <gtest/gtest.h>

SEN_RANGED_QUANTITY(MyQuantity, float32_t, -15.0f, 15.0f)

SEN_RANGED_QUANTITY(MySmallQuantity, float64_t, 0.0055f, 0.0056f);

SEN_RANGED_QUANTITY(MyNegativeSmallQuantity, float32_t, -20.001f, -20.00001f);

SEN_RANGED_QUANTITY(MyMatchingBoundsQuantity, float32_t, -11.15f, -11.15f);

SEN_RANGED_QUANTITY(MyCharsQuantity, char, 'A', 'Z');

/// @test
/// Check quantity default constructor
/// @requirements(SEN-355)
TEST(RangeChecked, defaultConstuction)
{
  MyQuantity value;
  EXPECT_EQ(0.0f, value);
}

/// @test
/// Check a valid value for a specific quantity range
/// @requirements(SEN-355)
TEST(RangeChecked, validValue)
{
  MyQuantity value;
  EXPECT_NO_THROW(value = 2.0f);
}

/// @test
/// Check invalid values for a specific quantity range
/// @requirements(SEN-355)
TEST(RangeChecked, invalidValue)
{
  MyQuantity value;
  EXPECT_ANY_THROW(value = 20.0f);
  EXPECT_ANY_THROW(value = 15.1f);
  EXPECT_ANY_THROW(value = -15.1f);
}

/// @test
/// Check basic comparisons (==, =!, <, <=, >, >=) between quantity and literals
/// @requirements(SEN-355)
TEST(RangeChecked, comparison)
{
  MyQuantity value = 4.0f;
  EXPECT_EQ(4.0f, value);
  EXPECT_NE(4.1f, value);
  EXPECT_GT(5.0f, value);
  EXPECT_LT(3.0f, value);
  EXPECT_GE(5.0f, value);
  EXPECT_GE(4.0f, value);
  EXPECT_ANY_THROW(value = 15.1f);
  EXPECT_ANY_THROW(value = -15.1f);
}

/// @test
/// Check conversion of a quantity type (to smaller memory types)
/// @requirements(SEN-355)
TEST(RangeChecked, conversion)
{
  MyQuantity value = 4.0f;

  float64_t x = value;
  int32_t y = value;  // NOLINT as we want to test the conversion
  float32_t z = value;

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

  EXPECT_FLOAT_EQ(0.0055f, value);
  EXPECT_NO_THROW(value = 0.0055f);
  EXPECT_NO_THROW(value = 0.0055556f);
  EXPECT_NO_THROW(value = 0.00560000f);
  EXPECT_ANY_THROW(value = 0.00560001f);
}

/// @test
/// Check conversion between floating types
/// @requirements(SEN-355)
TEST(RangeChecked, floatConversion)
{
  MySmallQuantity value = 0.005511111111111f;

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
  EXPECT_ANY_THROW(value = -20.0011f);
  EXPECT_NO_THROW(value = -20.001f);
  EXPECT_NO_THROW(value = -20.0001f);
  EXPECT_NO_THROW(value = -20.00001f);
  EXPECT_ANY_THROW(value = -20.000001f);
}

/// @test
/// Check quantities with matching bounds
/// @requirements(SEN-355)
TEST(RangeChecked, matchingBounds)
{
  MyMatchingBoundsQuantity value = -11.15f;

  EXPECT_NO_THROW(std::ignore = (value == -11.15f));

  float64_t x = value;

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
  EXPECT_NO_THROW(value = 'N');
  EXPECT_NO_THROW(value = 'Z');
  EXPECT_ANY_THROW(value = 'a');
  EXPECT_ANY_THROW(value = 'd');
  EXPECT_ANY_THROW(value = 'z');
  EXPECT_ANY_THROW(value = '3');
  EXPECT_ANY_THROW(value = '_');
  EXPECT_ANY_THROW(value = '\0');
  EXPECT_ANY_THROW(value = '\n');
}

/// @test
/// Check conversion from quantity char type
/// @requirements(SEN-355)
TEST(RangeChecked, charConversion)
{
  MyCharsQuantity value = 'E';

  int32_t x = value;  // NOLINT as we want to test the conversion
  float32_t y = value;
  char z = y;  // NOLINT

  EXPECT_EQ(x, 69);
  EXPECT_EQ(y, 69.0);
  EXPECT_EQ(z, 'E');
}
