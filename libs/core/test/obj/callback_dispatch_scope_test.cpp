// === callback_dispatch_scope_test.cpp ================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/obj/callback.h"
#include "sen/core/obj/callback_dispatch_scope.h"
#include "sen/core/obj/detail/work_queue.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <optional>
#include <thread>

using sen::Callback;
using sen::impl::CallbackDispatchScope;
using sen::impl::isInsideCallbackDispatch;
using sen::impl::WorkQueue;

namespace
{

/// @test
/// Outside any callback the thread is not flagged
TEST(CallbackDispatchScope, notFlaggedByDefault) { EXPECT_FALSE(isInsideCallbackDispatch()); }

/// @test
/// A scope flags the thread while it lives and clears it when it ends
TEST(CallbackDispatchScope, flagsWhileAlive)
{
  {
    const CallbackDispatchScope scope;
    EXPECT_TRUE(isInsideCallbackDispatch());
  }
  EXPECT_FALSE(isInsideCallbackDispatch());
}

/// @test
/// The flag belongs to the thread running the callback only
TEST(CallbackDispatchScope, flagIsPerThread)
{
  const CallbackDispatchScope scope;

  std::optional<bool> seenByOtherThread;
  std::thread other([&]() { seenByOtherThread = isInsideCallbackDispatch(); });
  other.join();

  ASSERT_TRUE(seenByOtherThread.has_value());
  EXPECT_FALSE(*seenByOtherThread);
  EXPECT_TRUE(isInsideCallbackDispatch());
}

/// @test
/// Invoking a callback flags the thread for the duration of the user function only
TEST(CallbackDispatchScope, invokeFlagsTheUserFunction)
{
  auto queue = WorkQueue(50, false);

  std::optional<bool> seenInside;
  const auto cb =
    Callback<sen::EventInfo, uint64_t>(&queue, [&](uint64_t /*value*/) { seenInside = isInsideCallbackDispatch(); });

  ASSERT_FALSE(isInsideCallbackDispatch());
  cb.invoke({}, 1U);

  ASSERT_TRUE(seenInside.has_value());
  EXPECT_TRUE(*seenInside);
  EXPECT_FALSE(isInsideCallbackDispatch());
}

}  // namespace
