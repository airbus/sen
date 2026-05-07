// === participant_3.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "participant_3.h"

// sen
#include "sen/core/meta/class_type.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/sen/kernel/kernel_objects.stl.h"

namespace sen::test::type_clash
{

SEN_EXPORT_CLASS(ClashTypeImpl)

void App3ClassImpl::registered(sen::kernel::RegistrationApi& api)
{
  kernelApiSub_ = api.selectAllFrom<sen::kernel::KernelApiInterface>(
    "local.kernel", [this](const auto& addedObjects) { kernelApiObj_ = *addedObjects.begin(); });
}

void App3ClassImpl::shutdownKernelImpl()
{
  if (kernelApiObj_)
  {
    kernelApiObj_->shutdown();
  }
}

SEN_EXPORT_CLASS(App3ClassImpl)

}  // namespace sen::test::type_clash
