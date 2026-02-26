// === kernel_fixture.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "kernel_fixture.h"

// sen
#include "sen/core/base/result.h"
#include "sen/kernel/test_kernel.h"

// std
#include <memory>
#include <utility>

void KernelFixture::onInit(sen::kernel::TestComponent::InitFunc initFunc) { component_.onInit(std::move(initFunc)); }

void KernelFixture::onRun(sen::kernel::TestComponent::RunFunc runFunc) { component_.onRun(std::move(runFunc)); }

void KernelFixture::onUnload(sen::kernel::TestComponent::UnloadFunc unloadFunc)
{
  component_.onUnload(
    [this, unloadFunc](auto&& unloadApi)
    {
      deallocateResources();
      unloadFunc(std::move(unloadApi));  // NOLINT
      return sen::Ok();
    });
  unloadCallbackInstalled_ = true;
}

std::unique_ptr<sen::kernel::TestKernel> KernelFixture::makeKernel()
{
  if (!unloadCallbackInstalled_)
  {
    component_.onUnload(
      [this](auto&& /*unloadApi*/)
      {
        deallocateResources();
        return sen::Ok();
      });  // NOLINT
    unloadCallbackInstalled_ = true;
  }

  return std::make_unique<sen::kernel::TestKernel>(&component_);
}

void KernelFixture::deallocateResources()
{
  // no code needed
}
