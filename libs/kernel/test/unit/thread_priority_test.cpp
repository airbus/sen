// === thread_priority_test.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef _WIN32

// kernel
#  include "posix/posix_api.h"
#  include "posix/posix_os.h"
#  include "thread.h"

// stl
#  include "stl/sen/kernel/basic_types.stl.h"

// gtest
#  include <gtest/gtest.h>

// std
#  include <atomic>
#  include <memory>
#  include <string>

namespace
{

std::atomic<int> ranCount {0};

/// The scheduling policy the thread saw for itself. Asking from outside races the thread's exit:
/// one process per test means it has usually gone, and pthread_getschedparam returns ESRCH.
std::atomic<int> observedPolicy {-1};

void countingThread(void* /*arg*/)
{
  observedPolicy.store(::sched_getscheduler(0));
  ranCount.fetch_add(1);
}

/// @test
/// A component that asks for a priority must still start when the process cannot be granted one:
/// a real-time policy needs a privilege many deployments do not have.
TEST(ThreadPriority, StartsEvenWhenThePriorityCannotBeGranted)
{
  sen::kernel::PosixOS os {std::make_shared<sen::kernel::NativePosixAPI>()};

  sen::kernel::ThreadConfig config {};
  config.name = "prio-test";
  config.function = &countingThread;
  config.arg = &ranCount;
  config.priority = sen::kernel::Priority::highest;

  const auto before = ranCount.load();
  auto result = os.createThread(config);

  ASSERT_TRUE(result.isOk()) << "asking for a priority must not stop the thread starting";
  EXPECT_TRUE(os.joinThread(result.getValue()));
  EXPECT_EQ(ranCount.load(), before + 1) << "the thread never ran";

  // Whether it was granted depends on this process's privileges. What must hold either way is
  // that the thread ran and that we can tell which happened.
  const auto applied = result.getValue().priorityApplied;
  RecordProperty("priorityApplied", applied ? "true" : "false");
}

/// @test
/// A component asking to be scheduled low must not be promoted. "lowest" is for background work,
/// so a real-time policy would put it above every other thread in the process.
TEST(ThreadPriority, DoesNotPromoteAThreadThatAskedToBeLow)
{
  sen::kernel::PosixOS os {std::make_shared<sen::kernel::NativePosixAPI>()};

  sen::kernel::ThreadConfig config {};
  config.name = "low-test";
  config.function = &countingThread;
  config.arg = &ranCount;
  config.priority = sen::kernel::Priority::lowest;

  observedPolicy.store(-1);
  auto result = os.createThread(config);
  ASSERT_TRUE(result.isOk());
  EXPECT_TRUE(os.joinThread(result.getValue()));

  // The thread reports its own policy, so this holds whether or not a real-time one could have
  // been granted. Asking instead whether the request was granted would pass on a privileged
  // machine, which is exactly where the defect bites.
  EXPECT_EQ(observedPolicy.load(), SCHED_OTHER) << "a thread that asked to be low was given a real-time policy";
}

/// @test
/// An affinity the machine cannot honour leaves the thread running unpinned and says so. Failing
/// thread creation instead would destroy an object the new thread is still reading.
///
/// Linux only: the macOS shim always reports that it applied, so there is nothing to observe.
#  if defined(__linux__)
TEST(ThreadPriority, RunsUnpinnedWhenTheAffinityCannotBeApplied)
{
  sen::kernel::PosixOS os {std::make_shared<sen::kernel::NativePosixAPI>()};

  sen::kernel::ThreadConfig config {};
  config.name = "affinity-test";
  config.function = &countingThread;
  config.arg = &ranCount;
  // The mask is 64 bits wide, so on a machine with 64 or more cpus every bit names a real one and
  // there is no unapplicable mask to ask for.
  if (std::thread::hardware_concurrency() >= 64U)
  {
    GTEST_SKIP() << "every bit of the mask names a real cpu on this machine";
  }

  config.affinity = 1ULL << 63U;

  const auto before = ranCount.load();
  auto result = os.createThread(config);

  ASSERT_TRUE(result.isOk()) << "an affinity the machine cannot honour must not fail thread creation";
  EXPECT_FALSE(result.getValue().affinityApplied) << "it was reported as applied";
  EXPECT_TRUE(os.joinThread(result.getValue()));
  EXPECT_EQ(ranCount.load(), before + 1) << "the thread never ran";
}
#  endif  // __linux__

/// @test
/// A thread that genuinely could not be created is reported as an error. The creation path used to
/// discard that result, handing the caller a handle to a thread that never started.
TEST(ThreadPriority, ReportsAThreadThatCouldNotBeCreated)
{
  sen::kernel::PosixOS os {std::make_shared<sen::kernel::NativePosixAPI>()};

  sen::kernel::ThreadConfig config {};
  config.name = "doomed";
  config.function = &countingThread;
  config.arg = &ranCount;
  // This has to exceed the address space, not merely be enormous: a 64TB stack is mapped without
  // complaint under overcommit, and the test then skips instead of exercising the failure. Do not
  // reduce it -- a smaller value turns this into a permanent skip that still looks green.
  config.stackSize = 1ULL << 62U;

  const auto before = ranCount.load();
  auto result = os.createThread(config);

  if (result.isOk())
  {
    // the system did map it after all, so there is nothing to assert here
    EXPECT_TRUE(os.joinThread(result.getValue()));
    GTEST_SKIP() << "this system allocated a stack larger than its address space";
  }

  EXPECT_EQ(ranCount.load(), before) << "the thread ran, so this is not the failure path";
}

}  // namespace

#endif  // _WIN32
