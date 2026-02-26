// === posix_terminal.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "local_terminal.h"
#include "sen/core/base/compiler_macros.h"
#include "terminal.h"
#include "vterm.h"

// os
#include <bits/stdio_lim.h>
#include <bits/types/struct_iovec.h>
#include <fcntl.h>
#include <stdio.h>  // NOLINT
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <termios.h>
#include <unistd.h>

// std
#include <array>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <tuple>

namespace sen::components::shell
{

//--------------------------------------------------------------------------------------------------------------
// LocalTerminalImpl
//--------------------------------------------------------------------------------------------------------------

class LocalTerminalImpl final
{
public:
  SEN_NOCOPY_NOMOVE(LocalTerminalImpl)

public:
  LocalTerminalImpl()
  {
    // get and open the current terminal
    std::array<char, L_ctermid> termId {};
    ctermid(termId.data());
    termFd_ = open(termId.data(), O_RDWR | O_NOCTTY | O_SYNC | O_CLOEXEC);  // NOLINT

    // default to stdout
    if (termFd_ < 0)
    {
      termFd_ = STDOUT_FILENO;
    }

    vterm_.setFileDescriptor(termFd_);

    // save the current terminal status
    tcgetattr(termFd_, &originalTerm_);
  }

  ~LocalTerminalImpl()
  {
    disableRawMode();

    // close the terminal file descriptor (if it is not stdout)
    if (termFd_ != STDOUT_FILENO)
    {
      close(termFd_);
    }
  }

  [[nodiscard]] bool getRawModeEnabled() const { return rawEnabled_; }

  void enableRawMode()
  {
    if (rawEnabled_)
    {
      return;
    }

    // modify the original mode
    struct termios newTermios = originalTerm_;

    // input modes: no break, no CR to NL, no parity check,
    // no strip char, no start/stop output control.
    newTermios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);  // NOLINT

    // control modes - set 8 bit chars
    newTermios.c_cflag |= CS8;  // NOLINT

    // local modes - echoing off, canonical off, no extended functions,
    newTermios.c_lflag &= ~(ECHO | ICANON | IEXTEN);  // NOLINT

    // control chars - set return condition: min number of bytes and timer.
    newTermios.c_cc[VMIN] = 1;

    // We want read to return every single byte, without timeout.
    newTermios.c_cc[VTIME] = 0;  // 1 byte, no timer

    // put terminal in raw mode after flushing
    if (tcsetattr(termFd_, TCSAFLUSH, &newTermios) < 0)
    {
      return;
    }

    // now we have the raw mode active
    rawEnabled_ = true;
  }

  void disableRawMode() noexcept
  {
    if (!rawEnabled_)
    {
      return;
    }

    // default style
    vterm_.resetTextStyle();

    // restore original status
    if (tcsetattr(termFd_, TCSAFLUSH, &originalTerm_) != -1)
    {
      rawEnabled_ = false;
    }
  }

  void setWindowTitle(std::string_view title) { vterm_.setWindowTitle(title); }

  void setFgColor(const Color& color) { vterm_.setFgColor(color); }

  void setBgColor(const Color& color) { vterm_.setBgColor(color); }

  void saveCursorPosition() { vterm_.saveCursorPosition(); }

  void restoreCursorPosition() { vterm_.restoreCursorPosition(); }

  void moveCursorAllLeft() { vterm_.moveCursorAllLeft(); }

  void moveCursorLeft(uint32_t cells) { vterm_.moveCursorLeft(cells); }

  void moveCursorRight(uint32_t cells) { vterm_.moveCursorRight(cells); }

  void moveCursorUp(uint32_t cells) { vterm_.moveCursorUp(cells); }

  void moveCursorDown(uint32_t cells) { vterm_.moveCursorDown(cells); }

  void hideCursor() { vterm_.hideCursor(); }

  void showCursor() { vterm_.showCursor(); }

  static void getSize(uint32_t& rows, uint32_t& cols)
  {
    winsize ws {};

    if (auto rc = ioctl(1, TIOCGWINSZ, &ws); rc < 0)  // NOLINT
    {
      ws.ws_col = 0;
      ws.ws_row = 0;
    }

    rows = ws.ws_row ? ws.ws_row : 100;  // NOLINT
    cols = ws.ws_col ? ws.ws_col : 80;   // NOLINT
  }

  void newLine()
  {
    char nl = '\n';
    std::ignore = write(termFd_, &nl, 1);
    moveCursorAllLeft();
  }

  void clearScreen() const
  {
    sendCmd("H");
    sendCmd("2J");
  }

  void clearCurrentLine() const { sendCmd("2K"); }

  void clearRemainingCurrentLine() const { sendCmd("0K"); }

  void print(std::string_view str) { vterm_.print(str); }

  void cprint(const Style& style, std::string_view str)
  {
    vterm_.setTextFlags(style.flags);
    vterm_.setFgColor(style.color);
    vterm_.print(str);
    vterm_.resetTextStyle();
  }

  [[nodiscard]] bool getchar(Key& resultKey, char& character, Duration timeout) const  // NOLINT
  {
    resultKey = Key::keyNone;
    character = 0;

    const auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(timeout.toChrono()).count();
    struct timeval selectTimeout = {microseconds / 1000000L, microseconds % 1000000L};  // NOLINT

    fd_set readSet = {};
    FD_ZERO(&readSet);          // NOLINT
    FD_SET(termFd_, &readSet);  // NOLINT
    if (select(termFd_ + 1, &readSet, nullptr, nullptr, timeout.getNanoseconds() < 0 ? nullptr : &selectTimeout) > 0)
    {
      // read a character
      char in = 0;
      if (const auto readCount = read(termFd_, &in, 1); readCount <= 0)
      {
        return false;
      }

      switch (in)
      {
        case 1:
          resultKey = Key::keyCtrlA;
          return true;
        case 2:
          resultKey = Key::keyCtrlB;
          return true;
        case 4:
          resultKey = Key::keyCtrlD;
          return true;
        case 5:  // NOLINT
          resultKey = Key::keyCtrlE;
          return true;
        case 6:  // NOLINT
          resultKey = Key::keyCtrlF;
          return true;
        case 7:  // NOLINT
          resultKey = Key::keyCtrlG;
          return true;
        case 8:  // NOLINT
          resultKey = Key::keyBackspace;
          return true;
        case 9:  // NOLINT
          resultKey = Key::keyTab;
          return true;
        case 10:  // NOLINT
          resultKey = Key::keyCtrlJ;
          return true;
        case 11:  // NOLINT
          resultKey = Key::keyCtrlK;
          return true;
        case 12:  // NOLINT
          resultKey = Key::keyCtrlL;
          return true;
        case 13:  // NOLINT
          resultKey = Key::keyEnter;
          return true;
        case 14:  // NOLINT
          resultKey = Key::keyCtrlN;
          return true;
        case 15:  // NOLINT
          resultKey = Key::keyCtrlO;
          return true;
        case 16:  // NOLINT
          resultKey = Key::keyCtrlP;
          return true;
        case 17:  // NOLINT
          resultKey = Key::keyCtrlQ;
          return true;
        case 18:  // NOLINT
          resultKey = Key::keyCtrlR;
          return true;
        case 19:  // NOLINT
          resultKey = Key::keyCtrlS;
          return true;
        case 20:  // NOLINT
          resultKey = Key::keyCtrlT;
          return true;
        case 21:  // NOLINT
          resultKey = Key::keyCtrlU;
          return true;
        case 22:  // NOLINT
          resultKey = Key::keyCtrlV;
          return true;
        case 23:  // NOLINT
          resultKey = Key::keyCtrlW;
          return true;
        case 24:  // NOLINT
          resultKey = Key::keyCtrlX;
          return true;
        case 25:  // NOLINT
          resultKey = Key::keyCtrlY;
          return true;
        case 26:  // NOLINT
          resultKey = Key::keyCtrlZ;
          return true;
        case 127:  // NOLINT
          resultKey = Key::keyBackspace;
          return true;
        case 27:  // escape sequence  NOLINT
        {
          char c = 0;

          // read the next two bytes
          if (read(termFd_, &c, 1) != 1)
          {
            break;
          }

          if (c == 'O')  // 79 - Single Shift Select
          {
            if (read(termFd_, &c, 1) != 1)
            {
              c = 0;
            }

            switch (c)
            {
              case 80:  // NOLINT
                resultKey = Key::keyF1;
                return true;
              case 81:  // NOLINT
                resultKey = Key::keyF2;
                return true;
              case 82:  // NOLINT
                resultKey = Key::keyF3;
                return true;
              case 83:  // NOLINT
                resultKey = Key::keyF4;
                return true;
              default:
                break;
            }
          }
          else if (c == '[')  // 91 - Control Sequence Indicator (CSI)
          {
            if (read(termFd_, &c, 1) != 1)
            {
              break;
            }

            switch (c)
            {
              case 65:  // NOLINT
                resultKey = Key::keyArrowUp;
                return true;
              case 66:  // NOLINT
                resultKey = Key::keyArrowDown;
                return true;
              case 67:  // NOLINT
                resultKey = Key::keyArrowRight;
                return true;
              case 68:  // NOLINT
                resultKey = Key::keyArrowLeft;
                return true;
              case 70:  // NOLINT
                resultKey = Key::keyEnd;
                return true;
              case 72:  // NOLINT
                resultKey = Key::keyHome;
                return true;
              default:
              {
                // extended escape, read additional two bytes.
                if (c < 49 || c > 54)  // NOLINT
                {
                  break;
                }

                std::array<char, 2> seq2 {};

                ssize_t sz = read(termFd_, seq2.data(), 2);

                if (sz < 1)
                {
                  break;
                }
                if (sz == 1)
                {
                  seq2[1] = 0;
                }

                if (c == 49 && seq2[0] == 53 && seq2[1] == 126)  // NOLINT
                {
                  resultKey = Key::keyF5;
                  return true;
                }

                if (c == 49 && seq2[0] == 55 && seq2[1] == 126)  // NOLINT
                {
                  resultKey = Key::keyF6;
                  return true;
                }

                if (c == 49 && seq2[0] == 56 && seq2[1] == 126)  // NOLINT
                {
                  resultKey = Key::keyF7;
                  return true;
                }

                if (c == 49 && seq2[0] == 57 && seq2[1] == 126)  // NOLINT
                {
                  resultKey = Key::keyF8;
                  return true;
                }

                if (c == 50 && seq2[0] == 48 && seq2[1] == 126)  // NOLINT
                {
                  resultKey = Key::keyF9;
                  return true;
                }

                if (c == 50 && seq2[0] == 49 && seq2[1] == 126)  // NOLINT
                {
                  resultKey = Key::keyF10;
                  return true;
                }

                if (c == 50 && seq2[0] == 51 && seq2[1] == 126)  // NOLINT
                {
                  resultKey = Key::keyF11;
                  return true;
                }

                if (c == 50 && seq2[0] == 52 && seq2[1] == 126)  // NOLINT
                {
                  resultKey = Key::keyF12;
                  return true;
                }

                if (c == 50 && seq2[0] == 126 && seq2[1] == 0)  // NOLINT
                {
                  resultKey = Key::keyInsert;
                  return true;
                }

                if (c == 51 && seq2[0] == 126 && seq2[1] == 0)  // NOLINT
                {
                  resultKey = Key::keyDelete;
                  return true;
                }

                if (c == 53 && seq2[0] == 126 && seq2[1] == 0)  // NOLINT
                {
                  resultKey = Key::keyPageUp;
                  return true;
                }

                if (c == 54 && seq2[0] == 126 && seq2[1] == 0)  // NOLINT
                {
                  resultKey = Key::keyPageDown;
                  return true;
                }

                break;
              }
            }
          }
          else
          {
            // no code needed
          }

          break;
        }

        default:
        {
          // check for printable characters only
          if (in > 31 && in < 127)  // NOLINT
          {
            resultKey = Key::keyCharacter;
            character = in;
            return true;
          }

          break;
        }
      }
    }

    return false;
  }

  [[nodiscard]] bool isARealTerminal() const { return isatty(termFd_) == 1; }

private:
  void sendCmd(std::string_view cmd) const
  {
    constexpr std::string_view header("\033[");
    constexpr std::size_t vectors = 2U;

    auto* hdrData = static_cast<void*>(const_cast<char*>(header.data()));  // NOLINT forced by writev
    auto* cmdData = static_cast<void*>(const_cast<char*>(cmd.data()));     // NOLINT forced by writev

    std::array<iovec, vectors> iov = {iovec {hdrData, header.size()}, iovec {cmdData, cmd.size()}};
    std::ignore = writev(termFd_, iov.data(), vectors);
  }

private:
  bool rawEnabled_ = false;
  int termFd_;
  VTerm vterm_;
  termios originalTerm_ {};
};

//--------------------------------------------------------------------------------------------------------------
// LocalTerminal
//--------------------------------------------------------------------------------------------------------------

LocalTerminal::LocalTerminal(): pimpl_(std::make_unique<LocalTerminalImpl>()) {}

LocalTerminal::~LocalTerminal() { restore(); }

void LocalTerminal::setWindowTitle(std::string_view title) { pimpl_->setWindowTitle(title); }

void LocalTerminal::setFgColor(const Color& color) { pimpl_->setFgColor(color); }

void LocalTerminal::setBgColor(const Color& color) { pimpl_->setBgColor(color); }

bool LocalTerminal::getRawModeEnabled() const { return pimpl_->getRawModeEnabled(); }

void LocalTerminal::enableRawMode() { pimpl_->enableRawMode(); }

void LocalTerminal::disableRawMode() noexcept { pimpl_->disableRawMode(); }

void LocalTerminal::saveCursorPosition() { pimpl_->saveCursorPosition(); }

void LocalTerminal::restoreCursorPosition() { pimpl_->restoreCursorPosition(); }

void LocalTerminal::moveCursorAllLeft() { pimpl_->moveCursorAllLeft(); }

void LocalTerminal::moveCursorLeft(uint32_t cells) { pimpl_->moveCursorLeft(cells); }

void LocalTerminal::moveCursorRight(uint32_t cells) { pimpl_->moveCursorRight(cells); }

void LocalTerminal::moveCursorUp(uint32_t cells) { pimpl_->moveCursorUp(cells); }

void LocalTerminal::moveCursorDown(uint32_t cells) { pimpl_->moveCursorDown(cells); }

void LocalTerminal::hideCursor() { pimpl_->hideCursor(); }

void LocalTerminal::showCursor() { pimpl_->showCursor(); }

void LocalTerminal::getSize(uint32_t& rows, uint32_t& cols) const { LocalTerminalImpl::getSize(rows, cols); }

void LocalTerminal::newLine() { pimpl_->newLine(); }

void LocalTerminal::clearScreen() { pimpl_->clearScreen(); }

void LocalTerminal::clearCurrentLine() { pimpl_->clearCurrentLine(); }

void LocalTerminal::clearRemainingCurrentLine() { pimpl_->clearRemainingCurrentLine(); }

void LocalTerminal::print(std::string_view str) { pimpl_->print(str); }

void LocalTerminal::print(char c) { pimpl_->print(std::string(1U, c)); }

void LocalTerminal::cprint(const Style& style, std::string_view str) { pimpl_->cprint(style, str); }

bool LocalTerminal::getchar(Key& key, char& character, Duration timeout)
{
  return pimpl_->getchar(key, character, timeout);
}

void LocalTerminal::restore()
{
  // nothing to do here.
}

bool LocalTerminal::isARealTerminal() const { return pimpl_->isARealTerminal(); }

}  // namespace sen::components::shell
