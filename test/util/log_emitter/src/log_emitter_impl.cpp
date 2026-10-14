// === log_emitter_impl.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "log_emitter_impl.h"

// generated code
#include "stl/log_emitter/log_emitter.stl.h"

// sen
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/component_api.h"

// spdlog
#include <spdlog/common.h>
#include <spdlog/sinks/stdout_color_sinks.h>

// std
#include <cstdio>
#include <iostream>
#include <string>

namespace log_emitter
{

EmitterImpl::EmitterImpl(const std::string& name, const sen::VarMap& args): EmitterBase(name, args)
{
  // Through the kernel, not through spdlog directly. A logger made here with
  // `spdlog::stdout_color_mt` lands in the same registry, but misses what the kernel attaches at
  // creation: the crash reporter's ring.
  logger_ = sen::kernel::KernelApi::getOrCreateLogger("log_emitter." + name);

  // The route the comment above warns against, made on purpose and only when asked for: what a
  // third-party component does when it reaches for spdlog rather than the kernel, giving a logger
  // the kernel never made that writes to the descriptor all the same.
  if (getConfig().output == OutputKind::plainSpdlog)
  {
    plainLogger_ = spdlog::stdout_color_mt("plain." + name);
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
    case OutputKind::stderrOnly:
      emitStderr();
      break;
    case OutputKind::coutOnly:
      emitCout();
      break;
    case OutputKind::printfOnly:
      emitPrintf();
      break;
    case OutputKind::plainSpdlog:
      emitPlainSpdlog();
      break;
    case OutputKind::mixed:
      // Alternate between the kernel's logger and stderr
      if (counter % 2 == 0)
      {
        emitSpdlog();
      }
      else
      {
        emitStderr();
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

void EmitterImpl::emitStderr()
{
  auto counter = getCounter();
  const auto& prefix = getConfig().prefix;

  auto message = prefix + " stderr line #" + std::to_string(counter);
  std::cerr << message << std::endl;
  setNextLastMessage(message);
  messageEmitted(message, "stderr");
}

void EmitterImpl::emitCout()
{
  auto counter = getCounter();
  const auto& prefix = getConfig().prefix;

  // Buffered, unlike cerr, so it reaches the descriptor in blocks rather than per line. That
  // difference is visible as a flicker rather than as clean lines when the renderer owns fd 1.
  auto message = prefix + " cout line #" + std::to_string(counter);
  std::cout << message << std::endl;
  setNextLastMessage(message);
  messageEmitted(message, "cout");
}

void EmitterImpl::emitPrintf()
{
  auto counter = getCounter();
  const auto& prefix = getConfig().prefix;

  // Separate from cout because it reaches fd 1 through the C stdout FILE*, which is the route a
  // spdlog console sink takes. A capture that catches one and not the other would look like it
  // works.
  auto message = prefix + " printf line #" + std::to_string(counter);
  std::printf("%s\n", message.c_str());  // NOLINT(hicpp-vararg)
  std::fflush(stdout);
  setNextLastMessage(message);
  messageEmitted(message, "printf");
}

void EmitterImpl::emitPlainSpdlog()
{
  auto counter = getCounter();
  const auto& prefix = getConfig().prefix;

  auto message = prefix + " plain spdlog line #" + std::to_string(counter);
  plainLogger_->info(message);
  setNextLastMessage(message);
  messageEmitted(message, "info");
}

SEN_EXPORT_CLASS(EmitterImpl)

}  // namespace log_emitter
