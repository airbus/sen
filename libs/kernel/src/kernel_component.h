// === kernel_component.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_KERNEL_COMPONENT_H
#define SEN_LIBS_KERNEL_SRC_KERNEL_COMPONENT_H

#include "kernel_impl.h"
#include "sen/core/base/class_helpers.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/sen/kernel/kernel_objects.stl.h"

namespace sen::kernel::impl
{

class KernelComponent final: public Component
{
  SEN_NOCOPY_NOMOVE(KernelComponent)

public:
  explicit KernelComponent(KernelImpl* impl);

  ~KernelComponent() override = default;

public:
  [[nodiscard]] FuncResult run(RunApi& api) override;

  [[nodiscard]] bool isRealTimeOnly() const noexcept override;

private:
  KernelImpl* impl_;
};

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_KERNEL_COMPONENT_H
