// === main.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "cli_archive_setup.h"

// sen
#include "sen/core/base/assert.h"

// cli11
#include <CLI/Validators.hpp>

// os
#ifdef _WIN32
#  include <io.h>
#else
#  include <unistd.h>
#endif

// std
#include <cstdio>
#include <exception>

int runApp(int argc, char* argv[])
{
  CLI::App app {"Recording inspection and manipulation utility\n"};
  app.name("sen archive");
  app.get_formatter()->column_width(12);  // NOLINT

  setupInfo(app);
  setupIndexed(app);
  setupMerger(app);

  app.footer("For help on specific commands run 'sen archive <command> --help'");

  CLI11_PARSE(app, argc, argv);
  return 0;
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
