// === vterm.h =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_VTERM_H
#define SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_VTERM_H

// sen
#include "sen/core/base/class_helpers.h"
#include "terminal.h"
#include "util.h"

// std
#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#else
#  include <unistd.h>
#endif

namespace sen::components::shell
{

/// Implementation helper for issuing virtual
/// terminal control commands.
class VTerm final
{
public:
  SEN_NOCOPY_NOMOVE(VTerm)

public:  // special members
  VTerm() = default;
  ~VTerm() = default;

public:
  void setFileDescriptor(int fd) noexcept { fd_ = fd; }

public:
  void setFgColor(const Color& color);
  void setBgColor(const Color& color);
  void setFgColor(uint32_t src);
  void hideCursor();
  void showCursor();
  void resetTextStyle();
  void saveCursorPosition();
  void restoreCursorPosition();
  void moveCursorAllLeft();
  void moveCursorLeft(uint32_t cells);
  void moveCursorRight(uint32_t cells);
  void moveCursorUp(uint32_t cells);
  void moveCursorDown(uint32_t cells);
  void newLine() { writeChars("\n"); }

  void clearCurrentLine();
  void print(std::string_view str);
  void setTextFlags(uint32_t flags);
  void setWindowTitle(std::string_view title);

private:
  void writeChars(std::string_view chars);
  void sendCSI(const char* fmt, ...);
  void sendOSC(const char* fmt, ...);

private:
#ifdef _WIN32
  int fd_ = _fileno(stdout);
#else
  int fd_ = fileno(stdout);
#endif
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

#ifdef _WIN32
constexpr const char* csiControlSeq = "\x1b[";
constexpr const char* oscControlSeq = "\x1b]";
#else
constexpr const char* csiControlSeq = "\033[";
constexpr const char* oscControlSeq = "\033]";
#endif

inline void VTerm::setFgColor(const Color& color)
{
  sendCSI("38;2;%u;%u;%um", color.r, color.g, color.b);  // NOLINT(hicpp-vararg)
}

inline void VTerm::setBgColor(const Color& color)
{
  sendCSI("48;2;%u;%u;%um", color.r, color.g, color.b);  // NOLINT(hicpp-vararg)
}

inline void VTerm::setFgColor(uint32_t src) { setFgColor(toColor(src)); }

inline void VTerm::hideCursor()
{
#ifdef WIN32
  sendCSI("25l");
#else
  writeChars("\033[?25l");
#endif
}

inline void VTerm::showCursor()
{
#ifdef WIN32
  sendCSI("25h");
#else
  writeChars("\033[?25h");
#endif
}

inline void VTerm::resetTextStyle()
{
  sendCSI("0m");  // NOLINT
}

inline void VTerm::saveCursorPosition()
{
  sendCSI("s");  // NOLINT
}

inline void VTerm::restoreCursorPosition()
{
  sendCSI("u");  // NOLINT
}

inline void VTerm::moveCursorAllLeft()
{
  sendCSI("1G");  // NOLINT
}

inline void VTerm::moveCursorLeft(uint32_t cells)
{
  if (cells == 0)
  {
    return;
  }

  sendCSI("%uD", cells);  // NOLINT
}

inline void VTerm::moveCursorRight(uint32_t cells)
{
  if (cells == 0)
  {
    return;
  }

  sendCSI("%uC", cells);  // NOLINT
}

inline void VTerm::moveCursorUp(uint32_t cells)
{
  if (cells == 0)
  {
    return;
  }

  sendCSI("%uA", cells);  // NOLINT
}

inline void VTerm::moveCursorDown(uint32_t cells)
{
  if (cells == 0)
  {
    return;
  }

  sendCSI("%uB", cells);  // NOLINT
}

inline void VTerm::clearCurrentLine()
{
  sendCSI("2K");  // NOLINT
}

inline void VTerm::print(std::string_view str) { writeChars(str); }

inline void VTerm::setTextFlags(uint32_t flags)
{
  if ((flags & TextFlags::boldText) != 0)
  {
    sendCSI("1m");  // NOLINT
  }

  if ((flags & TextFlags::italicText) != 0)
  {
    sendCSI("3m");  // NOLINT
  }

  if ((flags & TextFlags::underlineText) != 0)
  {
    sendCSI("4m");  // NOLINT
  }
}

inline void VTerm::setWindowTitle(std::string_view title)
{
  sendOSC("2;%s\x1b\\", title.data());  // NOLINT
}

inline void VTerm::writeChars(std::string_view chars)
{
#ifdef _WIN32
  std::ignore = fwrite(chars.data(), 1U, chars.length(), stdout);
#else
  std::ignore = write(fd_, chars.data(), chars.length());
#endif
}

inline void VTerm::sendCSI(const char* fmt, ...)  // NOLINT
{
  std::string result = csiControlSeq;

  va_list args;  // NOLINT
  va_start(args, fmt);
  result.append(fromArgList(fmt, args));
  va_end(args);

  writeChars(result);
}

inline void VTerm::sendOSC(const char* fmt, ...)  // NOLINT
{
  std::string result = oscControlSeq;

  va_list args;  // NOLINT
  va_start(args, fmt);
  result.append(fromArgList(fmt, args));
  va_end(args);

  writeChars(result);
}

}  // namespace sen::components::shell

#endif  // SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_VTERM_H
