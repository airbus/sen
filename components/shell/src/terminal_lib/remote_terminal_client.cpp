// === remote_terminal_client.cpp ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "remote_terminal_client.h"

// component
#include "local_terminal.h"
#include "terminal.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/io/detail/serialization_traits.h"
#include "sen/core/io/input_stream.h"

// generated code
#include "stl/remote_shell.stl.h"

// asio
#include <asio/buffer.hpp>
#include <asio/connect.hpp>  // NOLINT(misc-include-cleaner)
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>
#include <asio/write.hpp>

// std
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <thread>
#include <tuple>
#include <variant>

namespace sen::components::shell
{

//--------------------------------------------------------------------------------------------------------------
// CommandHandler
//--------------------------------------------------------------------------------------------------------------

class CommandHandler
{
  SEN_NOCOPY_NOMOVE(CommandHandler)

public:
  explicit CommandHandler(LocalTerminal& term): term_(term) {}

  ~CommandHandler() = default;

public:
  void operator()(const Print& val) { term_.print(val.text); }

  void operator()(const CPrint& val)
  {
    Style style;
    style.color = val.color;
    style.flags = val.flags;

    term_.cprint(style, val.text);
  }

  void operator()(const NewLine& val)
  {
    std::ignore = val;
    term_.newLine();
  }

  void operator()(const MoveCursorLeft& val) { term_.moveCursorLeft(val.cells); }

  void operator()(const MoveCursorRight& val) { term_.moveCursorRight(val.cells); }

  void operator()(const MoveCursorUp& val) { term_.moveCursorUp(val.cells); }

  void operator()(const MoveCursorDown& val) { term_.moveCursorDown(val.cells); }

  void operator()(const MoveCursorAllLeft& val)
  {
    std::ignore = val;
    term_.moveCursorAllLeft();
  }

  void operator()(const SetFgColor& val) { term_.setFgColor(toColor(val.color)); }

  void operator()(const SetBgColor& val) { term_.setBgColor(toColor(val.color)); }

  void operator()(const ClearCurrentLine& val)
  {
    std::ignore = val;
    term_.clearCurrentLine();
  }

  void operator()(const ClearRemainingCurrentLine& val)
  {
    std::ignore = val;
    term_.clearRemainingCurrentLine();
  }

  void operator()(const HideCursor& val)
  {
    std::ignore = val;
    term_.hideCursor();
  }

  void operator()(const ShowCursor& val)
  {
    std::ignore = val;
    term_.showCursor();
  }
  void operator()(const SaveCursorPosition& val)
  {
    std::ignore = val;
    term_.saveCursorPosition();
  }
  void operator()(const RestoreCursorPosition& val)
  {
    std::ignore = val;
    term_.restoreCursorPosition();
  }
  void operator()(const ClearScreen& val)
  {
    std::ignore = val;
    term_.clearScreen();
  }
  void operator()(const SetWindowTitle& val) { term_.setWindowTitle(val.text); }

private:
  LocalTerminal& term_;
};

//--------------------------------------------------------------------------------------------------------------
// RemoteTerminalClient
//--------------------------------------------------------------------------------------------------------------

RemoteTerminalClient::~RemoteTerminalClient() { term_.disableRawMode(); }

void RemoteTerminalClient::run(const std::string& host, const std::string& port)  // NOSONAR
{
  Style errStyle;
  errStyle.color = 0xee8881;  // NOLINT(readability-magic-numbers)
  errStyle.flags = TextFlags::boldText;

  Style msgStyle;
  msgStyle.color = 0xd4d5d9;  // NOLINT(readability-magic-numbers)
  msgStyle.flags = TextFlags::defaultText;

  std::atomic_bool readTerminal = false;
  std::thread terminalReader;

  term_.enableRawMode();

  while (true)  // NOLINT
  {
    asio::io_context io;
    asio::ip::tcp::socket socket(io);

    // Resolve the host name and service to a list of endpoints.
    asio::ip::tcp::resolver resolver(io);
    auto results = resolver.resolve(host, port);

    term_.newLine();
    term_.cprintf(msgStyle, " - connecting...\n");  // NOLINT(hicpp-vararg)

    // connect
    asio::error_code ec;
    asio::connect(socket, results, ec);

    if (!ec)
    {
      // start the reading loop
      readSize(socket);
      readTerminal = true;
      terminalReader = std::thread([this, &socket, &readTerminal]() { readLocalTerminal(socket, readTerminal); });
      io.run();

      // done reading, try again
      term_.newLine();
      term_.cprintf(errStyle, " - connection lost\n");  // NOLINT(hicpp-vararg)

      readTerminal = false;
      terminalReader.join();
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  term_.disableRawMode();
}

void RemoteTerminalClient::readLocalTerminal(asio::ip::tcp::socket& socket, std::atomic_bool& continueReading)
{
  while (continueReading)
  {
    Key key = Key::keyNone;
    char character;
    if (term_.getchar(key, character, std::chrono::milliseconds(200)))  // NOLINT(readability-magic-numbers,)
    {
      if (key == Key::keyCtrlD)
      {
        term_.newLine();
        term_.print("bye :)\n");
        exit(0);
      }

      outputBuffer_.clear();
      outputBuffer_.push_back(static_cast<uint8_t>(key));
      outputBuffer_.push_back(character);

      asio::async_write(socket,
                        asio::buffer(outputBuffer_),
                        [](const auto& err, auto /*unused*/)
                        {
                          if (err)
                          {
                            return;
                          }
                        });
    }
  }
}

void RemoteTerminalClient::readSize(asio::ip::tcp::socket& socket)
{
  inputBuffer_.resize(::sen::impl::getSerializedSize(static_cast<uint32_t>(0)));

  asio::async_read(socket,
                   asio::buffer(inputBuffer_),
                   [this, &socket](const auto& err, auto /*unused*/)
                   {
                     if (!err)
                     {
                       uint32_t size = 0;
                       {
                         InputStream in(inputBuffer_);
                         in.readUInt32(size);
                       }
                       readCommand(socket, size);
                     }
                   });
}

void RemoteTerminalClient::readCommand(asio::ip::tcp::socket& socket, uint32_t size)
{
  inputBuffer_.resize(size);

  asio::async_read(socket,
                   asio::buffer(inputBuffer_),
                   [this, &socket](const auto& err, auto /*unused*/)
                   {
                     if (err)
                     {
                       return;
                     }

                     TerminalCmd cmd;
                     {
                       InputStream in(inputBuffer_);
                       SerializationTraits<TerminalCmd>::read(in, cmd);
                     }

                     std::visit(CommandHandler(term_), cmd);
                     readSize(socket);
                   });
}

}  // namespace sen::components::shell
