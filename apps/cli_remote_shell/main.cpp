// === main.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "remote_terminal_client.h"
#include "remote_terminal_server.h"

// sen
#include "sen/core/io/util.h"

// cli11
#include <CLI/App.hpp>
// NOLINTNEXTLINE (misc-include-cleaner): cli11 needs all headers to correctly link required vtables
#include <CLI/CLI.hpp>

// std
#include <cstdio>
#include <exception>
#include <memory>
#include <string>

int runApp(int argc, char* argv[])
{
  struct ProgramArgs
  {
    std::string address;
  };
  auto args = std::make_shared<ProgramArgs>();

  CLI::App app {"Connects to a remote sen shell\n"};
  app.name("sen rshell");
  app.get_formatter()->column_width(35);  // NOLINT
  app.add_option("address", args->address, "Address of the sen process (format <host>[:<port>]")->required();

  app.callback(
    [args]()
    {
      auto tokens = sen::impl::split(args->address, ':');
      auto port =
        tokens.size() > 1U ? tokens.at(1) : std::to_string(sen::components::shell::RemoteTerminalServer::defaultPort);

      sen::components::shell::RemoteTerminalClient client;
      client.run(tokens.front(), port);
    });

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
