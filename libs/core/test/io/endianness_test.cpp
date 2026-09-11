// === endianness_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/bits.h"
#include "sen/core/base/numbers.h"
#include "sen/core/io/detail/endianness.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <iterator>
#include <limits>

namespace
{

bool isHostLittleEndianSlow()
{
  uint32_t test = 1U;
  auto* ptest = reinterpret_cast<unsigned char*>(&test);  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

  if ((1U == *ptest) && (0U == *(std::next(ptest, 3))))
  {
    return true;
  }

  if ((0U == *ptest) && (1 == *(std::next(ptest, 3))))
  {
    return false;
  }

  sen::throwRuntimeError("unknown endianness");
}

}  // namespace

/// @test
/// Check little endian detection in host
/// @requirements(SEN-893)
TEST(Endianness, detection)
{
  EXPECT_EQ(isHostLittleEndianSlow(), sen::impl::hostIsLittleEndian());
  EXPECT_EQ(!sen::impl::hostIsLittleEndian(), sen::impl::hostIsBigEndian());
}

/// @test
/// Check bytes swaping in endianess (8bits)
/// @requirements(SEN-893)
TEST(Endianness, swap8)
{
  {
    uint8_t val = 4;
    auto result = sen::impl::swapBytes(val);
    EXPECT_EQ(sizeof(result), sizeof(val));
    EXPECT_EQ(result, val);
  }

  {
    int8_t val = -4;
    auto result = sen::impl::swapBytes(val);
    EXPECT_EQ(sizeof(result), sizeof(val));
    EXPECT_EQ(result, val);
  }
}

/// @test
/// Check bytes swapping in endiannes (16bits)
/// @requirements(SEN-893)
TEST(Endianness, swap16)
{
  EXPECT_EQ(0x2211, sen::impl::swapBytes(int16_t {0x1122}));
  EXPECT_EQ(0x3322, sen::impl::swapBytes(int16_t {0x2233}));
  EXPECT_EQ(0x1032, sen::impl::swapBytes(int16_t {0x3210}));

  EXPECT_EQ(0x2211, sen ::impl::swapBytes(uint16_t {0x1122}));
  EXPECT_EQ(0xffff, sen ::impl::swapBytes(uint16_t {0xffff}));
  EXPECT_EQ(0x0123, sen ::impl::swapBytes(uint16_t {0x2301}));
}

/// @test
/// Check bytes swapping in endiannes (32bits)
/// @requirements(SEN-893)
TEST(Endianness, swap32)
{
  EXPECT_EQ(0x00000000, sen::impl::swapBytes(int32_t {0x00000000}));
  EXPECT_EQ(0x44332211, sen::impl::swapBytes(int32_t {0x11223344}));
  EXPECT_EQ(0x33221100, sen::impl::swapBytes(int32_t {0x00112233}));
  EXPECT_EQ(0x10324568, sen::impl::swapBytes(int32_t {0x68453210}));

  EXPECT_EQ(0x44332211, sen ::impl::swapBytes(uint32_t {0x11223344}));
  EXPECT_EQ(0x33221100, sen ::impl::swapBytes(uint32_t {0x00112233}));
  EXPECT_EQ(0x10324568, sen ::impl::swapBytes(uint32_t {0x68453210}));
  EXPECT_EQ(0x00000001, sen ::impl::swapBytes(uint32_t {0x01000000}));
}

/// @test
/// Check bytes swapping in endiannes (64bits)
/// @requirements(SEN-893)
TEST(Endianness, swap64)
{
  EXPECT_EQ(0x8877665544332211, sen::impl::swapBytes(int64_t {0x1122334455667788}));
  EXPECT_EQ(0x7766554433221100, sen::impl::swapBytes(int64_t {0x0011223344556677}));
  EXPECT_EQ(0x10324568ffcdaa1f, sen::impl::swapBytes(int64_t {0x1faacdff68453210}));

  EXPECT_EQ(0x8877665544332211, sen ::impl::swapBytes(uint64_t {0x1122334455667788}));
  EXPECT_EQ(0x7766554433221100, sen ::impl::swapBytes(uint64_t {0x0011223344556677}));
  EXPECT_EQ(0x10324568ffcdaa1f, sen ::impl::swapBytes(uint64_t {0x1faacdff68453210}));
  EXPECT_EQ(0xffffffffffffffff, sen ::impl::swapBytes(uint64_t {0xffffffffffffffff}));
}

/// @test
/// Check bytes swapping in floating types
/// @requirements(SEN-893)
TEST(Endianness, swapFloats)
{
  // This asserted that swapping a float returned it unchanged, which is what the two overloads
  // did and is why the stream was little-endian for integers only. IEEE 754 fixes the bit
  // pattern, not the order its bytes sit in, so a float swaps like the integer of its width.
  {
    const auto swapped = sen::impl::swapBytes(3.14159F);
    EXPECT_EQ(0xD00F4940U, sen::std_util::bit_cast<uint32_t>(swapped));
    EXPECT_EQ(0x40490FD0U, sen::std_util::bit_cast<uint32_t>(3.14159F));
  }

  {
    const auto swapped = sen::impl::swapBytes(3.14159);
    EXPECT_EQ(0x6E861BF0F9210940ULL, sen::std_util::bit_cast<uint64_t>(swapped));
    EXPECT_EQ(0x400921F9F01B866EULL, sen::std_util::bit_cast<uint64_t>(3.14159));
  }

  // Swapping twice is the identity, which also covers the negative and the denormal without
  // hard-coding their patterns.
  for (const auto value: {3.14159F, -3.14159F, 0.0F, std::numeric_limits<float32_t>::min()})
  {
    EXPECT_EQ(value, sen::impl::swapBytes(sen::impl::swapBytes(value)));
  }

  for (const auto value: {3.14159, -3.14159, 0.0, std::numeric_limits<float64_t>::min()})
  {
    EXPECT_EQ(value, sen::impl::swapBytes(sen::impl::swapBytes(value)));
  }
}
