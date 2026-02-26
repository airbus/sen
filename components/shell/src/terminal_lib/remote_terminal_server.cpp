// === remote_terminal_server.cpp ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "remote_terminal_server.h"

// component
#include "terminal.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/meta/class_type.h"

// generated code
#include "stl/remote_shell.stl.h"

// asio
#include <asio/buffer.hpp>
#include <asio/error_code.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/system_error.hpp>
#include <asio/write.hpp>  // NOLINT(misc-include-cleaner)

// std
#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>

namespace sen::components::shell
{

//--------------------------------------------------------------------------------------------------------------
// RemoteTerminal
//--------------------------------------------------------------------------------------------------------------

RemoteTerminal::RemoteTerminal(asio::ip::tcp::socket socket, DisconnectionCallback onDisconnected)
  : socket_(std::move(socket)), onDisconnected_(std::move(onDisconnected))
{
  read();
}

RemoteTerminal::~RemoteTerminal()
{
  try
  {
    socket_.close();
  }
  catch (const asio::system_error& err)
  {
    // ok to swallow the exception here
    std::ignore = err;
  }
}

void RemoteTerminal::setWindowTitle(std::string_view title) { sendMsg(SetWindowTitle {std::string(title)}); }

bool RemoteTerminal::getRawModeEnabled() const { return false; }

void RemoteTerminal::enableRawMode()
{
  // not possible over remote link
}

void RemoteTerminal::disableRawMode() noexcept
{
  // not possible over remote link
}

void RemoteTerminal::saveCursorPosition() { sendMsg(SaveCursorPosition {}); }

void RemoteTerminal::restoreCursorPosition() { sendMsg(RestoreCursorPosition {}); }

void RemoteTerminal::moveCursorAllLeft() { sendMsg(MoveCursorAllLeft {}); }

void RemoteTerminal::moveCursorLeft(uint32_t cells) { sendMsg(MoveCursorLeft {cells}); }

void RemoteTerminal::moveCursorRight(uint32_t cells) { sendMsg(MoveCursorRight {cells}); }

void RemoteTerminal::moveCursorUp(uint32_t cells) { sendMsg(MoveCursorUp {cells}); }

void RemoteTerminal::moveCursorDown(uint32_t cells) { sendMsg(MoveCursorDown {cells}); }

void RemoteTerminal::hideCursor() { sendMsg(HideCursor {}); }

void RemoteTerminal::showCursor() { sendMsg(ShowCursor {}); }

void RemoteTerminal::getSize(uint32_t& rows, uint32_t& cols) const
{
  std::ignore = rows;
  std::ignore = cols;
}

void RemoteTerminal::newLine() { sendMsg(NewLine {}); }

void RemoteTerminal::clearScreen() { sendMsg(ClearScreen {}); }

void RemoteTerminal::clearCurrentLine() { sendMsg(ClearCurrentLine {}); }

void RemoteTerminal::clearRemainingCurrentLine() { sendMsg(ClearRemainingCurrentLine {}); }

void RemoteTerminal::setFgColor(const Color& color) { sendMsg(SetFgColor {fromColor(color)}); }

void RemoteTerminal::setBgColor(const Color& color) { sendMsg(SetBgColor {fromColor(color)}); }

void RemoteTerminal::cprint(const Style& style, std::string_view str)
{
  CPrint msg;
  msg.color = style.color;
  msg.flags = style.flags;
  msg.text = str;

  sendMsg(std::move(msg));
}

void RemoteTerminal::print(std::string_view str) { sendMsg(Print {std::string(str)}); }

void RemoteTerminal::print(char c) { sendMsg(Print {std::string(1, c)}); }

bool RemoteTerminal::getchar(Key& key, char& character, Duration timeout)
{
  if (inBuffer_.size() >= 2)
  {
    key = Key {inBuffer_.front()};
    inBuffer_.pop();

    character = static_cast<char>(inBuffer_.front());
    inBuffer_.pop();
    return true;
  }

  std::this_thread::sleep_for(timeout.toChrono());
  return false;
}

bool RemoteTerminal::isARealTerminal() const { return false; }

void RemoteTerminal::read()
{
  asio::async_read(socket_,
                   asio::buffer(readBuffer_),
                   [this](const auto& ec, auto bytes)
                   {
                     if (!ec)
                     {
                       for (std::size_t i = 0; i < bytes; ++i)
                       {
                         inBuffer_.push(readBuffer_.at(i));
                       }
                       read();
                     }
                     else
                     {
                       onDisconnected_();
                     }
                   });
}

template <typename T>
void RemoteTerminal::sendMsg(T&& msg)
{
  TerminalCmd cmd(std::forward<T>(msg));
  const auto size = SerializationTraits<TerminalCmd>::serializedSize(cmd);  // NOLINT(misc-include-cleaner)

  {
    outBuffer_.clear();
    ResizableBufferWriter writer(outBuffer_);
    OutputStream out(writer);

    out.writeUInt32(size);
    SerializationTraits<TerminalCmd>::write(out, cmd);
  }

  asio::error_code ec;
  asio::write(socket_, asio::buffer(outBuffer_), ec);

  if (ec)
  {
    onDisconnected_();
  }
}

//--------------------------------------------------------------------------------------------------------------
// RemoteTerminalServer
//--------------------------------------------------------------------------------------------------------------

RemoteTerminalServer::RemoteTerminalServer(uint16_t port, Callback onAccepted)
  : acceptor_(io_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)), onAccepted_(std::move(onAccepted))
{
  acceptor_.listen();
  doAccept();
  executor_ = std::thread([this]() { io_.run(); });
}

RemoteTerminalServer::~RemoteTerminalServer()
{
  io_.stop();
  if (executor_.joinable())
  {
    executor_.join();
  }
}

void RemoteTerminalServer::doAccept()
{
  acceptor_.async_accept(
    [this](std::error_code ec, asio::ip::tcp::socket socket)
    {
      if (!ec)
      {
        onAccepted_(io_, std::move(socket));
      }
      doAccept();
    });
}

}  // namespace sen::components::shell

extern "C" SEN_EXPORT void get0xdd3f42Types(::sen::ExportedTypesList& types);  // remote_shell.stl

extern "C" SEN_EXPORT void get0x6ced4279Types(::sen::ExportedTypesList& types) { get0xdd3f42Types(types); }
