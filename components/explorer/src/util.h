// === util.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_EXPLORER_SRC_UTIL_H
#define SEN_COMPONENTS_EXPLORER_SRC_UTIL_H

// spdlog
#include <spdlog/logger.h>

// std
#include <memory>
#include <string>
#include <string_view>

namespace sen::components::explorer
{

[[nodiscard]] std::shared_ptr<spdlog::logger> getLogger();

struct UIError
{
  std::string message;
  double errorTime {};
};

void setUIError(UIError& error, std::string_view msg);
void clearUIError(UIError& error);

void drawMessageError(UIError& error, double duration = 5.0);
void drawTooltipError(UIError& error, std::string tooltipTitle, double duration = 5.0);

}  // namespace sen::components::explorer

#endif  // SEN_COMPONENTS_EXPLORER_SRC_UTIL_H
