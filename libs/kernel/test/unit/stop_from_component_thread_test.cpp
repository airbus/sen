// === stop_from_component_thread_test.cpp =============================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/base/duration.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/test_kernel.h"

// google test
#include <gtest/gtest.h>

// std
#include <atomic>
#include <chrono>
#include <memory>

namespace sen::kernel
{
namespace
{

/// A component asking the kernel to stop from its own thread, which is what a terminal component does
/// when the user types `exit`.
///
/// On a kernel started with doNotBlock, which is what TestKernel uses and what an embedding application may
/// use, `requestStop` would otherwise run the whole shutdown on the caller's thread. That
/// reaches the calling component's own `stopThread`, which joins the calling thread: EDEADLK, and the
/// join failure went to `toErrorState`, which calls std::terminate. So the ordinary exit path aborted
/// the process, and (because a terminal component has taken the console over) said nothing about it.
///
/// This test would not have failed before the fix. It would have killed the test binary.
TEST(StopFromComponentThread, AComponentCanAskToStopWithoutAbortingTheProcess)
{
  TestComponent component;
  std::atomic_bool asked {false};

  component.onRun(
    [&asked](RunApi& api) -> FuncResult
    {
      return api.execLoop(Duration(std::chrono::milliseconds(10)),
                          [&api, &asked]()
                          {
                            if (!asked.exchange(true))
                            {
                              api.requestKernelStop(0);
                            }
                          });
    });

  auto kernel = std::make_unique<TestKernel>(&component);
  kernel->step(5);

  EXPECT_TRUE(asked.load()) << "the component never reached the request, so nothing was exercised";

  // Reaching here at all is the assertion: the process is still alive and the test binary can go on.
  kernel.reset();
  SUCCEED() << "the kernel survived a stop requested from a component thread";
}

}  // namespace
}  // namespace sen::kernel
