// === work_queue_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/duration.h"
#include "sen/core/obj/detail/work_queue.h"

// google test
#include <gtest/gtest.h>

// std
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <thread>
#include <utility>

using sen::impl::Call;
using sen::impl::WorkQueue;

namespace
{

int64_t counter;

Call incrementCounter = []() { counter++; };  // NOLINT(cert-err58-cpp)
Call decrementCounter = []() { counter--; };  // NOLINT(cert-err58-cpp)

void checkWorkQueue(const size_t maxSize, const bool dropOldest) { EXPECT_NO_THROW(WorkQueue(maxSize, dropOldest)); }

}  // namespace

/// @test
/// Checks creation of queue with different size
/// @requirements(SEN-360)
TEST(WorkQueue, make)
{
  // basic
  {
    checkWorkQueue(50U, false);
    checkWorkQueue(100U, true);
  }

  // numeric limits
  {
    checkWorkQueue(std::numeric_limits<std::size_t>::max(), false);
    checkWorkQueue(std::numeric_limits<std::size_t>::max(), true);

    checkWorkQueue(std::numeric_limits<std::size_t>::min(), false);
    checkWorkQueue(std::numeric_limits<std::size_t>::min(), true);
  }

  // zero
  {
    checkWorkQueue(0U, false);
    checkWorkQueue(0U, true);
  }

  // negative values
  {
    checkWorkQueue(-20, false);
    checkWorkQueue(-935, true);
  }
}

/// @test
/// Checks creation of queue and basic push operation
/// @requirements(SEN-360)
TEST(WorkQueue, basic)
{
  counter = 0;
  WorkQueue queue(50, false);

  // push function to not enabled queue and execute
  queue.push(std::move(incrementCounter), true);
  EXPECT_FALSE(queue.executeAll());

  // enable and try execution of empty queue
  queue.enable();
  EXPECT_FALSE(queue.executeAll());

  // push function and execute
  queue.push(std::move(incrementCounter), false);
  EXPECT_TRUE(queue.executeAll());
  EXPECT_EQ(counter, 1);

  queue.disable();
  queue.push(std::move(incrementCounter), true);
  EXPECT_FALSE(queue.executeAll());
  EXPECT_EQ(counter, 1);

  queue.enable();
  for (auto i = 0; i < 10; i++)
  {
    queue.push(std::move(incrementCounter), true);
  }
  EXPECT_EQ(queue.getCurrentSize(), 10);

  queue.clear();
  EXPECT_EQ(queue.getCurrentSize(), 0);
}

/// @test
/// Checks max size argument of queue
/// @requirements(SEN-360)
TEST(WorkQueue, maxSize)
{
  std::size_t droppedCount = 0;
  counter = 0;
  WorkQueue queue(1, false);
  queue.setOnDropped([&](const auto& /*call*/) { droppedCount++; });
  queue.enable();

  queue.push(([]() { counter = 20; }), true);
  queue.push(([]() { counter = 100; }), false);
  EXPECT_TRUE(queue.executeAll());
  EXPECT_EQ(counter, 20);
  EXPECT_EQ(droppedCount, 1);

  EXPECT_FALSE(queue.executeAll());
  queue.push(([]() { counter = 10; }), false);
  EXPECT_TRUE(queue.executeAll());
  EXPECT_EQ(counter, 10);
}

/// @test
/// Checks max size zero work queue
/// @requirements(SEN-360)
TEST(WorkQueue, zeroSize)
{
  std::size_t droppedCount = 0;
  counter = 0;

  constexpr auto callsNum = 2000U;
  WorkQueue queue(0U, false);
  queue.setOnDropped([&](const auto& /*call*/) { droppedCount++; });

  // check can not push into queue if it is disabled
  queue.disable();
  queue.push([]() { counter = 300; }, true);
  EXPECT_EQ(counter, 0);
  EXPECT_EQ(droppedCount, 0);

  queue.enable();
  for (auto i = 0; i < callsNum; i++)
  {
    queue.push([]() { counter++; }, false);
  }

  EXPECT_TRUE(queue.executeAll());
  EXPECT_EQ(counter, callsNum);
  EXPECT_EQ(droppedCount, 0);
}

/// @test
/// Checks dropping of oldest call in queue
/// @requirements(SEN-360)
TEST(WorkQueue, dropOldest)
{
  std::size_t droppedCount = 0;
  counter = 0;

  WorkQueue queue(1, true);
  queue.setOnDropped([&](const auto& /*call*/) { droppedCount++; });
  queue.enable();

  queue.push(std::move(incrementCounter), false);
  queue.push(std::move(decrementCounter), false);
  EXPECT_TRUE(queue.executeAll());
  EXPECT_EQ(counter, -1);
  EXPECT_EQ(droppedCount, 1);
}

/// @test
/// Checks multiple push operations in queue with no limit
/// @requirements(SEN-360)
TEST(WorkQueue, unlimited)
{
  counter = 0;

  size_t numOperations = 2000;
  WorkQueue queue(0, false);
  queue.enable();

  for (auto i = 0; i < numOperations; i++)
  {
    queue.push(([]() { counter++; }), true);
  }
  EXPECT_EQ(queue.getCurrentSize(), numOperations);

  EXPECT_TRUE(queue.executeAll());
  EXPECT_EQ(counter, numOperations);
  EXPECT_EQ(queue.getCurrentSize(), 0);
}

/// @test
/// Check timeout on wait execution of all calls
/// @requirements(SEN-360)
TEST(WorkQueue, waitExecuteAll)
{
  counter = 0;
  constexpr auto nCalls = 2000U;

  // timeout zero
  {
    WorkQueue queue(0, false);

    for (auto i = 0; i < nCalls; i++)
    {
      queue.push(std::move(incrementCounter), true);
    }

    queue.waitExecuteAll(sen::Duration {0});
    EXPECT_EQ(counter, 0U);
  }
}

/// @test
/// Checks dropping of an element when no custom drop callback is set
/// @requirements(SEN-360)
TEST(WorkQueue, dropOldestDefaultCallback)
{
  counter = 0;

  WorkQueue queue(1, true);
  queue.enable();

  queue.push([]() { counter += 10; }, false);

  queue.push([]() { counter += 20; }, false);

  EXPECT_TRUE(queue.executeAll());

  EXPECT_EQ(counter, 20);
}

/// @test
/// Check wait execution of a single call
/// @requirements(SEN-360)
TEST(WorkQueue, waitExecuteOne)
{
  counter = 0;
  WorkQueue queue(0, false);
  queue.enable();

  std::thread worker([&queue]() { queue.waitExecuteOne(); });

  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  EXPECT_EQ(counter, 0);

  queue.push([]() { counter++; }, false);

  worker.join();

  EXPECT_EQ(counter, 1);
  EXPECT_EQ(queue.getCurrentSize(), 0);
}
