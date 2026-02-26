// === integer_compare_test.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/integer_compare.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <tuple>

using sen::std_util::cmp_equal;
using sen::std_util::cmp_greater;
using sen::std_util::cmp_greater_equal;
using sen::std_util::cmp_less;
using sen::std_util::cmp_less_equal;
using sen::std_util::cmp_not_equal;

struct ComparisonResultSpec
{
  bool shouldCompareEqual;
  bool shouldCompareNotEqual;
  bool shouldCompareLess;
  bool shouldCompareGreater;
  bool shouldCompareLessEqual;
  bool shouldCompareGreaterEqual;
};

template <typename T1, typename T2>
struct IntegerCompareTestSuiteBase: public testing::TestWithParam<std::tuple<T1, T2, ComparisonResultSpec>>
{
  void runComparison()
  {
    ComparisonResultSpec expect = std::get<2>(this->GetParam());
    T1 lhs = std::get<0>(this->GetParam());
    T2 rhs = std::get<1>(this->GetParam());

    EXPECT_EQ(cmp_equal(lhs, rhs), expect.shouldCompareEqual);
    EXPECT_EQ(cmp_not_equal(lhs, rhs), expect.shouldCompareNotEqual);

    EXPECT_EQ(cmp_less(lhs, rhs), expect.shouldCompareLess);
    EXPECT_EQ(cmp_greater(lhs, rhs), expect.shouldCompareGreater);

    EXPECT_EQ(cmp_less_equal(lhs, rhs), expect.shouldCompareLessEqual);
    EXPECT_EQ(cmp_greater_equal(lhs, rhs), expect.shouldCompareGreaterEqual);
  }
};

// ---------------------------------------------------------------------------------------------------------------------
// signed vs signed comparisons

class IntegerCompareTestSuiteSignedCmpSigned: public IntegerCompareTestSuiteBase<int32_t, int32_t>
{
};

/// @test
/// Check safe comparison between two signed types
/// @requirements(SEN-1046)
TEST_P(IntegerCompareTestSuiteSignedCmpSigned, CheckCorrectComparison) { runComparison(); };

INSTANTIATE_TEST_SUITE_P(
  IntegerCompareTests,
  IntegerCompareTestSuiteSignedCmpSigned,
  ::testing::Values(std::make_tuple(int32_t {4},
                                    int32_t {7},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ false,
                                                          /* .shouldCompareNotEqual = */ true,
                                                          /* .shouldCompareLess = */ true,
                                                          /* .shouldCompareGreater = */ false,
                                                          /* .shouldCompareLessEqual = */ true,
                                                          /* .shouldCompareGreaterEqual = */ false}),
                    std::make_tuple(int32_t {70},
                                    int32_t {42},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ false,
                                                          /* .shouldCompareNotEqual = */ true,
                                                          /* .shouldCompareLess = */ false,
                                                          /* .shouldCompareGreater = */ true,
                                                          /* .shouldCompareLessEqual = */ false,
                                                          /* .shouldCompareGreaterEqual = */ true}),
                    std::make_tuple(int32_t {80},
                                    int32_t {80},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ true,
                                                          /* .shouldCompareNotEqual = */ false,
                                                          /* .shouldCompareLess = */ false,
                                                          /* .shouldCompareGreater = */ false,
                                                          /* .shouldCompareLessEqual = */ true,
                                                          /* .shouldCompareGreaterEqual = */ true}),
                    std::make_tuple(int32_t {-4},
                                    int32_t {-7},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ false,
                                                          /* .shouldCompareNotEqual = */ true,
                                                          /* .shouldCompareLess = */ false,
                                                          /* .shouldCompareGreater = */ true,
                                                          /* .shouldCompareLessEqual = */ false,
                                                          /* .shouldCompareGreaterEqual = */ true}),
                    std::make_tuple(int32_t {-7},
                                    int32_t {-4},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ false,
                                                          /* .shouldCompareNotEqual = */ true,
                                                          /* .shouldCompareLess = */ true,
                                                          /* .shouldCompareGreater = */ false,
                                                          /* .shouldCompareLessEqual = */ true,
                                                          /* .shouldCompareGreaterEqual = */ false}),
                    std::make_tuple(int32_t {-4},
                                    int32_t {7},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ false,
                                                          /* .shouldCompareNotEqual = */ true,
                                                          /* .shouldCompareLess = */ true,
                                                          /* .shouldCompareGreater = */ false,
                                                          /* .shouldCompareLessEqual = */ true,
                                                          /* .shouldCompareGreaterEqual = */ false}),
                    std::make_tuple(int32_t {7},
                                    int32_t {-4},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ false,
                                                          /* .shouldCompareNotEqual = */ true,
                                                          /* .shouldCompareLess = */ false,
                                                          /* .shouldCompareGreater = */ true,
                                                          /* .shouldCompareLessEqual = */ false,
                                                          /* .shouldCompareGreaterEqual = */ true}),
                    std::make_tuple(int32_t {-40},
                                    int32_t {-40},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ true,
                                                          /* .shouldCompareNotEqual = */ false,
                                                          /* .shouldCompareLess = */ false,
                                                          /* .shouldCompareGreater = */ false,
                                                          /* .shouldCompareLessEqual = */ true,
                                                          /* .shouldCompareGreaterEqual = */ true})));

// ---------------------------------------------------------------------------------------------------------------------
// unsigned signed vs unsigned signed comparisons

class IntegerCompareTestSuiteUnsignedCmpUnsigned: public IntegerCompareTestSuiteBase<uint32_t, uint32_t>
{
};

/// @test
/// Check safe comparison between two unsigned types
/// @requirements(SEN-1046)
TEST_P(IntegerCompareTestSuiteUnsignedCmpUnsigned, CheckCorrectComparison) { runComparison(); };

INSTANTIATE_TEST_SUITE_P(
  IntegerCompareTests,
  IntegerCompareTestSuiteUnsignedCmpUnsigned,
  ::testing::Values(std::make_tuple(uint32_t {4},
                                    uint32_t {7},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ false,
                                                          /* .shouldCompareNotEqual = */ true,
                                                          /* .shouldCompareLess = */ true,
                                                          /* .shouldCompareGreater = */ false,
                                                          /* .shouldCompareLessEqual = */ true,
                                                          /* .shouldCompareGreaterEqual = */ false}),
                    std::make_tuple(uint32_t {70},
                                    uint32_t {42},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ false,
                                                          /* .shouldCompareNotEqual = */ true,
                                                          /* .shouldCompareLess = */ false,
                                                          /* .shouldCompareGreater = */ true,
                                                          /* .shouldCompareLessEqual = */ false,
                                                          /* .shouldCompareGreaterEqual = */ true}),
                    std::make_tuple(uint32_t {80},
                                    uint32_t {80},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ true,
                                                          /* .shouldCompareNotEqual = */ false,
                                                          /* .shouldCompareLess = */ false,
                                                          /* .shouldCompareGreater = */ false,
                                                          /* .shouldCompareLessEqual = */ true,
                                                          /* .shouldCompareGreaterEqual = */ true})));

// ---------------------------------------------------------------------------------------------------------------------
// signed vs unsigned signed comparisons

class IntegerCompareTestSuiteSignedCmpUnsigned: public IntegerCompareTestSuiteBase<int32_t, uint32_t>
{
};

/// @test
/// Check safe comparison from signed to unsigned types
/// @requirements(SEN-1046)
TEST_P(IntegerCompareTestSuiteSignedCmpUnsigned, CheckCorrectComparison) { runComparison(); };

INSTANTIATE_TEST_SUITE_P(
  IntegerCompareTests,
  IntegerCompareTestSuiteSignedCmpUnsigned,
  ::testing::Values(std::make_tuple(int32_t {4},
                                    uint32_t {7},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ false,
                                                          /* .shouldCompareNotEqual = */ true,
                                                          /* .shouldCompareLess = */ true,
                                                          /* .shouldCompareGreater = */ false,
                                                          /* .shouldCompareLessEqual = */ true,
                                                          /* .shouldCompareGreaterEqual = */ false}),
                    std::make_tuple(int32_t {70},
                                    uint32_t {42},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ false,
                                                          /* .shouldCompareNotEqual = */ true,
                                                          /* .shouldCompareLess = */ false,
                                                          /* .shouldCompareGreater = */ true,
                                                          /* .shouldCompareLessEqual = */ false,
                                                          /* .shouldCompareGreaterEqual = */ true}),
                    std::make_tuple(int32_t {80},
                                    uint32_t {80},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ true,
                                                          /* .shouldCompareNotEqual = */ false,
                                                          /* .shouldCompareLess = */ false,
                                                          /* .shouldCompareGreater = */ false,
                                                          /* .shouldCompareLessEqual = */ true,
                                                          /* .shouldCompareGreaterEqual = */ true}),
                    std::make_tuple(int32_t {-4},
                                    uint32_t {7},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ false,
                                                          /* .shouldCompareNotEqual = */ true,
                                                          /* .shouldCompareLess = */ true,
                                                          /* .shouldCompareGreater = */ false,
                                                          /* .shouldCompareLessEqual = */ true,
                                                          /* .shouldCompareGreaterEqual = */ false})));

// ---------------------------------------------------------------------------------------------------------------------
// unsigned signed vs signed comparisons

class IntegerCompareTestSuiteUnsignedCmpSigned: public IntegerCompareTestSuiteBase<uint32_t, int32_t>
{
};

/// @test
/// Check safe comparison from unsigned to signed types
/// @requirements(SEN-1046)
TEST_P(IntegerCompareTestSuiteUnsignedCmpSigned, CheckCorrectComparison) { runComparison(); };

INSTANTIATE_TEST_SUITE_P(
  IntegerCompareTests,
  IntegerCompareTestSuiteUnsignedCmpSigned,
  ::testing::Values(std::make_tuple(uint32_t {4},
                                    int32_t {7},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ false,
                                                          /* .shouldCompareNotEqual = */ true,
                                                          /* .shouldCompareLess = */ true,
                                                          /* .shouldCompareGreater = */ false,
                                                          /* .shouldCompareLessEqual = */ true,
                                                          /* .shouldCompareGreaterEqual = */ false}),
                    std::make_tuple(uint32_t {70},
                                    int32_t {42},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ false,
                                                          /* .shouldCompareNotEqual = */ true,
                                                          /* .shouldCompareLess = */ false,
                                                          /* .shouldCompareGreater = */ true,
                                                          /* .shouldCompareLessEqual = */ false,
                                                          /* .shouldCompareGreaterEqual = */ true}),
                    std::make_tuple(uint32_t {80},
                                    int32_t {80},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ true,
                                                          /* .shouldCompareNotEqual = */ false,
                                                          /* .shouldCompareLess = */ false,
                                                          /* .shouldCompareGreater = */ false,
                                                          /* .shouldCompareLessEqual = */ true,
                                                          /* .shouldCompareGreaterEqual = */ true}),
                    std::make_tuple(uint32_t {7},
                                    int32_t {-4},
                                    ComparisonResultSpec {/* .shouldCompareEqual = */ false,
                                                          /* .shouldCompareNotEqual = */ true,
                                                          /* .shouldCompareLess = */ false,
                                                          /* .shouldCompareGreater = */ true,
                                                          /* .shouldCompareLessEqual = */ false,
                                                          /* .shouldCompareGreaterEqual = */ true})));
