// === self_armed_crash.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// An embedder that arms crash reporting itself, which `sen run` never does with
// crashReportDisabled set: there the key is honoured by not arming at all.
//
// Usage: self_armed_crash <report directory> enabled|disabled

#include "sen/core/base/span.h"
#include "sen/kernel/kernel.h"
#include "sen/kernel/kernel_config.h"
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>

// The uncaught exception is the point: it is what reaches the terminate handler arm() installed.
// NOLINTNEXTLINE(bugprone-exception-escape)
int main(int argc, char* argv[])
{
  if (const auto handlerExit = sen::kernel::crash::runHandlerIfRequested(argc, argv); handlerExit.has_value())
  {
    return *handlerExit;
  }

  const sen::Span<char*> args(argv, static_cast<std::size_t>(argc));
  if (args.size() != 3U)
  {
    std::cerr << "usage: self_armed_crash <report directory> enabled|disabled\n";
    return EXIT_FAILURE;
  }

  const std::filesystem::path reportDirectory = args[1];
  const auto mode = std::string_view(args[2]);
  if (mode != "enabled" && mode != "disabled")
  {
    std::cerr << "the second argument must be 'enabled' or 'disabled'\n";
    return EXIT_FAILURE;
  }

  if (!sen::kernel::crash::arm(reportDirectory))
  {
    std::cerr << "the crash handler is not running, so this run would prove nothing\n";
    return EXIT_FAILURE;
  }

  sen::kernel::KernelParams params;
  params.runMode = sen::kernel::RunMode::startAndStop;
  params.appName = "self_armed_crash";
  params.crashReportDir = reportDirectory.string();
  params.crashReportDisabled = mode == "disabled";

  sen::kernel::KernelConfig config;
  config.setParams(params);

  sen::kernel::Kernel kernel(config);
  std::ignore = kernel.run();

  throw std::runtime_error("self_armed_crash is meant to end here");
}
