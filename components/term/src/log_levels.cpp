// === log_levels.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "log_levels.h"

// sen
#include "sen/kernel/component_api.h"

// spdlog
#include <spdlog/common.h>
#include <spdlog/logger.h>

// std
#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sen::components::term
{

bool setGlobalLevel(spdlog::level::level_enum level)
{
  // The registry's level, not a walk over the loggers that happen to exist: a walk stopped applying
  // the moment another component made a logger.
  return kernel::KernelApi::setAllLoggersLevel(level).isOk();
}

bool setLoggerLevel(std::string_view loggerName, spdlog::level::level_enum level)
{
  // By name over the kernel's view. `spdlog::get` would find the same logger, since spdlog ships
  // shared and the registry is one per process, but the kernel's loggers are the kernel's to hand out.
  bool found = false;
  kernel::KernelApi::applyToAllLoggers(
    [&](const std::shared_ptr<spdlog::logger>& logger)
    {
      if (logger->name() == loggerName)
      {
        logger->set_level(level);
        found = true;
      }
    });
  return found;
}

spdlog::level::level_enum getGlobalLevel() noexcept { return kernel::KernelApi::getAllLoggersLevel(); }

std::vector<LoggerInfo> listLoggers()
{
  std::vector<LoggerInfo> result;
  kernel::KernelApi::applyToAllLoggers([&result](const std::shared_ptr<spdlog::logger>& logger)
                                       { result.push_back({logger->name(), logger->level()}); });

  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
  return result;
}

bool parseLevel(std::string_view name, spdlog::level::level_enum& level)
{
  // from_str accepts the canonical names plus "warn" and "err" and answers off for anything else, so
  // off has to be told apart from unrecognised; on failure the out-param is left alone, which
  // log_levels_test asserts. Case is folded because from_str knows only the lower-case spellings.
  std::string lowered(name);
  std::transform(lowered.begin(),
                 lowered.end(),
                 lowered.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  const auto parsed = spdlog::level::from_str(lowered);
  if (parsed == spdlog::level::off && lowered != "off")
  {
    return false;
  }
  level = parsed;
  return true;
}

}  // namespace sen::components::term
