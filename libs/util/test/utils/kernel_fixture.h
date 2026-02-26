// === kernel_fixture.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_SEN_LIBS_UTILS_TEST_UTILS_KERNEL_FIXTURE_H_H
#define SEN_SEN_LIBS_UTILS_TEST_UTILS_KERNEL_FIXTURE_H_H

// gtest
#include <gtest/gtest.h>

// sen
#include "sen/kernel/test_kernel.h"

/// Wrapper class used in the integration tests that facilitates the interaction with the Sen kernel test API,
/// managing the allocation and deallocation of common resources.
class KernelFixture: public ::testing::Test
{
protected:
  /// Wrapper for the onInit method of the Sen kernel test API.
  void onInit(sen::kernel::TestComponent::InitFunc initFunc);

  /// Wrapper for the onRun method of the Sen kernel test API.
  void onRun(sen::kernel::TestComponent::RunFunc runFunc);

  /// Wrapper for the onUnload method of the Sen kernel test API.
  void onUnload(sen::kernel::TestComponent::UnloadFunc unloadFunc);

protected:
  /// Constructor for the Sen kernel test API that takes care about allocating and deallocating resources needed for
  /// the correct functioning of the kernel.
  std::unique_ptr<sen::kernel::TestKernel> makeKernel();

  /// Virtual method that can be implemented by integration test fixtures which need to deallocate specific resources
  /// at the end of each test.
  virtual void deallocateResources();

private:
  sen::kernel::TestComponent component_;
  bool unloadCallbackInstalled_ = false;
};

#endif  // SEN_SEN_LIBS_UTILS_TEST_UTILS_KERNEL_FIXTURE_H_H
