// === main.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/hash32.h"
#include "sen/kernel/bootloader.h"
#include "sen/kernel/kernel.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <stdexcept>

#ifdef SEN_CLI_RUN_HAS_SHELL_PRESET
#  include "builtin_configs/shell.h"
#endif
#ifdef SEN_CLI_RUN_HAS_REPLAY_PRESET
#  include "builtin_configs/replay.h"
#endif
#ifdef SEN_CLI_RUN_HAS_EXPLORER_PRESET
#  include "builtin_configs/explorer.h"
#endif
#ifdef SEN_CLI_RUN_HAS_WEBEXPLORER_PRESET
#  include "browser_open.h"
#  include "builtin_configs/webexplorer.h"
#endif

// cli11
#include <CLI/App.hpp>
#include <CLI/CLI.hpp>  // NOLINT (misc-include-cleaner): to correctly link
#include <CLI/Validators.hpp>

// std
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>

namespace
{

constexpr std::string_view happyFace = "☺";  // U+263A WHITE SMILING FACE

struct RunArgs
{
  std::filesystem::path configFile;
  bool startStop = false;
  std::string preset;
  bool printConfig = false;
  bool noBrowser = false;
};

constexpr auto webExplorerUrl = "http://127.0.0.1:8080/explorer/";
constexpr auto webExplorerHost = "127.0.0.1";
constexpr int webExplorerPort = 8080;
constexpr auto webExplorerReadyTimeout = std::chrono::seconds(15);

[[nodiscard]] bool replace(std::string& str, std::string_view from, std::string_view to)
{
  const auto startPos = str.find(from);
  if (startPos == std::string::npos)
  {
    return false;
  }
  str.replace(startPos, from.length(), to);
  return true;
}

std::unique_ptr<sen::kernel::Bootloader> makeBootloader(const std::shared_ptr<RunArgs>& args,
                                                        [[maybe_unused]] CLI::App& app)
{
  if (args->preset.empty())
  {
    auto bootloader = sen::kernel::Bootloader::fromYamlFile(args->configFile, args->printConfig);

    if (bootloader->getConfig().getParams().appName.empty())
    {
      auto params = bootloader->getConfig().getParams();
      params.appName = args->configFile.stem().generic_string();
      bootloader->getConfig().setParams(params);
    }

    if (args->startStop)
    {
      auto params = bootloader->getConfig().getParams();
      params.runMode = sen::kernel::RunMode::startAndStop;
      bootloader->getConfig().setParams(params);
    }
    return bootloader;
  }

  std::string presetContents;
  bool presetMatched = false;
#ifdef SEN_CLI_RUN_HAS_SHELL_PRESET
  if (args->preset == "shell")
  {
    presetContents = sen::decompressSymbolToString(shell, shellSize);
    presetMatched = true;
  }
#endif
#ifdef SEN_CLI_RUN_HAS_EXPLORER_PRESET
  if (!presetMatched && args->preset == "explorer")
  {
    presetContents = sen::decompressSymbolToString(explorer, explorerSize);
    presetMatched = true;
  }
#endif
#ifdef SEN_CLI_RUN_HAS_WEBEXPLORER_PRESET
  if (!presetMatched && args->preset == "web-explorer")
  {
    presetContents = sen::decompressSymbolToString(webexplorer, webexplorerSize);
    presetMatched = true;
  }
#endif
#ifdef SEN_CLI_RUN_HAS_REPLAY_PRESET
  if (!presetMatched && args->preset == "replay")
  {
    bool autoPlay = true;
    const auto autoOpen = args->configFile.string();

    for (const auto& elem: app.remaining())
    {
      if (elem == "--stopped")
      {
        autoPlay = false;
      }
    }

    presetContents = sen::decompressSymbolToString(replay, replaySize);
    std::ignore = replace(presetContents, "$autoOpen", autoOpen);
    std::ignore = replace(presetContents, "$autoPlay", autoPlay ? "true" : "false");
    presetMatched = true;
  }
#endif
  if (!presetMatched)
  {
    std::string err;
    err.append("invalid preset '");
    err.append(args->preset);
    err.append("'");
    sen::throwRuntimeError(err);
  }

  auto bootloader = sen::kernel::Bootloader::fromYamlString(presetContents, args->printConfig);
  if (args->startStop)
  {
    auto params = bootloader->getConfig().getParams();
    params.runMode = sen::kernel::RunMode::startAndStop;
    bootloader->getConfig().setParams(params);
  }
  return bootloader;
}

// Exit codes:
//   0       success
//   1       std::runtime_error escaped from the kernel
//   2       std::logic_error escaped from the kernel
//   3       other std::exception escaped from the kernel
//   4       unknown exception escaped from the kernel
//   other   kernel.run() returned a non-zero exit code (kernel-defined)
[[nodiscard]] int runKernel(const std::shared_ptr<RunArgs>& args, CLI::App& app)
{
  int exitCode = EXIT_FAILURE;
  try
  {
    auto bootloader = makeBootloader(args, app);

    if (!bootloader->getConfig().getParams().crashReportDisabled)
    {
      sen::kernel::Kernel::registerTerminationHandler();
    }
    sen::kernel::Kernel kernel(bootloader->getConfig());

#ifdef SEN_CLI_RUN_HAS_WEBEXPLORER_PRESET
    // Detached helper for the web-explorer preset: wait for the jsonrpc listener to come
    // up, then hand the URL to the user's default browser. Print the URL unconditionally
    // so a headless user (no display, --no-browser, or launcher failure) still sees it.
    if (args->preset == "web-explorer")
    {
      std::cout << "Web Explorer at " << webExplorerUrl << '\n' << std::flush;
      if (!args->noBrowser)
      {
        std::thread(
          []
          {
            if (sen::cli_run::waitForTcpListening(webExplorerHost, webExplorerPort, webExplorerReadyTimeout))
            {
              sen::cli_run::openInBrowser(webExplorerUrl);
            }
          })
          .detach();
      }
    }
#endif

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
  fwrite(happyFace.data(), 1U, happyFace.size(), stdout);
  fputc('\n', stdout);
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
#if defined(SEN_CLI_RUN_HAS_SHELL_PRESET) || defined(SEN_CLI_RUN_HAS_REPLAY_PRESET) ||                                 \
  defined(SEN_CLI_RUN_HAS_EXPLORER_PRESET) || defined(SEN_CLI_RUN_HAS_WEBEXPLORER_PRESET)
  app.add_option("--preset", args->preset, "Preset name")
    ->check(CLI::IsMember({
#  ifdef SEN_CLI_RUN_HAS_SHELL_PRESET
      "shell",
#  endif
#  ifdef SEN_CLI_RUN_HAS_REPLAY_PRESET
      "replay",
#  endif
#  ifdef SEN_CLI_RUN_HAS_EXPLORER_PRESET
      "explorer",
#  endif
#  ifdef SEN_CLI_RUN_HAS_WEBEXPLORER_PRESET
      "web-explorer",
#  endif
    }));
#endif
  app.add_flag("--start-stop", args->startStop, "Stop execution after all components are running");
#ifdef SEN_CLI_RUN_HAS_WEBEXPLORER_PRESET
  app.add_flag("--no-browser", args->noBrowser, "With --preset web-explorer: don't auto-open the URL in a browser");
#endif
  app.add_flag("--print-config", args->printConfig, "Print the configuration that will be used");

  CLI11_PARSE(app, argc, argv)

  return runKernel(args, app);
}

}  // namespace

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
