// === assert_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/assert.h"

// sen
#include "sen/core/base/detail/assert_impl.h"
#include "sen/core/base/source_location.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>

using sen::impl::CheckInfo;
using sen::impl::CheckType;
using sen::impl::FailedCheckHandler;
using sen::impl::setFailedCheckHandler;

namespace
{

[[nodiscard]] CheckInfo& getExpectedCheckInfo()
{
  static CheckInfo expectedCheckInfo = {CheckType::assert, "", SEN_SL()};
  return expectedCheckInfo;
}

std::size_t callCount = 0U;

void testAssertHandler(const CheckInfo& checkInfo) noexcept
{
  callCount++;

  auto& expectedCheckInfo = getExpectedCheckInfo();
  ASSERT_EQ(checkInfo.getCheckType(), expectedCheckInfo.getCheckType());
  ASSERT_EQ(checkInfo.getSourceLocation().lineNumber, expectedCheckInfo.getSourceLocation().lineNumber + 1);
  ASSERT_EQ(checkInfo.getSourceLocation().fileName, expectedCheckInfo.getSourceLocation().fileName);
  ASSERT_EQ(checkInfo.getExpression(), expectedCheckInfo.getExpression());
}

class AssertTest: public ::testing::Test
{
protected:
  void SetUp() override
  {
    auto oldHandler = setFailedCheckHandler(testAssertHandler);
    ASSERT_NE(oldHandler, nullptr);
    prevCallCount = callCount;
  }

  void TearDown() override { oldHandler = setFailedCheckHandler(testAssertHandler); }

  FailedCheckHandler oldHandler;   // NOLINT(misc-non-private-member-variables-in-classes)
  std::size_t prevCallCount = 0U;  // NOLINT(misc-non-private-member-variables-in-classes)
};

}  // namespace

/// @test
/// Check expect type macro triggering
/// @requirements(SEN-363)
TEST_F(AssertTest, macros_triggering_expect)
{
  auto& expectedCheckInfo = getExpectedCheckInfo();

  expectedCheckInfo = {CheckType::expect, "false", SEN_SL()};
  SEN_EXPECT(false);

  ASSERT_EQ(callCount, prevCallCount + 1);
}

/// @test
/// Check ensure type macro triggering
/// @requirements(SEN-363)
TEST_F(AssertTest, macros_triggering_ensure)
{
  auto& expectedCheckInfo = getExpectedCheckInfo();

  expectedCheckInfo = {CheckType::ensure, "false", SEN_SL()};
  SEN_ENSURE(false);

  ASSERT_EQ(callCount, prevCallCount + 1);
}

/// @test
/// Check assert type macro triggering
/// @requirements(SEN-363)
TEST_F(AssertTest, macros_triggering_assert)
{
  auto& expectedCheckInfo = getExpectedCheckInfo();

  expectedCheckInfo = {CheckType::assert, "false", SEN_SL()};
  SEN_ASSERT(false);

  ASSERT_EQ(callCount, prevCallCount + 1);
}

/// @test
/// Check not triggering macros
/// @requirements(SEN-363)
TEST_F(AssertTest, macros_not_triggering)
{
  SEN_EXPECT(true);
  SEN_ENSURE(true);
  SEN_ASSERT(true);

  ASSERT_EQ(callCount, prevCallCount);
}
