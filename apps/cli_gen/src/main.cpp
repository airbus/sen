// === main.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/assert.h"

// cli11
#include <CLI/App.hpp>
#include <CLI/CLI.hpp>  // NOLINT (misc-include-cleaner): to correctly link

// std
#include <cstdio>
#include <exception>
#include <string>

//--------------------------------------------------------------------------------------------------------------
// Per-topic CLI setup functions.
//--------------------------------------------------------------------------------------------------------------

namespace sen::cli_gen
{
void setupCppCli(CLI::App& app);
void setupJsonCli(CLI::App& app);
void setupMkDocsCli(CLI::App& app);
void setupPlantUMLCli(CLI::App& app);
void setupPythonCli(CLI::App& app);
void setupTypeScriptCli(CLI::App& app);
}  // namespace sen::cli_gen

//--------------------------------------------------------------------------------------------------------------
// Main application logic
//--------------------------------------------------------------------------------------------------------------

int runApp(int argc, char* argv[])
{
  CLI::App app {"sen code generator\n"};
  app.name("sen generate");
  app.get_formatter()->column_width(35);  // NOLINT
  app.require_subcommand();

  bool expectFailure = false;
  app.add_flag("--expect-failure", expectFailure, "Expect a failure in the generation process (for testing purposes)");

  try
  {
    using namespace sen::cli_gen;  // NOLINT (google-build-using-namespace): scoped to runApp
    setupCppCli(app);
    setupPlantUMLCli(app);
    setupPythonCli(app);
    setupMkDocsCli(app);
    setupJsonCli(app);
    setupTypeScriptCli(app);

    app.footer("For help on specific commands run 'sen generate <command> --help'");

    CLI11_PARSE(app, argc, argv)

    if (expectFailure)
    {
      return 1;
    }
    return 0;
  }
  catch (const std::exception& /*e*/)
  {
    if (expectFailure)
    {
      return 0;
    }
    throw;
  }
}

int main(int argc, char* argv[])
{
  sen::registerTerminateHandler();
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
    std::fputs("Unknown error detected\n", stderr);
    return 1;
  }
}
