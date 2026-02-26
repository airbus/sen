// === remote_terminal_client.h ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_REMOTE_TERMINAL_CLIENT_H
#define SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_REMOTE_TERMINAL_CLIENT_H

// component
#include "local_terminal.h"

// asio
#include <asio/ip/tcp.hpp>

namespace sen::components::shell
{

class RemoteTerminalClient
{
  SEN_NOCOPY_NOMOVE(RemoteTerminalClient)

public:
  RemoteTerminalClient() = default;
  ~RemoteTerminalClient();

public:
  void run(const std::string& host, const std::string& port);

private:
  void readSize(asio::ip::tcp::socket& socket);
  void readCommand(asio::ip::tcp::socket& socket, uint32_t size);
  void readLocalTerminal(asio::ip::tcp::socket& socket, std::atomic_bool& continueReading);

private:
  LocalTerminal term_;
  std::vector<uint8_t> inputBuffer_;
  std::vector<uint8_t> outputBuffer_;
};

}  // namespace sen::components::shell

#endif  // SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_REMOTE_TERMINAL_CLIENT_H
