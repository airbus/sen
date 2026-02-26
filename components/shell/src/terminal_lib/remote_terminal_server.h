// === remote_terminal_server.h ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_REMOTE_TERMINAL_SERVER_H
#define SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_REMOTE_TERMINAL_SERVER_H

#include "terminal.h"

// asio
#include <asio/ip/tcp.hpp>

// std
#include <queue>
#include <thread>

// std
#include <queue>

namespace sen::components::shell
{

class RemoteTerminal final: public Terminal
{
public:
  SEN_NOCOPY_NOMOVE(RemoteTerminal)

public:
  using DisconnectionCallback = std::function<void()>;

public:
  RemoteTerminal(asio::ip::tcp::socket socket, DisconnectionCallback onDisconnected);
  ~RemoteTerminal() override;

public:  // implements Terminal
  void setWindowTitle(std::string_view title) override;
  [[nodiscard]] bool getRawModeEnabled() const override;
  void enableRawMode() override;
  void disableRawMode() noexcept override;
  void saveCursorPosition() override;
  void restoreCursorPosition() override;
  void moveCursorAllLeft() override;
  void moveCursorLeft(uint32_t cells) override;
  void moveCursorRight(uint32_t cells) override;
  void moveCursorUp(uint32_t cells) override;
  void moveCursorDown(uint32_t cells) override;
  void hideCursor() override;
  void showCursor() override;
  void getSize(uint32_t& rows, uint32_t& cols) const override;
  void newLine() override;
  void clearScreen() override;
  void clearCurrentLine() override;
  void clearRemainingCurrentLine() override;
  void setFgColor(const Color& color) override;
  void setBgColor(const Color& color) override;
  void cprint(const Style& style, std::string_view str) override;
  void print(std::string_view str) override;
  void print(char c) override;
  [[nodiscard]] bool getchar(Key& key, char& character, Duration timeout) override;
  [[nodiscard]] bool isARealTerminal() const override;

private:
  template <typename T>
  void sendMsg(T&& msg);
  void read();

private:
  asio::ip::tcp::socket socket_;
  std::vector<uint8_t> outBuffer_;
  std::queue<uint8_t> inBuffer_;
  std::array<uint8_t, 2U> readBuffer_ {};
  DisconnectionCallback onDisconnected_;
};

class RemoteTerminalServer
{
  SEN_NOCOPY_NOMOVE(RemoteTerminalServer)

public:
  using Callback = std::function<void(asio::io_context& io, asio::ip::tcp::socket socket)>;

public:
  RemoteTerminalServer(uint16_t port, Callback onAccepted);
  ~RemoteTerminalServer();

public:
  static constexpr uint16_t defaultPort = 30486;

private:
  void doAccept();

private:
  asio::io_context io_;
  asio::ip::tcp::acceptor acceptor_;
  Callback onAccepted_;
  std::thread executor_;
};

}  // namespace sen::components::shell

#endif  // SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_REMOTE_TERMINAL_SERVER_H
