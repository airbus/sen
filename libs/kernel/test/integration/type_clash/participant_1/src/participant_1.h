// === participant_1.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_TEST_INTEGRATION_TYPE_CLASH_PARTICIPANT_1_SRC_PARTICIPANT_1_H
#define SEN_LIBS_KERNEL_TEST_INTEGRATION_TYPE_CLASH_PARTICIPANT_1_SRC_PARTICIPANT_1_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/participant_1.stl.h"
#include "stl/sen/kernel/kernel_objects.stl.h"

// std
#include <memory>

namespace sen::test::type_clash
{

class App1ClassImpl final: public App1ClassBase
{
public:
  SEN_NOCOPY_NOMOVE(App1ClassImpl)
  using App1ClassBase::App1ClassBase;
  ~App1ClassImpl() override = default;

  void registered(sen::kernel::RegistrationApi& api) override;
  void shutdownKernelImpl() override;

private:
  sen::kernel::KernelApiInterface* kernelApiObj_ = nullptr;
  std::shared_ptr<sen::Subscription<sen::kernel::KernelApiInterface>> kernelApiSub_;
};

}  // namespace sen::test::type_clash

#endif  // SEN_LIBS_KERNEL_TEST_INTEGRATION_TYPE_CLASH_PARTICIPANT_1_SRC_PARTICIPANT_1_H
