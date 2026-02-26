// === main.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/base/hash32.h"
#include "sen/core/base/version.h"

// cli11
#include <CLI/App.hpp>
// NOLINTNEXTLINE (misc-include-cleaner): cli11 needs all headers to correctly link required vtables
#include <CLI/CLI.hpp>
#include <CLI/Error.hpp>
#include <CLI/Validators.hpp>

// system
#ifdef __linux__
#  include <linux/limits.h>
#  include <sys/types.h>
#endif

// std
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#ifdef WIN32
#  include "windows.h"

#  include <process.h>
#else
#  include <unistd.h>
#endif

void runChildProcess(const char* binPath, int argc, char* argv[])
{
  std::vector<std::string> cmdLine;
  cmdLine.reserve(argc - 1);
  cmdLine.emplace_back(binPath);
  for (int i = 2; i < argc; ++i)
  {
    cmdLine.emplace_back(argv[i]);  // NOLINT
  }

  std::vector<char*> cmdVec;
  cmdVec.reserve(cmdLine.size());
  for (auto& str: cmdLine)
  {
    cmdVec.push_back(str.data());
  }
  cmdVec.push_back(nullptr);

#ifdef WIN32
  auto rc = _spawnv(_P_WAIT, binPath, cmdVec.data());
  exit(rc);
#else
  execv(binPath, cmdVec.data());
#endif
}

std::filesystem::path getExePath()
{
#ifdef _WIN32
  wchar_t path[MAX_PATH] = {0};
  GetModuleFileNameW(NULL, path, MAX_PATH);
  return path;
#else
  char result[PATH_MAX];
  ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
  return std::string(result, (count > 0) ? count : 0);
#endif
}

void configureSubcommand(CLI::App& app,
                         const char* name,
                         const char* description,
                         const char* binPath,
                         int argc,
                         char* argv[])
{
  auto cmd = app.add_subcommand(name, description);
  cmd->allow_extras();
  cmd->set_help_flag();
  cmd->add_flag("-h,--help", "Request help");
  cmd->callback(
    [=]()
    {
      auto bin = getExePath().parent_path() / binPath;
      runChildProcess(bin.generic_string().c_str(), argc, argv);
    });
}

void configureFileToArray(CLI::App& app)
{
  struct Args
  {
    std::filesystem::path inputFile;
    std::filesystem::path outputFile;
    std::string varName;
  };
  std::shared_ptr<Args> args = std::make_shared<Args>();

  auto cmd = app.add_subcommand("fileToArray", "simple utility to convert a file to a C++ array");

  cmd->add_option("-i", args->inputFile, "File to read")->required()->check(CLI::ExistingFile);
  cmd->add_option("-o", args->outputFile, "File to write")->required();
  cmd->add_option("-v", args->varName, "Name of the variable that will hold the data")->required();
  cmd->callback(
    [args]()
    {
      if (!sen::fileToCompressedArrayFile(args->inputFile, args->varName, args->outputFile))
      {
        std::cerr << "error compressing data\n";
        exit(1);
      }
    });
}

void configureShortcut(CLI::App& app, const char* preset, int argc, char* argv[])
{
  std::string description;
  description.append("shortcut to run the sen ");
  description.append(preset);
  description.append(" stand-alone");

  auto cmd = app.add_subcommand(preset, description);
  cmd->allow_extras();
  cmd->set_help_flag();

  std::vector<const char*> ourArgv;
  ourArgv.reserve(argc + 2U);
  ourArgv.push_back(argv[0]);  // NOLINT
  ourArgv.push_back("run");
  ourArgv.push_back("--preset");
  ourArgv.push_back(preset);

  // add the remaining arguments
  for (int i = 2; i < argc; ++i)
  {
    ourArgv.push_back(argv[i]);  // NOLINT
  }

  cmd->callback(
    [=]()
    {
      auto bin = getExePath().parent_path() / "cli_run";
      runChildProcess(
        bin.string().c_str(), static_cast<int>(ourArgv.size()), const_cast<char**>(ourArgv.data()));  // NOLINT
    });
}

void configureShortcuts(CLI::App& app, int argc, char* argv[])
{
  configureShortcut(app, "shell", argc, argv);
  configureShortcut(app, "term", argc, argv);
  configureShortcut(app, "explorer", argc, argv);
  configureShortcut(app, "replay", argc, argv);

  // remote shell
  {
    std::string description = "shortcut to connect to a remote sen shell";

    auto cmd = app.add_subcommand("rshell", description);
    cmd->allow_extras();
    cmd->set_help_flag();

    std::vector<const char*> ourArgv;
    ourArgv.reserve(argc + 2U);
    ourArgv.push_back(argv[0]);  // NOLINT
    ourArgv.push_back("rshell");

    // add the remaining arguments
    for (int i = 2; i < argc; ++i)
    {
      ourArgv.push_back(argv[i]);  // NOLINT
    }

    cmd->callback(
      [=]()
      {
        runChildProcess(
          "cli_remote_shell", static_cast<int>(ourArgv.size()), const_cast<char**>(ourArgv.data()));  // NOLINT
      });
  }
}

int runApp(int argc, char* argv[])
{
  CLI::App app {"Welcome to sen\n"};

  try
  {
    app.name("sen");
    app.set_version_flag("--version", SEN_VERSION_STRING);
    app.get_formatter()->column_width(15);  // NOLINT

    configureSubcommand(app, "run", "runs a sen kernel", "cli_run", argc, argv);
    configureSubcommand(app, "gen", "generates code (mainly used by build systems)", "cli_gen", argc, argv);
    configureSubcommand(app, "archive", "allows inspecting archives", "cli_archive", argc, argv);
    configureSubcommand(app, "package", "creates or manipulates sen packages", "cli_package", argc, argv);
    configureFileToArray(app);
    configureShortcuts(app, argc, argv);

    app.footer("For help on specific commands run 'sen <command> --help'");

    app.parse(argc, argv);
  }
  catch (const CLI::ParseError& e)
  {
    return (app).exit(e);
  }

  return EXIT_SUCCESS;
}

int main(int argc, char* argv[])
{
  try
  {
    return runApp(argc, argv);
  }
  catch (const std::exception& err)
  {
    fputs("Error detected: ", stderr);
    fputs(err.what(), stderr);
    fputs("\n", stderr);
    return 1;
  }
  catch (...)
  {
    fputs("Unknown error detected\n", stderr);
    return 1;
  }
}
