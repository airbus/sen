// === participant_3.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_TEST_INTEGRATION_TYPE_CLASH_PARTICIPANT_3_SRC_PARTICIPANT_3_H
#define SEN_LIBS_KERNEL_TEST_INTEGRATION_TYPE_CLASH_PARTICIPANT_3_SRC_PARTICIPANT_3_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/participant_3.stl.h"
#include "stl/sen/kernel/kernel_objects.stl.h"

// std
#include <memory>

namespace sen::test::type_clash
{

class ClashTypeImpl: public ClashTypeBase
{
public:
  SEN_NOCOPY_NOMOVE(ClashTypeImpl)
  using ClashTypeBase::ClashTypeBase;
  ~ClashTypeImpl() override = default;
};

class App3ClassImpl final: public App3ClassBase<ClashTypeImpl>
{
public:
  SEN_NOCOPY_NOMOVE(App3ClassImpl)
  using App3ClassBase<ClashTypeImpl>::App3ClassBase;
  ~App3ClassImpl() override = default;

  void registered(sen::kernel::RegistrationApi& api) override;
  void shutdownKernelImpl() override;

private:
  sen::kernel::KernelApiInterface* kernelApiObj_ = nullptr;
  std::shared_ptr<sen::Subscription<sen::kernel::KernelApiInterface>> kernelApiSub_;
};

}  // namespace sen::test::type_clash

#endif  // SEN_LIBS_KERNEL_TEST_INTEGRATION_TYPE_CLASH_PARTICIPANT_3_SRC_PARTICIPANT_3_H
