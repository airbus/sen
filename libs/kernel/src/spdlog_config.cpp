// === spdlog_config.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "./spdlog_config.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"

// generated code
#include "stl/sen/kernel/log.stl.h"

// spdlog
#include <spdlog/common.h>
#include <spdlog/details/registry.h>
#include <spdlog/formatter.h>
#include <spdlog/logger.h>
#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

// std
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

// os
#ifdef __linux__
#  include <spdlog/sinks/syslog_sink.h>
#endif

// std
#include <filesystem>
#include <set>
#include <string>

namespace sen::kernel::impl
{

namespace
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

[[nodiscard]] inline ::spdlog::level::level_enum mapLogLevel(log::LogLevel level) noexcept
{
  switch (level)
  {
    case log::LogLevel::warn:
      return ::spdlog::level::warn;
    case log::LogLevel::err:
      return ::spdlog::level::err;
    case log::LogLevel::critical:
      return ::spdlog::level::critical;
    case log::LogLevel::trace:
      return ::spdlog::level::trace;
    case log::LogLevel::debug:
      return ::spdlog::level::debug;
    case log::LogLevel::info:
      return ::spdlog::level::info;
    case log::LogLevel::off:
      return ::spdlog::level::off;
    default:
      return ::spdlog::level::off;
  }
}

using SinkMap = std::map<std::string, std::shared_ptr<::spdlog::sinks::sink>, std::less<>>;

SinkMap createSinks(const log::Config& config, const spdlog::details::registry& registry)
{
  std::ignore = registry;
  std::map<std::string, std::shared_ptr<::spdlog::sinks::sink>, std::less<>> result;

  for (const auto& sinkConfig: config.sinks)
  {
    auto sink = std::visit(
      Overloaded {[&sinkConfig](const log::Stdout& elem) -> std::shared_ptr<::spdlog::sinks::sink>
                  {
                    std::ignore = elem;
                    if (sinkConfig.singleThreaded)
                    {
                      return std::make_shared<::spdlog::sinks::stdout_sink_st>();
                    }
                    return std::make_shared<::spdlog::sinks::stdout_sink_mt>();
                  },
                  [&sinkConfig](const log::Stderr& elem) -> std::shared_ptr<::spdlog::sinks::sink>
                  {
                    std::ignore = elem;
                    if (sinkConfig.singleThreaded)
                    {
                      return std::make_shared<::spdlog::sinks::stderr_sink_st>();
                    }
                    return std::make_shared<::spdlog::sinks::stderr_sink_mt>();
                  },
                  [&sinkConfig](const log::ColorStdout& elem) -> std::shared_ptr<::spdlog::sinks::sink>
                  {
                    std::ignore = elem;
                    if (sinkConfig.singleThreaded)
                    {
                      return std::make_shared<::spdlog::sinks::stdout_color_sink_st>();
                    }
                    return std::make_shared<::spdlog::sinks::stdout_color_sink_mt>();
                  },
                  [&sinkConfig](const log::ColorStderr& elem) -> std::shared_ptr<::spdlog::sinks::sink>
                  {
                    std::ignore = elem;
                    if (sinkConfig.singleThreaded)
                    {
                      return std::make_shared<::spdlog::sinks::stderr_color_sink_st>();
                    }
                    return std::make_shared<::spdlog::sinks::stderr_color_sink_mt>();
                  },
                  [&sinkConfig](const log::BasicFile& elem) -> std::shared_ptr<::spdlog::sinks::sink>
                  {
                    if (elem.createParentDir)
                    {
                      std::filesystem::path path(elem.fileName);
                      if (path.has_parent_path())
                      {
                        std::filesystem::create_directories(path.parent_path());
                      }
                    }

                    if (sinkConfig.singleThreaded)
                    {
                      return std::make_shared<::spdlog::sinks::basic_file_sink_st>(elem.fileName, elem.truncate);
                    }
                    return std::make_shared<::spdlog::sinks::basic_file_sink_mt>(elem.fileName, elem.truncate);
                  },
                  [&sinkConfig](const log::RotatingFile& elem) -> std::shared_ptr<::spdlog::sinks::sink>
                  {
                    if (sinkConfig.singleThreaded)
                    {
                      return std::make_shared<::spdlog::sinks::rotating_file_sink_st>(
                        elem.baseFileName, elem.maxSize, elem.maxFiles);
                    }
                    return std::make_shared<::spdlog::sinks::rotating_file_sink_mt>(
                      elem.baseFileName, elem.maxSize, elem.maxFiles);
                  },
                  [&sinkConfig](const log::Null& elem) -> std::shared_ptr<::spdlog::sinks::sink>
                  {
                    std::ignore = elem;
                    if (sinkConfig.singleThreaded)
                    {
                      return std::make_shared<::spdlog::sinks::null_sink_st>();
                    }
                    return std::make_shared<::spdlog::sinks::null_sink_mt>();
                  },
                  [&sinkConfig](const log::Syslog& elem) -> std::shared_ptr<::spdlog::sinks::sink>
                  {
#ifdef __linux__
                    constexpr int syslogFacility = 1;  // user
                    if (sinkConfig.singleThreaded)
                    {
                      return std::make_shared<::spdlog::sinks::syslog_sink_st>(
                        elem.identity, elem.option, syslogFacility, elem.enableFormatting);
                    }
                    return std::make_shared<::spdlog::sinks::syslog_sink_mt>(
                      elem.identity, elem.option, syslogFacility, elem.enableFormatting);
#else
                    std::ignore = elem;
                    std::ignore = sinkConfig;
                    return std::make_shared<::spdlog::sinks::null_sink_st>();
#endif
                  }},
      sinkConfig.config);

    sink->set_level(mapLogLevel(sinkConfig.level));
    result.try_emplace(sinkConfig.name, sink);
  }

  return result;
}

void registerLoggers(const log::Config& config, const SinkMap& sinkMap, spdlog::details::registry& registry)
{
  for (const auto& loggerConfig: config.loggers)
  {
    std::vector<std::shared_ptr<::spdlog::sinks::sink>> sinks;
    sinks.reserve(loggerConfig.sinks.size());
    for (auto& sinkName: loggerConfig.sinks)
    {
      sinks.push_back(sinkMap.find(sinkName)->second);
    }

    auto logger = ::spdlog::get(loggerConfig.name);
    if (!logger)
    {
      logger = std::make_shared<::spdlog::logger>(loggerConfig.name, sinks.begin(), sinks.end());
    }

    if (!loggerConfig.pattern.empty())
    {
      logger->set_pattern(loggerConfig.pattern);
    }

    logger->set_level(mapLogLevel(loggerConfig.level));
    registry.register_logger(logger);
  }
}

void validate(const log::Config& config)
{
  std::set<std::string, std::less<>> readSinks;
  for (const auto& sinkConfig: config.sinks)
  {
    if (sinkConfig.name.empty())
    {
      throwRuntimeError("empty sink name");
    }

    if (readSinks.count(sinkConfig.name) != 0)
    {
      throwRuntimeError("repeated sink name " + sinkConfig.name);
    }

    readSinks.insert(sinkConfig.name);
  }

  {
    std::set<std::string, std::less<>> readLoggers;
    for (const auto& loggerConfig: config.loggers)
    {
      if (loggerConfig.name.empty())
      {
        throwRuntimeError("empty logger name");
      }

      if (readLoggers.count(loggerConfig.name) != 0)
      {
        throwRuntimeError("repeated logger name " + loggerConfig.name);
      }

      readLoggers.insert(loggerConfig.name);

      for (const auto& sinkName: loggerConfig.sinks)
      {
        if (readSinks.count(sinkName) == 0)
        {
          throwRuntimeError("undefined sink named " + sinkName + " for logger " + loggerConfig.name);
        }
      }
    }
  }
}

}  // namespace

void configureSpdlog(const log::Config& config)
{
  constexpr std::size_t loggingBacktraceSize = 2048;
  spdlog::enable_backtrace(loggingBacktraceSize);

  auto& reg = spdlog::details::registry::instance();

  if (!config.pattern.empty())
  {
    reg.set_formatter(std::unique_ptr<spdlog::formatter>(                                 // NOSONAR
      new spdlog::pattern_formatter(config.pattern, spdlog::pattern_time_type::local)));  // NOSONAR
  }

  // check that its valid
  validate(config);

  // apply the config
  reg.set_level(mapLogLevel(config.level));
  auto sinks = createSinks(config, reg);
  registerLoggers(config, sinks, reg);
}

}  // namespace sen::kernel::impl
