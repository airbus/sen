// === bits_test.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/bits.h"

// google test
#include <gtest/gtest.h>

// std
#include <bitset>
#include <cstdint>

using sen::std_util::bit_cast;

/// @test
/// bit_cast to the same signed 32-bit type returns the value unchanged.
/// @requirements(SEN-1045)
TEST(BitCast, CastSameTypeSigned)
{
  // val = 10101010101010101010101010101010
  int32_t val = -1431655766;

  int32_t res = bit_cast<int32_t>(val);

  EXPECT_EQ(val, res);
}

/// @test
/// bit_cast to the same unsigned 32-bit type returns the value unchanged.
/// @requirements(SEN-1045)
TEST(BitCast, CastSameTypeUnsigned)
{
  // val = 01010101010101010101010101010101
  uint32_t val = 1431655765;

  uint32_t res = bit_cast<uint32_t>(val);

  EXPECT_EQ(val, res);
}

/// @test
/// bit_cast from a negative int32_t to uint32_t keeps the bit pattern, so -1431655766 becomes 2863311530.
/// @requirements(SEN-1045)
TEST(BitCast, CastInterchangeTypeSigned)
{
  // val = 10101010101010101010101010101010
  int32_t val = -1431655766;
  uint32_t expected = 2863311530;

  uint32_t res = bit_cast<uint32_t>(val);

  EXPECT_EQ(expected, res);
}

/// @test
/// bit_cast from uint32_t to int32_t keeps the bit pattern, so 1431655765 stays 1431655765.
/// @requirements(SEN-1045)
TEST(BitCast, CastInterchangeTypeUnsigned)
{
  // val = 01010101010101010101010101010101
  uint32_t val = 1431655765;
  int32_t expected = 1431655765;

  int32_t res = bit_cast<int32_t>(val);

  EXPECT_EQ(expected, res);
}

/// @test
/// bit_cast reads a struct of two uint32_t as one uint64_t, the first member in the low half and
/// the second in the high half.
/// @requirements(SEN-1045)
TEST(BitCast, CastCombineValues)
{
  struct TwoInts
  {
    uint32_t a;
    uint32_t b;
  };
  TwoInts v;
  // a = 01010101010101010101010101010101
  v.a = 1431655765;
  // b = 00000000000000000000000000000001
  v.b = 1;

  // expected = 0000000000000000000000000000000101010101010101010101010101010101
  uint64_t expected = 5726623061;

  uint64_t res = bit_cast<uint64_t>(v);

  EXPECT_EQ(expected, res);
}

/// @test
/// bit_cast of a bitset's to_ullong value yields the matching integer in both uint64_t and
/// int64_t form: 1, 3, 7 and 15 as bits zero to three are set in turn.
/// @requirements(SEN-1045)
TEST(BitCast, FromBitSet)
{
  std::bitset<64> b;

  b.set(0, true);
  EXPECT_EQ(1U, bit_cast<uint64_t>(b.to_ullong()));
  EXPECT_EQ(1, bit_cast<int64_t>(b.to_ullong()));

  b.set(1, true);
  EXPECT_EQ(3U, bit_cast<uint64_t>(b.to_ullong()));
  EXPECT_EQ(3, bit_cast<int64_t>(b.to_ullong()));

  b.set(2, true);
  EXPECT_EQ(7U, bit_cast<uint64_t>(b.to_ullong()));
  EXPECT_EQ(7, bit_cast<int64_t>(b.to_ullong()));

  b.set(3, true);
  EXPECT_EQ(15U, bit_cast<uint64_t>(b.to_ullong()));
  EXPECT_EQ(15, bit_cast<int64_t>(b.to_ullong()));
}
