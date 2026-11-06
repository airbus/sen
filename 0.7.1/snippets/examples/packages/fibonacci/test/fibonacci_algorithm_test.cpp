// === fibonacci_algorithm_test.cpp ====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>

namespace fibonacci
{
// computeFibonacci is defined in fibonacci.cpp (not a public header).
// The TEST_TARGET static library exposes it so tests can call it directly.
[[nodiscard]] uint64_t computeFibonacci(uint32_t n);
}  // namespace fibonacci

/// @test
/// computeFibonacci(0) is 0.
TEST(FibonacciAlgorithmTest, BaseCase0) { EXPECT_EQ(fibonacci::computeFibonacci(0), 0U); }
/// @test
/// computeFibonacci(1) is 1.
TEST(FibonacciAlgorithmTest, BaseCase1) { EXPECT_EQ(fibonacci::computeFibonacci(1), 1U); }
/// @test
/// computeFibonacci returns 1, 2, 3 and 5 for n from 2 to 5, and 55 for 10.
TEST(FibonacciAlgorithmTest, SmallValues)
{
  EXPECT_EQ(fibonacci::computeFibonacci(2), 1U);
  EXPECT_EQ(fibonacci::computeFibonacci(3), 2U);
  EXPECT_EQ(fibonacci::computeFibonacci(4), 3U);
  EXPECT_EQ(fibonacci::computeFibonacci(5), 5U);
  EXPECT_EQ(fibonacci::computeFibonacci(10), 55U);
}

/// @test
/// computeFibonacci(20) is 6765, so the recursion holds beyond the first terms.
TEST(FibonacciAlgorithmTest, LargerValue) { EXPECT_EQ(fibonacci::computeFibonacci(20), 6765U); }
