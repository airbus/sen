// === win32_terminal.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifdef _WIN32

#  include "local_terminal.h"
#  include "vterm.h"

// windows
#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT 0x0500  // NOLINT
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif

// clang-format off
#include <windows.h>
#include <wincon.h>

#include <fcntl.h>
#include <io.h>
// clang-format on

// std
#  include <cstdio>

namespace sen::components::shell
{

//--------------------------------------------------------------------------------------------------------------
// LocalTerminalImpl
//--------------------------------------------------------------------------------------------------------------

class LocalTerminalImpl
{
public:
  SEN_MOVE_ONLY(LocalTerminalImpl)

public:
  LocalTerminalImpl()
  {
    // get handles to STDIN and STDOUT.
    hStdin_ = GetStdHandle(STD_INPUT_HANDLE);
    hStdout_ = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleOutputCP(437);  // NOLINT

    DWORD dwMode = 0;
    GetConsoleMode(hStdout_, &dwMode);

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hStdout_, dwMode);
  }

  ~LocalTerminalImpl() { disableRawMode(); }

  [[nodiscard]] bool getRawModeEnabled() const { return rawEnabled_; }

  void enableRawMode()
  {
    if (rawEnabled_)
    {
      return;
    }

    // save the console mode
    GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &originalMode);

    // disable echo and line input
    DWORD fdwMode = originalMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);

    // enable window events
    fdwMode = fdwMode | ENABLE_WINDOW_INPUT;

    rawEnabled_ = (SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), fdwMode) != 0);
  }

  void disableRawMode() noexcept
  {
    if (!rawEnabled_)
    {
      return;
    }

    // restore original status
    SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), originalMode);

    rawEnabled_ = false;
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

  void hideCursor()
  {
    CONSOLE_CURSOR_INFO oldCursorInfo;
    GetConsoleCursorInfo(hStdout_, &oldCursorInfo);

    // make the cursor invisible
    CONSOLE_CURSOR_INFO newCursorInfo = oldCursorInfo;
    newCursorInfo.bVisible = false;
    SetConsoleCursorInfo(hStdout_, &newCursorInfo);
  }

  void showCursor()
  {
    CONSOLE_CURSOR_INFO oldCursorInfo;
    GetConsoleCursorInfo(hStdout_, &oldCursorInfo);

    // make the cursor invisible
    CONSOLE_CURSOR_INFO newCursorInfo = oldCursorInfo;
    newCursorInfo.bVisible = true;
    SetConsoleCursorInfo(hStdout_, &newCursorInfo);
  }

  void getSize(uint32_t& rows, uint32_t& cols) const
  {
    CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
    if (GetConsoleScreenBufferInfo(hStdout_, &csbiInfo))
    {
      rows = csbiInfo.dwSize.Y;
      cols = csbiInfo.dwSize.X;
    }
    else
    {
      rows = 100;  // NOLINT
      cols = 80;   // NOLINT
    }
  }

  void newLine() { vterm_.newLine(); }

  void clearScreen()
  {
    CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
    GetConsoleScreenBufferInfo(hStdout_, &csbiInfo);

    COORD coordScreen = {0, 0};  // home for the cursor

    // get the number of character cells in the current buffer.
    if (!GetConsoleScreenBufferInfo(hStdout_, &csbiInfo))
    {
      return;
    }

    DWORD dwConSize = csbiInfo.dwSize.X * csbiInfo.dwSize.Y;
    DWORD charsWritten;

    // fill the entire screen with blanks.
    if (!FillConsoleOutputCharacter(hStdout_,        // handle to console screen buffer
                                    (TCHAR)' ',      // character to write to the buffer
                                    dwConSize,       // number of cells to write
                                    coordScreen,     // coordinates of first cell
                                    &charsWritten))  // receive number of characters written
    {
      return;
    }

    // get the current text attribute.
    if (!GetConsoleScreenBufferInfo(hStdout_, &csbiInfo))
    {
      return;
    }

    // set the buffer's attributes accordingly.
    if (!FillConsoleOutputAttribute(hStdout_,              // handle to console screen buffer
                                    csbiInfo.wAttributes,  // character attributes to use
                                    dwConSize,             // number of cells to set attribute
                                    coordScreen,           // coordinates of first cell
                                    &charsWritten))        // receive number of characters written
    {
      return;
    }

    // put the cursor at its home coordinates.
    SetConsoleCursorPosition(hStdout_, coordScreen);
  }

  void clearCurrentLine() { vterm_.clearCurrentLine(); }

  void clearRemainingCurrentLine()
  {
    // get the console status
    CONSOLE_SCREEN_BUFFER_INFO csbiInfo;
    GetConsoleScreenBufferInfo(hStdout_, &csbiInfo);

    int remainingChars = csbiInfo.dwSize.X - csbiInfo.dwCursorPosition.X;
    DWORD writtenChars = 0;

    FillConsoleOutputCharacter(hStdout_,                   // handle to console screen buffer
                               (TCHAR)' ',                 // character to write to the buffer
                               remainingChars,             // number of cells to write
                               csbiInfo.dwCursorPosition,  // coordinates of first cell
                               &writtenChars);

    // move cursor to original position.
    SetConsoleCursorPosition(hStdout_, csbiInfo.dwCursorPosition);
  }

  void print(std::string_view str) { vterm_.print(str); }

  void cprint(const Style& style, std::string_view str)
  {
    vterm_.setTextFlags(style.flags);
    vterm_.setFgColor(style.color);
    vterm_.print(str);
    vterm_.resetTextStyle();
  }

  [[nodiscard]] bool getchar(Key& resultKey, char& character, Duration timeout)
  {
    resultKey = Key::keyNone;
    character = 0;

    if (timeout.getNanoseconds() > 0)
    {
      auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(timeout.toChrono()).count();

      DWORD milliseconds = static_cast<DWORD>(waitTime);

      DWORD result = WaitForSingleObject(hStdin_, milliseconds);
      if (result != WAIT_OBJECT_0)
      {
        return false;
      }
    }

    {
      // to store the event info
      INPUT_RECORD irInBuf;

      // read the next event
      DWORD cRead;

      ReadConsoleInput(hStdin_, &irInBuf, 1, &cRead);

      if (irInBuf.EventType != KEY_EVENT)
      {
        return false;
      }

      if (!irInBuf.Event.KeyEvent.bKeyDown)
      {
        return false;
      }

      WORD key = irInBuf.Event.KeyEvent.wVirtualKeyCode;

      if ((irInBuf.Event.KeyEvent.dwControlKeyState & LEFT_CTRL_PRESSED) ||
          (irInBuf.Event.KeyEvent.dwControlKeyState & RIGHT_CTRL_PRESSED))
      {
        if (key == 0x41)  // a
        {
          resultKey = Key::keyCtrlA;
          return true;
        }

        if (key == 0x42)  // b
        {
          resultKey = Key::keyCtrlB;
          return true;
        }

        if (key == 0x44)  // d
        {
          resultKey = Key::keyCtrlD;
          return true;
        }

        if (key == 0x45)  // e
        {
          resultKey = Key::keyCtrlE;
          return true;
        }

        if (key == 0x46)  // f
        {
          resultKey = Key::keyCtrlF;
          return true;
        }

        if (key == 0x47)  // g
        {
          resultKey = Key::keyCtrlG;
          return true;
        }

        if (key == 0x48)  // h
        {
          resultKey = Key::keyCtrlH;
          return true;
        }

        if (key == 0x49)  // i
        {
          resultKey = Key::keyCtrlI;
          return true;
        }

        if (key == 0x4A)  // j
        {
          resultKey = Key::keyCtrlJ;
          return true;
        }

        if (key == 0x4B)  // k
        {
          resultKey = Key::keyCtrlK;
          return true;
        }

        if (key == 0x4C)  // l
        {
          resultKey = Key::keyCtrlL;
          return true;
        }

        if (key == 0x4D)  // m
        {
          resultKey = Key::keyCtrlM;
          return true;
        }

        if (key == 0x4E)  // n
        {
          resultKey = Key::keyCtrlN;
          return true;
        }

        if (key == 0x4F)  // o
        {
          resultKey = Key::keyCtrlO;
          return true;
        }

        if (key == 0x50)  // p
        {
          resultKey = Key::keyCtrlP;
          return true;
        }

        if (key == 0x51)  // q
        {
          resultKey = Key::keyCtrlQ;
          return true;
        }

        if (key == 0x52)  // r
        {
          resultKey = Key::keyCtrlR;
          return true;
        }

        if (key == 0x53)  // s
        {
          resultKey = Key::keyCtrlS;
          return true;
        }

        if (key == 0x54)  // t
        {
          resultKey = Key::keyCtrlT;
          return true;
        }

        if (key == 0x55)  // u
        {
          resultKey = Key::keyCtrlU;
          return true;
        }

        if (key == 0x56)  // v
        {
          resultKey = Key::keyCtrlV;
          return true;
        }

        if (key == 0x57)  // w
        {
          resultKey = Key::keyCtrlW;
          return true;
        }

        if (key == 0x58)  // x
        {
          resultKey = Key::keyCtrlX;
          return true;
        }

        if (key == 0x59)  // y
        {
          resultKey = Key::keyCtrlY;
          return true;
        }

        if (key == 0x5A)  // z
        {
          resultKey = Key::keyCtrlZ;
          return true;
        }
      }
      else if (key == VK_DELETE)
      {
        resultKey = Key::keyDelete;
        return true;
      }
      else if (key == VK_BACK)
      {
        resultKey = Key::keyBackspace;
        return true;
      }
      else if (key == VK_UP)
      {
        resultKey = Key::keyArrowUp;
        return true;
      }
      else if (key == VK_DOWN)
      {
        resultKey = Key::keyArrowDown;
        return true;
      }
      else if (key == VK_LEFT)
      {
        resultKey = Key::keyArrowLeft;
        return true;
      }
      else if (key == VK_RIGHT)
      {
        resultKey = Key::keyArrowRight;
        return true;
      }
      else if (key == VK_TAB)
      {
        resultKey = Key::keyTab;
        return true;
      }
      else if (key == VK_HOME)
      {
        resultKey = Key::keyHome;
        return true;
      }
      else if (key == VK_END)
      {
        resultKey = Key::keyEnd;
        return true;
      }
      else if (key == VK_RETURN)
      {
        resultKey = Key::keyEnter;
        return true;
      }
      else if (key == VK_F1)
      {
        resultKey = Key::keyF1;
        return true;
      }
      else if (key == VK_F2)
      {
        resultKey = Key::keyF2;
        return true;
      }
      else if (key == VK_F3)
      {
        resultKey = Key::keyF3;
        return true;
      }
      else if (key == VK_F4)
      {
        resultKey = Key::keyF4;
        return true;
      }
      else if (key == VK_F5)
      {
        resultKey = Key::keyF5;
        return true;
      }
      else if (key == VK_F6)
      {
        resultKey = Key::keyF6;
        return true;
      }
      else if (key == VK_F7)
      {
        resultKey = Key::keyF7;
        return true;
      }
      else if (key == VK_F8)
      {
        resultKey = Key::keyF8;
        return true;
      }
      else if (key == VK_F9)
      {
        resultKey = Key::keyF9;
        return true;
      }
      else if (key == VK_F10)
      {
        resultKey = Key::keyF10;
        return true;
      }
      else if (key == VK_F11)
      {
        resultKey = Key::keyF11;
        return true;
      }
      else if (key == VK_F12)
      {
        resultKey = Key::keyF12;
        return true;
      }
      else if ((key >= 0x30 && key <= 0x5A) ||  // normal chars (from '0' to 'Z')
               (key >= 0x60 && key <= 0x6F) ||  // numpad keys
               (key >= 0x92 && key <= 0x96) ||  // OEM specific
               (key >= 0xE3 && key <= 0xE4) ||  // OEM specific
               (key >= 0xE9 && key <= 0xF5) ||  // OEM specific
               key == 0xE1 ||                   // OEM specific
               key == 0xE6 ||                   // OEM specific
               key == VK_SPACE ||               // space
               key == VK_OEM_PLUS ||            // for any country, the '+' key
               key == VK_OEM_COMMA ||           // for any country, the ',' key
               key == VK_OEM_MINUS ||           // for any country, the '-' key
               key == VK_OEM_PERIOD ||          // for any country, the '.' key
               key == VK_OEM_1 ||               // miscellaneous characters
               key == VK_OEM_2 ||               // miscellaneous characters
               key == VK_OEM_3 ||               // miscellaneous characters
               key == VK_OEM_4 ||               // miscellaneous characters
               key == VK_OEM_5 ||               // miscellaneous characters
               key == VK_OEM_6 ||               // miscellaneous characters
               key == VK_OEM_7 ||               // miscellaneous characters
               key == VK_OEM_8 ||               // miscellaneous characters
               key == VK_OEM_102)               // angle bracket or backslash
      {
        resultKey = Key::keyCharacter;
        character = irInBuf.Event.KeyEvent.uChar.AsciiChar;
        return true;
      }
      else
      {
        // no code needed
      }
    }

    return false;
  }

private:
  VTerm vterm_;
  bool rawEnabled_ = false;
  DWORD originalMode = 0;
  HANDLE hStdout_ = 0;
  HANDLE hStdin_ = 0;
};

//--------------------------------------------------------------------------------------------------------------
// LocalTerminal
//--------------------------------------------------------------------------------------------------------------

LocalTerminal::LocalTerminal(): pimpl_(std::make_unique<LocalTerminalImpl>()) {}

LocalTerminal::~LocalTerminal() { restore(); }

void LocalTerminal::setWindowTitle(std::string_view title) { pimpl_->setWindowTitle(title); }

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

void LocalTerminal::getSize(uint32_t& rows, uint32_t& cols) const { pimpl_->getSize(rows, cols); }

void LocalTerminal::newLine() { pimpl_->newLine(); }

void LocalTerminal::clearScreen() { pimpl_->clearScreen(); }

void LocalTerminal::clearCurrentLine() { pimpl_->clearCurrentLine(); }

void LocalTerminal::clearRemainingCurrentLine() { pimpl_->clearRemainingCurrentLine(); }

void LocalTerminal::setFgColor(const Color& color) { pimpl_->setFgColor(color); }

void LocalTerminal::setBgColor(const Color& color) { pimpl_->setBgColor(color); }

void LocalTerminal::print(std::string_view str) { pimpl_->print(str); }

void LocalTerminal::print(char c) { pimpl_->print(std::string(1U, c)); }

void LocalTerminal::cprint(const Style& style, std::string_view str) { pimpl_->cprint(style, str); }

bool LocalTerminal::getchar(Key& key, char& character, Duration timeout)
{
  return pimpl_->getchar(key, character, timeout);
}

void LocalTerminal::restore()
{
  // nothing to do here. Windows restores this automatically.
}

bool LocalTerminal::isARealTerminal() const { return true; }

}  // namespace sen::components::shell

#endif
