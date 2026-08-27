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
#include "sen/core/io/util.h"
#include "sen/kernel/bootloader.h"
#include "sen/kernel/kernel.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/log.stl.h"
#include "stl/sen/kernel/network_footprint.stl.h"

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
#include <fstream>
#include <memory>
#include <sstream>
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
  bool printNetworkFootprint = false;
  std::vector<std::string> suppliedBusArguments;
  std::vector<std::filesystem::path> busFiles;
  std::string networkFootprintFormat = "text";
};

/// Removes whitespace from both ends of a string
[[nodiscard]] std::string trimWhitespace(const std::string& text)
{
  constexpr auto whitespace = " \t\r\n";
  const auto first = text.find_first_not_of(whitespace);
  if (first == std::string::npos)
  {
    return {};
  }

  const auto last = text.find_last_not_of(whitespace);
  return text.substr(first, last - first + 1U);
}

/// Parses a non-local session.bus address
[[nodiscard]] sen::kernel::BusAddress parseBusAddress(const std::string& address, const std::string& location = {})
{
  const auto normalizedAddress = trimWhitespace(address);
  const auto elements = sen::impl::split(normalizedAddress, '.');
  const auto hasInvalidFormat = elements.size() != 2U || elements.at(0U).empty() || elements.at(1U).empty();
  const auto isLocal = !hasInvalidFormat && elements.at(0U) == "local";
  if (hasInvalidFormat || isLocal)
  {
    std::string error = "invalid bus '";
    error.append(address);
    error.push_back('\'');
    if (!location.empty())
    {
      error.append(" in ");
      error.append(location);
    }
    error.append(hasInvalidFormat ? "; expected session.bus" : "; local buses have no network footprint");
    sen::throwRuntimeError(error);
  }

  return {elements.at(0U), elements.at(1U)};
}

/// Reads session.bus addresses from a file, ignoring empty lines and comments
[[nodiscard]] std::vector<sen::kernel::BusAddress> readBusFile(const std::filesystem::path& path)
{
  std::ifstream input(path);
  if (!input.is_open())
  {
    sen::throwRuntimeError(std::string("could not open bus file '").append(path.generic_string()).append("'"));
  }

  std::vector<sen::kernel::BusAddress> busAddresses;
  std::string line;
  std::size_t lineNumber = 0U;
  constexpr auto utf8Bom = "\xEF\xBB\xBF";
  constexpr std::size_t utf8BomSize = 3U;
  while (std::getline(input, line))
  {
    ++lineNumber;
    if (lineNumber == 1U && line.compare(0U, utf8BomSize, utf8Bom) == 0)
    {
      line.erase(0U, utf8BomSize);
    }

    if (const auto commentStart = line.find('#'); commentStart != std::string::npos)
    {
      line.erase(commentStart);
    }

    const auto address = trimWhitespace(line);
    if (address.empty())
    {
      continue;
    }

    std::string location = "bus file '";
    location.append(path.generic_string());
    location.append("' at line ");
    location.append(std::to_string(lineNumber));
    busAddresses.push_back(parseBusAddress(address, location));
  }

  if (input.bad())
  {
    std::string error = "could not read bus file '";
    error.append(path.generic_string());
    error.append("' after line ");
    error.append(std::to_string(lineNumber));
    sen::throwRuntimeError(error);
  }
  return busAddresses;
}

/// Collects direct bus arguments followed by buses read from files
[[nodiscard]] std::vector<sen::kernel::BusAddress> collectSuppliedBusAddresses(const RunArgs& args)
{
  std::vector<sen::kernel::BusAddress> busAddresses;
  busAddresses.reserve(args.suppliedBusArguments.size());
  for (const auto& argument: args.suppliedBusArguments)
  {
    busAddresses.push_back(parseBusAddress(argument, "--bus"));
  }

  for (const auto& busFile: args.busFiles)
  {
    const auto fileBusAddresses = readBusFile(busFile);
    busAddresses.insert(busAddresses.end(), fileBusAddresses.begin(), fileBusAddresses.end());
  }
  return busAddresses;
}

/// Disables logging so stdout contains only the json report
void disableLoggingForJsonOutput(sen::kernel::Bootloader* bootloader)
{
  auto params = bootloader->getConfig().getParams();
  params.logConfig.level = sen::kernel::log::LogLevel::off;
  params.logConfig.sinks.clear();
  params.logConfig.loggers.clear();
  bootloader->getConfig().setParams(params);
}

/// remove the new-line when a sequence is empty in a footprint report, so [empty] is shown in the same line
[[nodiscard]] std::string removeEmptyNewline(std::string report)
{
  bool emptySequenceReplaced = false;
  do
  {
    emptySequenceReplaced = replace(report, ":\n[empty]", ": [empty]");
  } while (emptySequenceReplaced);

  return report;
}

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

/// Converts the network footprint into human-readable text or JSON
[[nodiscard]] std::string formatNetworkFootprint(const sen::kernel::NetworkFootprint& footprint,
                                                 const std::string& format)
{
  if (format == "json")
  {
    auto output = sen::SerializationTraits<sen::kernel::NetworkFootprint>::toJsonString(footprint);
    output.push_back('\n');
    return output;
  }

  std::ostringstream output;
  output << footprint << '\n';
  output << "For ephemeral ports (default mode), <empty> means the operating system assigns the port at runtime.\n";
  output << "Configured buses and buses from --bus or --bus-file are included. ";
  output << "Other runtime-created buses are not included.\n";

  return removeEmptyNewline(output.str());
}

[[nodiscard]] int runKernel(const std::shared_ptr<RunArgs>& args, CLI::App& app)
{
  int exitCode = EXIT_FAILURE;
  try
  {
    const auto suppliedBusAddresses = collectSuppliedBusAddresses(*args);
    auto bootloader = makeBootloader(args, app);
    if (args->printNetworkFootprint && args->networkFootprintFormat == "json")
    {
      disableLoggingForJsonOutput(bootloader.get());
    }

    // install the termination handler
    if (!bootloader->getConfig().getParams().crashReportDisabled)
    {
      sen::kernel::Kernel::registerTerminationHandler();
    }
    sen::kernel::Kernel kernel(bootloader->getConfig());

    if (args->printNetworkFootprint)
    {
      const auto footprint = kernel.getNetworkFootprint(suppliedBusAddresses);
      const auto formattedFootprint = formatNetworkFootprint(footprint, args->networkFootprintFormat);
      fputs(formattedFootprint.c_str(), stdout);
      fflush(stdout);
      return EXIT_SUCCESS;
    }

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
  auto startStopOption =
    app.add_flag("--start-stop", args->startStop, "Stop execution after all components are running");
  auto networkFootprintOption =
    app.add_flag("--print-network-footprint", args->printNetworkFootprint, "Print the offline footprint and exit");
  auto printConfigOption =
    app.add_flag("--print-config", args->printConfig, "Print the configuration that will be used");
  networkFootprintOption->excludes(startStopOption);
  networkFootprintOption->excludes(printConfigOption);
  networkFootprintOption->group("Network footprint");

  auto busOption = app.add_option("--bus", args->suppliedBusArguments, "Include a session.bus; repeat to include more");
  busOption->expected(1)->take_all()->needs(networkFootprintOption)->group("Network footprint");

  auto busFileOption =
    app.add_option("--bus-file", args->busFiles, "Read one session.bus per line; # starts a comment");
  busFileOption->expected(1)
    ->take_all()
    ->check(CLI::ExistingFile)
    ->needs(networkFootprintOption)
    ->group("Network footprint");

  auto formatOption =
    app.add_option("--format", args->networkFootprintFormat, "Set output format to readable text or json");
  formatOption->capture_default_str()
    ->check(CLI::IsMember({"text", "json"}))
    ->needs(networkFootprintOption)
    ->group("Network footprint");

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
