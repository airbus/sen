// === log_levels.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_LOG_LEVELS_H
#define SEN_COMPONENTS_TERM_SRC_LOG_LEVELS_H

// spdlog
#include <spdlog/common.h>

// std
#include <string>
#include <string_view>
#include <vector>

namespace sen::components::term
{

/// What term's `log` commands read and change.
///
/// Levels are not about output: `log level debug` changes what the loggers emit, which capturing a
/// descriptor cannot do from outside. Free functions, because there is no state to keep.
///
/// Nothing called from inside these walks may log. `KernelApi::applyToAllLoggers` runs under spdlog's
/// logger-map mutex, which is not recursive, and `getLogger()` reaches `registry::get`, which takes
/// the same one. A single log line inside one of those callbacks deadlocks the thread that owns the
/// display.

/// Sets the global log level, which applies to every logger and to loggers made afterwards.
/// Returns false if the kernel refused the level, which only an out-of-range value can cause.
bool setGlobalLevel(spdlog::level::level_enum level);

/// Sets the level of one logger by name. Returns false if no logger has that name.
bool setLoggerLevel(std::string_view loggerName, spdlog::level::level_enum level);

/// The level every logger is at, read from the kernel on each call so it cannot drift from what the
/// loggers are set to.
[[nodiscard]] spdlog::level::level_enum getGlobalLevel() noexcept;

/// One logger as `log` lists it.
struct LoggerInfo
{
  std::string name;
  spdlog::level::level_enum level;
};

/// Every logger the kernel knows, with its level, sorted by name.
[[nodiscard]] std::vector<LoggerInfo> listLoggers();

/// Parses a level name (trace, debug, info, warn, error, critical, off). Returns true on success and
/// leaves `level` alone on failure.
bool parseLevel(std::string_view name, spdlog::level::level_enum& level);

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_LOG_LEVELS_H
