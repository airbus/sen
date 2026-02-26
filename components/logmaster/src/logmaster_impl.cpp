// === logmaster_impl.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "logmaster_impl.h"

// sen
#include "sen/core/obj/native_object.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/sen/kernel/log.stl.h"

// spdlog
#include <spdlog/details/registry.h>
#include <spdlog/logger.h>

// std
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace sen::components::logmaster
{

//--------------------------------------------------------------------------------------------------------------
// helpers
//--------------------------------------------------------------------------------------------------------------

[[nodiscard]] std::string getLoggerName(const std::shared_ptr<spdlog::logger>& logger)
{
  return logger->name().empty() ? "default" : logger->name();
}

//--------------------------------------------------------------------------------------------------------------
// LogMasterImpl
//--------------------------------------------------------------------------------------------------------------

LogMasterImpl::~LogMasterImpl()
{
  // collect the logger objects that we have created so far
  std::vector<std::shared_ptr<::sen::NativeObject>> loggerObjects;
  {
    loggerObjects.reserve(loggers_.size());

    for (auto& [name, logger]: loggers_)
    {
      loggerObjects.push_back(logger);
    }
  }

  // remove them from the bus
  targetBus_->remove(loggerObjects);
}

void LogMasterImpl::registered(kernel::RegistrationApi& api)
{
  // connect to the target bus and check for any existing loggers
  targetBus_ = api.getSource(getTargetBus());
  checkForLoggers();
}

void LogMasterImpl::update(kernel::RunApi& runApi)
{
  std::ignore = runApi;

  // wait a bit, to give chance to the other components to create their loggers
  if (cyclesLeftForCheck_ == 10)
  {
    checkForLoggers();
  }
  else if (cyclesLeftForCheck_ < 10)
  {
    ++cyclesLeftForCheck_;
  }
}

void LogMasterImpl::muteAllImpl()
{
  forAllLoggers([](auto& logger) { logger->mute(); });
  muted_ = true;
}

void LogMasterImpl::unmuteAllImpl()
{
  forAllLoggers([](auto& logger) { logger->unmute(); });
  muted_ = false;
}

void LogMasterImpl::toggleMuteAllImpl()
{
  if (muted_)
  {
    unmuteAllImpl();
  }
  else
  {
    muteAllImpl();
  }
}

void LogMasterImpl::setLevelImpl(const ::sen::kernel::log::LogLevel& level)
{
  forAllLoggers([level](auto& logger) { logger->setLevel(level); });
}

void LogMasterImpl::checkForLoggers()
{
  auto& reg = spdlog::details::registry::instance();

  reg.apply_all(
    [this](auto logger)
    {
      const auto name = getLoggerName(logger);

      // ensure the logger object is not repeated
      if (loggers_.find(name) == loggers_.end())
      {
        auto loggerObject = std::make_shared<LoggerImpl>(name, logger);
        loggers_.emplace(name, loggerObject);
        targetBus_->add(loggerObject);
      }
    });
}

template <typename F>
void LogMasterImpl::forAllLoggers(F&& func)
{
  for (auto& [name, logger]: loggers_)
  {
    func(logger);
  }
}

}  // namespace sen::components::logmaster
