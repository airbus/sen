// === main.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "util.h"

// cli11
#include <CLI/App.hpp>
#include <CLI/CLI.hpp>  // NOLINT (misc-include-cleaner): to correctly link

// std
#include <cstdio>
#include <exception>

int runApp(int argc, char* argv[])
{
  CLI::App app {"This is the sen package helper\n"};
  app.name("sen package");
  app.get_formatter()->column_width(35);  // NOLINT

  setupInitPackage(app);
  setupInitComponent(app);

  CLI11_PARSE(app, argc, argv);
  return 0;
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
    std::fputs("Unknown error detected\n", stderr);
    return 1;
  }
}
