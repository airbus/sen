// === util.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "util.h"

// sen
#include "sen/kernel/component_api.h"

// spdlog
#include <spdlog/logger.h>

// std
#include <memory>

namespace sen::components::recorder
{

std::shared_ptr<spdlog::logger> getLogger() { return kernel::KernelApi::getOrCreateLogger("recorder"); }

}  // namespace sen::components::recorder
