// === logger_impl.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "logger_impl.h"

// generated code
#include "stl/logmaster.stl.h"
#include "stl/sen/kernel/log.stl.h"

// spdlog
#include <spdlog/common.h>
#include <spdlog/logger.h>

// std
#include <memory>
#include <string>
#include <utility>

namespace sen::components::logmaster
{

LoggerImpl::LoggerImpl(const std::string& name, std::shared_ptr<spdlog::logger> logger)
  : LoggerBase(name, {}), logger_(std::move(logger)), level_(logger_->level())
{
  setNextLevel(mapLogLevel(level_));
}

LoggerImpl::~LoggerImpl() = default;

void LoggerImpl::muteImpl()
{
  level_ = logger_->level();
  logger_->set_level(spdlog::level::off);
}

void LoggerImpl::unmuteImpl() { logger_->set_level(level_); }

void LoggerImpl::setLevelImpl(const ::sen::kernel::log::LogLevel& level)
{
  setNextLevel(level);
  level_ = mapLogLevel(level);
  logger_->set_level(level_);
}

void LoggerImpl::setPatternImpl(const std::string& pattern) { logger_->set_pattern(pattern); }

::spdlog::level::level_enum LoggerImpl::mapLogLevel(::sen::kernel::log::LogLevel level) noexcept
{
  switch (level)
  {
    case ::sen::kernel::log::LogLevel::warn:
      return ::spdlog::level::warn;
    case ::sen::kernel::log::LogLevel::err:
      return ::spdlog::level::err;
    case ::sen::kernel::log::LogLevel::critical:
      return ::spdlog::level::critical;
    case ::sen::kernel::log::LogLevel::trace:
      return ::spdlog::level::trace;
    case ::sen::kernel::log::LogLevel::debug:
      return ::spdlog::level::debug;
    case ::sen::kernel::log::LogLevel::info:
      return ::spdlog::level::info;
    case ::sen::kernel::log::LogLevel::off:
      return ::spdlog::level::off;
    default:
      return ::spdlog::level::off;
  }
}

::sen::kernel::log::LogLevel LoggerImpl::mapLogLevel(::spdlog::level::level_enum level) noexcept
{
  switch (level)
  {
    case ::spdlog::level::warn:
      return ::sen::kernel::log::LogLevel::warn;
    case ::spdlog::level::err:
      return ::sen::kernel::log::LogLevel::err;
    case ::spdlog::level::critical:
      return ::sen::kernel::log::LogLevel::critical;
    case ::spdlog::level::trace:
      return ::sen::kernel::log::LogLevel::trace;
    case ::spdlog::level::debug:
      return ::sen::kernel::log::LogLevel::debug;
    case ::spdlog::level::info:
      return ::sen::kernel::log::LogLevel::info;
    case ::spdlog::level::off:
      return ::sen::kernel::log::LogLevel::off;
    default:
      return ::sen::kernel::log::LogLevel::off;
  }
}

}  // namespace sen::components::logmaster
