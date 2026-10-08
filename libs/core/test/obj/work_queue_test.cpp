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
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <thread>
#include <vector>

using sen::impl::Call;
using sen::impl::WorkQueue;

namespace
{

int64_t counter;

/// Bounded so a producer cannot make the test's cost a function of how long the main thread
/// happens to wait, and force=false so the bound actually applies.
constexpr std::size_t queueBound = 64U;
constexpr int pusherCount = 4;

// Plain functions rather than Call objects shared between tests. A moved-from Call is empty, and
// executing an empty one jumps through a null pointer because NDEBUG compiles out the assert that
// catches it, so which test moved it first decided whether a later one crashed.
void incrementCounter() { counter++; }
void decrementCounter() { counter--; }

void checkWorkQueue(const size_t maxSize, const bool dropOldest) { EXPECT_NO_THROW(WorkQueue(maxSize, dropOldest)); }

}  // namespace

/// @test
/// Constructs without throwing for any maximum size, including zero, size_t extremes, and
/// negative values converted to size_t, under either drop policy.
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
/// Ignores pushes before enable and after disable, executes queued calls in between with
/// executeAll reporting whether any ran, and clear resets the current size to zero.
/// @requirements(SEN-360)
TEST(WorkQueue, basic)
{
  counter = 0;
  WorkQueue queue(50, false);

  // push function to not enabled queue and execute
  queue.push(&incrementCounter, true);
  EXPECT_FALSE(queue.executeAll());

  // enable and try execution of empty queue
  queue.enable();
  EXPECT_FALSE(queue.executeAll());

  // push function and execute
  queue.push(&incrementCounter, false);
  EXPECT_TRUE(queue.executeAll());
  EXPECT_EQ(counter, 1);

  queue.disable();
  queue.push(&incrementCounter, true);
  EXPECT_FALSE(queue.executeAll());
  EXPECT_EQ(counter, 1);

  queue.enable();
  for (auto i = 0; i < 10; i++)
  {
    queue.push(&incrementCounter, true);
  }
  EXPECT_EQ(queue.getCurrentSize(), 10);

  queue.clear();
  EXPECT_EQ(queue.getCurrentSize(), 0);
}

/// @test
/// Discards an unforced push that meets a full queue when dropOldest is false, firing the
/// onDropped callback once, and accepts pushes again after the queue drains.
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
/// Ignores a push while disabled without invoking the drop callback, then treats maximum size
/// zero as unbounded, queueing and executing 2000 unforced pushes with none dropped.
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
  for (auto i = 0U; i < callsNum; i++)
  {
    queue.push([]() { counter++; }, false);
  }

  EXPECT_TRUE(queue.executeAll());
  EXPECT_EQ(counter, callsNum);
  EXPECT_EQ(droppedCount, 0);
}

/// @test
/// Evicts the oldest queued call in favor of a new push once the bound is reached with
/// dropOldest true, firing the drop callback once, so only the newest call executes.
/// @requirements(SEN-360)
TEST(WorkQueue, dropOldest)
{
  std::size_t droppedCount = 0;
  counter = 0;

  WorkQueue queue(1, true);
  queue.setOnDropped([&](const auto& /*call*/) { droppedCount++; });
  queue.enable();

  queue.push(&incrementCounter, false);
  queue.push(&decrementCounter, false);
  EXPECT_TRUE(queue.executeAll());
  EXPECT_EQ(counter, -1);
  EXPECT_EQ(droppedCount, 1);
}

/// @test
/// Accumulates 2000 pushes when constructed with maximum size zero, reporting the full count
/// from getCurrentSize, then executes them all and returns to size zero.
/// @requirements(SEN-360)
TEST(WorkQueue, unlimited)
{
  counter = 0;

  size_t numOperations = 2000;
  WorkQueue queue(0, false);
  queue.enable();

  for (size_t i = 0; i < numOperations; i++)
  {
    queue.push(([]() { counter++; }), true);
  }
  EXPECT_EQ(queue.getCurrentSize(), numOperations);

  EXPECT_TRUE(queue.executeAll());
  EXPECT_EQ(counter, numOperations);
  EXPECT_EQ(queue.getCurrentSize(), 0);
}

/// @test
/// Ignores pushes on a never-enabled queue, so waitExecuteAll with a zero timeout returns
/// without executing any call.
/// @requirements(SEN-360)
TEST(WorkQueue, waitExecuteAll)
{
  counter = 0;
  constexpr auto nCalls = 2000U;

  // timeout zero
  {
    WorkQueue queue(0, false);

    for (auto i = 0U; i < nCalls; i++)
    {
      queue.push(&incrementCounter, true);
    }

    queue.waitExecuteAll(sen::Duration {0});
    EXPECT_EQ(counter, 0U);
  }
}

/// @test
/// Drops the oldest call silently once the bound is reached and no onDropped callback was set,
/// leaving only the newest call to execute.
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
/// Blocks waitExecuteOne on an empty queue until a call is pushed from another thread, then
/// executes that call and leaves the queue empty.
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

/// @test
/// Survives 100 rounds of clear racing a thread that pushes continuously into an enabled
/// bounded queue, without freeing storage an in-flight push still uses.
TEST(WorkQueue, clearDoesNotFreeUnderAConcurrentPush)
{
  // Bounded, and the clear waits for a push to be observed rather than for a duration: an
  // unbounded producer plus a sleep makes the cost depend on the host's timer granularity, which
  // on Windows is coarse enough to turn 200 microseconds into tens of milliseconds of allocation.
  for (auto round = 0; round < 100; ++round)
  {
    WorkQueue queue(queueBound, true);
    queue.enable();

    std::atomic_bool stop {false};
    std::atomic<int> pushes {0};
    std::thread pusher(
      [&queue, &stop, &pushes]()
      {
        while (!stop.load(std::memory_order_relaxed))
        {
          queue.push([]() {}, false);
          pushes.fetch_add(1, std::memory_order_relaxed);
        }
      });

    while (pushes.load(std::memory_order_relaxed) == 0)
    {
      std::this_thread::yield();
    }
    queue.clear();

    stop.store(true);
    pusher.join();
  }
}

/// @test
/// Survives 100 rounds of disable then clear while four pusher threads stay active, without
/// freeing storage out from under a push that passed its enabled check before the disable.
TEST(WorkQueue, clearAfterDisableDoesNotFreeUnderAnInFlightPush)
{
  for (auto round = 0; round < 100; ++round)
  {
    WorkQueue queue(queueBound, true);
    queue.enable();

    std::atomic_bool stop {false};
    std::atomic<int> pushes {0};
    std::vector<std::thread> pushers;
    pushers.reserve(pusherCount);
    for (auto i = 0; i < pusherCount; ++i)
    {
      pushers.emplace_back(
        [&queue, &stop, &pushes]()
        {
          while (!stop.load(std::memory_order_relaxed))
          {
            queue.push([]() {}, false);
            pushes.fetch_add(1, std::memory_order_relaxed);
          }
        });
    }

    while (pushes.load(std::memory_order_relaxed) < pusherCount)
    {
      std::this_thread::yield();
    }
    queue.disable();
    queue.clear();

    stop.store(true);
    for (auto& t: pushers)
    {
      t.join();
    }
  }
}
