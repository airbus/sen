// === util.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_REPLAYER_SRC_UTIL_H
#define SEN_COMPONENTS_REPLAYER_SRC_UTIL_H

#include <spdlog/logger.h>

namespace sen::components::replayer
{

[[nodiscard]] std::shared_ptr<spdlog::logger> getLogger();

}  // namespace sen::components::replayer

#endif  // SEN_COMPONENTS_REPLAYER_SRC_UTIL_H
