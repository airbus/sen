// === main.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// generated
#include "builtin_configs/explorer.h"
#include "builtin_configs/replay.h"
#include "builtin_configs/shell.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/hash32.h"
#include "sen/kernel/bootloader.h"
#include "sen/kernel/kernel.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// cli11
#include <CLI/App.hpp>
#include <CLI/CLI.hpp>  // NOLINT (misc-include-cleaner): to correctly link
#include <CLI/Validators.hpp>

// os
#ifdef WIN32
#  include <windows.h>
#endif

// std
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

[[nodiscard]] bool replace(std::string& str, const std::string& from, const std::string& to)
{
  size_t startPos = str.find(from);
  if (startPos == std::string::npos)
  {
    return false;
  }
  str.replace(startPos, from.length(), to);
  return true;
}

struct RunArgs
{
  std::filesystem::path configFile;
  bool startStop = false;
  std::string preset;
  bool printConfig = false;
};

void applyCustomConfiguration(sen::kernel::Bootloader* bootloader, const std::shared_ptr<RunArgs>& args)
{
  if (args->startStop)
  {
    auto params = bootloader->getConfig().getParams();
    params.runMode = sen::kernel::RunMode::startAndStop;
    bootloader->getConfig().setParams(params);
  }
}

std::unique_ptr<sen::kernel::Bootloader> makeBootloader(const std::shared_ptr<RunArgs>& args, CLI::App& app)
{
  if (args->preset.empty())
  {
    auto bootloader = sen::kernel::Bootloader::fromYamlFile(args->configFile, args->printConfig);

    // use the config file name as application name, if not specified
    if (bootloader->getConfig().getParams().appName.empty())
    {
      auto params = bootloader->getConfig().getParams();
      params.appName = args->configFile.stem().generic_string();
      bootloader->getConfig().setParams(params);
    }

    applyCustomConfiguration(bootloader.get(), args);

    return bootloader;
  }

  std::string presetContents;
  if (args->preset == "shell")
  {
    presetContents = sen::decompressSymbolToString(shell, shellSize);
  }
  else if (args->preset == "explorer")
  {
    presetContents = sen::decompressSymbolToString(explorer, explorerSize);
  }
  else if (args->preset == "replay")
  {
    bool autoPlay = true;
    std::string autoOpen = args->configFile.string();

    auto remaining = app.remaining();
    for (const auto& elem: remaining)
    {
      if (elem == "--stopped")
      {
        autoPlay = false;
      }
    }

    presetContents = sen::decompressSymbolToString(replay, replaySize);
    std::ignore = replace(presetContents, "$autoOpen", autoOpen);
    std::ignore = replace(presetContents, "$autoPlay", autoPlay ? "true" : "false");
  }
  else
  {
    std::string err;
    err.append("invalid preset '");
    err.append(args->preset);
    err.append("'");
    sen::throwRuntimeError(err);
  }

  auto bootloader = sen::kernel::Bootloader::fromYamlString(presetContents, args->printConfig);
  applyCustomConfiguration(bootloader.get(), args);
  return bootloader;
}

[[nodiscard]] int runKernel(const std::shared_ptr<RunArgs>& args, CLI::App& app)
{
  int exitCode = EXIT_FAILURE;
  try
  {
    auto bootloader = makeBootloader(args, app);

    // install the termination handler
    if (!bootloader->getConfig().getParams().crashReportDisabled)
    {
      sen::kernel::Kernel::registerTerminationHandler();
    }
    sen::kernel::Kernel kernel(bootloader->getConfig());
    exitCode = kernel.run();
  }
  catch (const std::runtime_error& err)
  {
    fputs("Runtime error: ", stderr);
    fputs(err.what(), stderr);
    fputc('\n', stderr);
    fflush(stderr);
    return 1;
  }
  catch (const std::logic_error& err)
  {
    fputs("Implementation error: ", stderr);
    fputs(err.what(), stderr);
    fputc('\n', stderr);
    fflush(stderr);
    return 2;
  }
  catch (const std::exception& err)
  {
    fputs("Error: ", stderr);
    fputs(err.what(), stderr);
    fputc('\n', stderr);
    fflush(stderr);
    return 3;
  }
  catch (...)
  {
    fputs("Unknown error\n", stderr);
    fflush(stderr);
    return 4;
  }

  if (exitCode != EXIT_SUCCESS)
  {
    fputs("bye :(\n", stderr);
    fprintf(stderr, "** exit code %d **\n", exitCode);  // NOLINT
    return exitCode;
  }

  fputs("bye ", stdout);
#if _WIN32
  auto oldOutCp = GetConsoleOutputCP();
  SetConsoleOutputCP(CP_UTF8);

  auto oldCp = GetConsoleCP();
  SetConsoleCP(CP_UTF8);

  std::string happy = u8"\u263A";
  fwrite(happy.data(), 1U, happy.length(), stdout);

  SetConsoleCP(oldCp);
  SetConsoleOutputCP(oldOutCp);
#else
  std::string happy = "\u263A";
  fwrite(happy.data(), 1U, happy.length(), stdout);
#endif
  fputs("\n", stdout);
  return 0;
}

int runApp(int argc, char* argv[])
{
  auto args = std::make_shared<RunArgs>();

  CLI::App app {"Run a sen kernel\n"};
  app.name("sen run");
  app.allow_extras();
  app.get_formatter()->column_width(35);

  app.add_option("config", args->configFile, "Configuration file")->check(CLI::ExistingPath);
  app.add_option("--preset", args->preset, "Preset name")->check(CLI::IsMember({"shell", "explorer", "replay"}));
  app.add_flag("--start-stop", args->startStop, "Stop execution after all components are running");
  app.add_flag("--print-config", args->printConfig, "Print the configuration that will be used");

  CLI11_PARSE(app, argc, argv)

  return runKernel(args, app);
}

int main(int argc, char* argv[])
{
  try
  {
    return runApp(argc, argv);
  }
  catch (const std::exception& err)
  {
    fprintf(stderr, "Error detected: %s\n", err.what());  // NOLINT
    return 1;
  }
  catch (...)
  {
    fputs("Unknown error detected\n", stderr);
    return 1;
  }
}
