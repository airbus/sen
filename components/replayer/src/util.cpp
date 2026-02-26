// === util.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "util.h"

// spdlog
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

// std
#include <memory>

namespace sen::components::replayer
{

std::shared_ptr<spdlog::logger> getLogger()
{
  constexpr auto* loggerName = "replayer";
  static auto logger = spdlog::get(loggerName);
  if (!logger)
  {
    logger = spdlog::stdout_color_st(loggerName);
    logger->flush_on(spdlog::level::debug);
  }

  return logger;
}

}  // namespace sen::components::replayer
