// === util.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "util.h"

// sen
#include "sen/kernel/component_api.h"

// std
#include <memory>

namespace sen::components::term
{

std::shared_ptr<spdlog::logger> getLogger() { return kernel::KernelApi::getOrCreateLogger("term"); }

}  // namespace sen::components::term
