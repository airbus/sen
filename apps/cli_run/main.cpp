// === main.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/hash32.h"
#include "sen/kernel/bootloader.h"
#include "sen/kernel/kernel.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

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

// platform
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <Windows.h>
#else
#  include <pthread.h>
#endif

// std
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
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

/// Turns a termination request from the operating system into an orderly kernel stop.
///
/// Without this the process keeps the default disposition and dies where it stands, losing whatever
/// it was in the middle of writing. `docker stop`, systemd and ctrl-c all arrive this way.
class SignalStopper
{
public:
  SEN_NOCOPY_NOMOVE(SignalStopper)

  SignalStopper() noexcept;
  ~SignalStopper() noexcept;

public:
  /// Stops the signals killing the process, and must run before anything else in main: until it
  /// does, the default disposition applies and a request arriving takes the process down. It is
  /// separate from watching because the kernel does not exist yet at that point.
  static void blockEarly() noexcept;

  /// Starts watching. The kernel must outlive this object.
  void watch(sen::kernel::Kernel& kernel) noexcept;

  /// True once a request has arrived, so one that lands during start-up is not lost.
  [[nodiscard]] bool signalled() const noexcept { return signalled_.load(); }

private:
  /// Asks the kernel to stop, and keeps asking.
  ///
  /// requestStop is dropped while the kernel is still starting up, and start-up is where a
  /// supervisor's first request often lands. Once one takes, the rest are no-ops.
  void askUntilItTakes(sen::kernel::Kernel& kernel) noexcept;

private:
  std::atomic_bool signalled_ {false};
  std::atomic_bool stopping_ {false};

#if defined(_WIN32)
  /// Windows runs this on a thread of its own, so it may take a lock. A static member rather than a
  /// free function because it reaches the two below.
  static BOOL WINAPI consoleHandler(DWORD event);

  static inline SignalStopper* instance_ {nullptr};
  static inline sen::kernel::Kernel* kernel_ {nullptr};
#else
  sigset_t mask_ {};
  bool armed_ {false};
  std::thread waiter_;

  /// Whether blockEarly succeeded. Static because it runs before any instance exists.
  static inline bool blocked_ {false};
#endif
};

void SignalStopper::askUntilItTakes(sen::kernel::Kernel& kernel) noexcept
{
  signalled_.store(true);
  while (!stopping_.load())
  {
    kernel.requestStop(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
}

#if defined(_WIN32)

/// Nothing to block: windows delivers to a handler on its own thread rather than killing outright.
void SignalStopper::blockEarly() noexcept {}

SignalStopper::SignalStopper() noexcept { instance_ = this; }

SignalStopper::~SignalStopper() noexcept
{
  stopping_.store(true);
  std::ignore = SetConsoleCtrlHandler(consoleHandler, FALSE);
  instance_ = nullptr;
  kernel_ = nullptr;
}

void SignalStopper::watch(sen::kernel::Kernel& kernel) noexcept
{
  kernel_ = &kernel;
  std::ignore = SetConsoleCtrlHandler(consoleHandler, TRUE);
}

/// Returning TRUE says the event is handled; the process then has a few seconds before windows ends
/// it regardless.
BOOL WINAPI SignalStopper::consoleHandler(DWORD event)
{
  if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT && event != CTRL_CLOSE_EVENT && event != CTRL_SHUTDOWN_EVENT)
  {
    return FALSE;
  }

  if (instance_ != nullptr && kernel_ != nullptr)
  {
    instance_->askUntilItTakes(*kernel_);
  }

  return TRUE;
}

#else

namespace
{
/// The signals this process turns into a stop.
void fillTerminationMask(sigset_t& mask) noexcept
{
  sigemptyset(&mask);
  sigaddset(&mask, SIGTERM);
  sigaddset(&mask, SIGINT);
}
}  // namespace

/// Blocking here rather than in the constructor closes the window between the process starting and
/// the kernel being built. Threads inherit the mask, so every thread made later is covered too.
void SignalStopper::blockEarly() noexcept
{
  sigset_t mask;
  fillTerminationMask(mask);
  blocked_ = pthread_sigmask(SIG_BLOCK, &mask, nullptr) == 0;
}

SignalStopper::SignalStopper() noexcept
{
  fillTerminationMask(mask_);
  armed_ = blocked_;
}

SignalStopper::~SignalStopper() noexcept
{
  stopping_.store(true);
  if (waiter_.joinable())
  {
    // The thread is parked in sigwait, so send it the signal it is waiting for.
    std::ignore = pthread_kill(waiter_.native_handle(), SIGTERM);
    waiter_.join();
  }
}

/// A handler may not touch a mutex and requestStop takes one, so a thread waits for the signal
/// instead and calls it as ordinary code.
void SignalStopper::watch(sen::kernel::Kernel& kernel) noexcept
{
  if (!armed_)
  {
    return;
  }

  waiter_ = std::thread(
    [this, &kernel]()
    {
      // Which of the two it was makes no difference: both mean stop.
      int sig = 0;
      if (sigwait(&mask_, &sig) != 0 || stopping_.load())
      {
        return;
      }

      askUntilItTakes(kernel);
    });
}

#endif

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

    // After the kernel, so it is destroyed before it: the watching thread holds a reference to the
    // kernel and has to be joined while that reference is still good.
    SignalStopper stopper;

    stopper.watch(kernel);
    if (stopper.signalled())
    {
      // Asked to stop while we were still loading, so there is nothing to run. Only a shortcut:
      // a request that arrives after this point is caught by the loop instead.
      return 0;
    }

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
  // First, before any thread exists and before anything can take time.
  SignalStopper::blockEarly();

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
