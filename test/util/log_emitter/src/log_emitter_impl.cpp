// === log_emitter_impl.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "log_emitter_impl.h"

// generated code
#include "stl/log_emitter/log_emitter.stl.h"

// spdlog
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

// std
#include <cstdio>
#include <iostream>
#include <string>

namespace log_emitter
{

EmitterImpl::EmitterImpl(const std::string& name, const sen::VarMap& args): EmitterBase(name, args)
{
  // Create a dedicated logger for this emitter using spdlog's factory function,
  // which attaches a stderr sink and registers it in the global registry.
  auto loggerName = "log_emitter." + name;
  logger_ = spdlog::get(loggerName);
  if (!logger_)
  {
    logger_ = spdlog::stdout_color_mt(loggerName);
  }

  // Set emission interval based on configured rate
  switch (getConfig().rate)
  {
    case EmitRate::slow:
      emitInterval_ = 150;  // ~5s at 30Hz
      break;
    case EmitRate::moderate:
      emitInterval_ = 30;  // ~1s at 30Hz
      break;
    case EmitRate::fast:
      emitInterval_ = 6;  // ~0.2s at 30Hz
      break;
  }

  setNextCounter(0);
  setNextLastMessage("");
}

void EmitterImpl::update(sen::kernel::RunApi& /*runApi*/)
{
  ++cycleCount_;

  auto counter = getCounter() + 1;
  setNextCounter(counter);

  const auto& config = getConfig();

  switch (config.output)
  {
    case OutputKind::spdlogOnly:
      emitSpdlog();
      break;
    case OutputKind::stdoutOnly:
      emitStdout();
      break;
    case OutputKind::mixed:
      // Alternate between spdlog and stdout
      if (counter % 2 == 0)
      {
        emitSpdlog();
      }
      else
      {
        emitStdout();
      }
      break;
  }
}

void EmitterImpl::emitSpdlog()
{
  auto counter = getCounter();
  const auto& prefix = getConfig().prefix;

  // Cycle through different log levels
  auto levelIndex = counter % 6;
  std::string message;

  switch (levelIndex)
  {
    case 0:
      message = prefix + " trace message #" + std::to_string(counter);
      logger_->trace(message);
      break;
    case 1:
      message = prefix + " debug message #" + std::to_string(counter);
      logger_->debug(message);
      break;
    case 2:
      message = prefix + " info message #" + std::to_string(counter);
      logger_->info(message);
      break;
    case 3:
      message = prefix + " warning message #" + std::to_string(counter);
      logger_->warn(message);
      break;
    case 4:
      message = prefix + " error message #" + std::to_string(counter);
      logger_->error(message);
      break;
    case 5:
      message = prefix + " critical message #" + std::to_string(counter);
      logger_->critical(message);
      break;
    default:
      break;
  }

  setNextLastMessage(message);
  messageEmitted(message,
                 std::string(spdlog::level::to_string_view(static_cast<spdlog::level::level_enum>(levelIndex)).data()));
}

void EmitterImpl::emitStdout()
{
  auto counter = getCounter();
  const auto& prefix = getConfig().prefix;

  auto message = prefix + " stderr line #" + std::to_string(counter);
  std::cerr << message << std::endl;
  setNextLastMessage(message);
  messageEmitted(message, "stderr");
}

SEN_EXPORT_CLASS(EmitterImpl)

}  // namespace log_emitter
