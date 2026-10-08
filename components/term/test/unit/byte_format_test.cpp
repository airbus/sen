// === byte_format_test.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "byte_format.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>

namespace sen::components::term::byte_format
{
namespace
{

constexpr std::size_t kilobyte = 1024U;
constexpr std::size_t megabyte = kilobyte * kilobyte;
constexpr std::size_t gigabyte = kilobyte * megabyte;

/// @test
/// A count below a kilobyte is given in bytes, with no decimal: a size the reader can count to
/// does not need one, and "512.0 B" reads as a measurement rather than a number of bytes.
TEST(ByteFormat, SmallCountsAreGivenInBytes)
{
  EXPECT_EQ(formatBytes(0U), "0 B");
  EXPECT_EQ(formatBytes(1U), "1 B");
  EXPECT_EQ(formatBytes(kilobyte - 1U), "1023 B");
}

/// @test
/// Each unit takes over exactly at its own boundary, which is where an off-by-one would show:
/// 1024 bytes is 1.0 KB and not 1024 B, and the same at the two boundaries above it.
TEST(ByteFormat, EachUnitTakesOverAtItsBoundary)
{
  EXPECT_EQ(formatBytes(kilobyte), "1.0 KB");
  EXPECT_EQ(formatBytes(megabyte - 1U), "1024.0 KB");
  EXPECT_EQ(formatBytes(megabyte), "1.0 MB");
  EXPECT_EQ(formatBytes(gigabyte - 1U), "1024.0 MB");
  EXPECT_EQ(formatBytes(gigabyte), "1.0 GB");
}

/// @test
/// A part of a unit is kept to one decimal, which is what the transport counters under inspect
/// are read as: the figure is a size to compare, not an amount to account for.
TEST(ByteFormat, PartOfAUnitKeepsOneDecimal)
{
  // Halves rather than quarters: 3.25 to one decimal is a tie, and which way a tie goes is the
  // standard library's business, not this component's.
  EXPECT_EQ(formatBytes(kilobyte + (kilobyte / 2U)), "1.5 KB");
  EXPECT_EQ(formatBytes((megabyte * 3U) + (megabyte / 2U)), "3.5 MB");
  EXPECT_EQ(formatBytes(gigabyte * 2U), "2.0 GB");
}

}  // namespace
}  // namespace sen::components::term::byte_format
