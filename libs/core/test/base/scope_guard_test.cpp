// === scope_guard_test.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/scope_guard.h"

// google test
#include <gtest/gtest.h>

using sen::makeScopeGuard;

/// @test
/// Checks scope guard basic constructor
/// @requirements(SEN-355)
TEST(ScopeGuard, basic)
{
  bool scope1 {false};
  bool scope2 {false};

  {
    auto sg1 = makeScopeGuard([&scope1] { scope1 = true; });
    auto sg2 = makeScopeGuard([&scope2] { scope2 = true; });
  }

  EXPECT_TRUE(scope1);
  EXPECT_TRUE(scope2);
}
