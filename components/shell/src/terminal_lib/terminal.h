// === terminal.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_TERMINAL_H
#define SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_TERMINAL_H

// terminal_lib
#include "util.h"

// sen
#include "sen/core/base/duration.h"

// std
#include <string>

namespace sen::components::shell
{

/// Types of keys that can be pressed.
enum class Key
{
  keyNone,        ///< no key pressed.
  keyCharacter,   ///< char:   any other key.
  keyTab,         ///< tab:    auto-complete/indent.
  keyEnter,       ///< intro:  return.
  keyBackspace,   ///< ctrl-h: backspace.
  keyHome,        ///< home:   go to the start of the line.
  keyEnd,         ///< end:    go to the end of the line.
  keyArrowLeft,   ///< left:   move cursor left.
  keyArrowRight,  ///< right:  move cursor right.
  keyArrowUp,     ///< up:     history previous.
  keyArrowDown,   ///< down:   history next.
  keyPageUp,      ///< pgup:   page up.
  keyPageDown,    ///< pgdown: page down.
  keyDelete,      ///< delete: remove char at right of cursor.
  keyInsert,      ///< insert
  keyCtrlA,       ///< ctrl-a: default - go to the start of the line.
  keyCtrlB,       ///< ctrl-b: default - move cursor left.
  keyCtrlD,       ///< ctrl-d: default - remove char at right of cursor
  keyCtrlE,       ///< ctrl-e: default - go to the end of the line.
  keyCtrlF,       ///< ctrl-f: default - move cursor right.
  keyCtrlG,       ///< ctrl-g
  keyCtrlH,       ///< ctrl-h
  keyCtrlI,       ///< ctrl-i
  keyCtrlJ,       ///< ctrl-j
  keyCtrlK,       ///< ctrl-k: default - delete from current to end of line.
  keyCtrlL,       ///< ctrl-l: default - clear screen.
  keyCtrlM,       ///< ctrl-m
  keyCtrlN,       ///< ctrl-n: default - history next.
  keyCtrlO,       ///< ctrl-o
  keyCtrlP,       ///< ctrl-p: default - history previous.
  keyCtrlQ,       ///< ctrl-q
  keyCtrlR,       ///< ctrl-r
  keyCtrlS,       ///< ctrl-s
  keyCtrlT,       ///< ctrl-t: default - swaps current character with previous.
  keyCtrlU,       ///< ctrl-u: default - delete the whole line.
  keyCtrlV,       ///< ctrl-v
  keyCtrlW,       ///< ctrl-w
  keyCtrlX,       ///< ctrl-x
  keyCtrlY,       ///< ctrl-y
  keyCtrlZ,       ///< ctrl-z
  keyCtrl0,       ///< ctrl-0
  keyCtrl1,       ///< ctrl-1
  keyCtrl2,       ///< ctrl-2
  keyCtrl3,       ///< ctrl-3
  keyCtrl4,       ///< ctrl-4
  keyCtrl5,       ///< ctrl-5
  keyCtrl6,       ///< ctrl-6
  keyCtrl7,       ///< ctrl-7
  keyCtrl8,       ///< ctrl-8
  keyCtrl9,       ///< ctrl-9
  keyF1,          ///< f1
  keyF2,          ///< f2
  keyF3,          ///< f3
  keyF4,          ///< f4
  keyF5,          ///< f5
  keyF6,          ///< f6
  keyF7,          ///< f7
  keyF8,          ///< f8
  keyF9,          ///< f9
  keyF10,         ///< f10
  keyF11,         ///< f11
  keyF12,         ///< f12
};

/// Text flags
enum TextFlags : uint32_t
{
  defaultText = 0x0,
  boldText = 0x20,
  italicText = 0x40,
  underlineText = 0x80
};

struct Style
{
  uint32_t flags = TextFlags::defaultText;
  uint32_t color = 0xffffff;  // NOLINT(readability-magic-numbers)
};

struct Color
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

[[nodiscard]] constexpr Color toColor(uint32_t color) noexcept
{
  constexpr uint32_t mask = 0x0000ffU;

  const auto r = static_cast<uint8_t>(mask & (color >> 16U));  // NOLINT(readability-magic-numbers)
  const auto g = static_cast<uint8_t>(mask & (color >> 8U));   // NOLINT(readability-magic-numbers)
  const auto b = static_cast<uint8_t>(mask & color);

  return Color {r, g, b};
}

[[nodiscard]] constexpr uint32_t fromColor(const Color& color) noexcept
{
  uint32_t result = 0x000000U;

  result |= static_cast<uint32_t>(color.r << 16U);  // NOLINT(readability-magic-numbers)
  result |= static_cast<uint32_t>(color.g << 8U);   // NOLINT(readability-magic-numbers)
  result |= static_cast<uint32_t>(color.b);

  return result;
}

/// Interface for abstracting away different implementations of a terminal.
class Terminal
{
  SEN_MOVE_ONLY(Terminal)

public:
  Terminal() = default;
  virtual ~Terminal() = default;

public:
  /// Set the window title (if available in the platform).
  virtual void setWindowTitle(std::string_view title) = 0;

  /// Get if the terminal is in raw mode.
  [[nodiscard]] virtual bool getRawModeEnabled() const = 0;

  /// Set the terminal in raw mode (no default line edit cap).
  virtual void enableRawMode() = 0;

  /// Disable the raw mode (if previously enabled).
  virtual void disableRawMode() noexcept = 0;

  /// Save the cursor position in a stack. Use restoreCursorPosition
  /// to recover the last save.
  virtual void saveCursorPosition() = 0;

  /// Restore the cursor position from the last call made to
  /// saveCursorPosition.
  virtual void restoreCursorPosition() = 0;

  /// Move the cursor to the left of the current line.
  virtual void moveCursorAllLeft() = 0;

  /// Move the cursor to the left by a given amount of cells.
  virtual void moveCursorLeft(uint32_t cells) = 0;

  /// Move the cursor to the left
  void moveCursorLeft() { moveCursorLeft(1); }

  /// Move the cursor to the right by a given amount of cells.
  virtual void moveCursorRight(uint32_t cells) = 0;

  /// Move the cursor to the right
  void moveCursorRight() { moveCursorRight(1); }

  /// Move the cursor up by a given amount of cells.
  virtual void moveCursorUp(uint32_t cells) = 0;

  /// Move the cursor up
  void moveCursorUp() { moveCursorUp(1); }

  /// Move the cursor down by a given amount of cells.
  virtual void moveCursorDown(uint32_t cells) = 0;

  /// Move the cursor down.
  void moveCursorDown() { moveCursorDown(1); }

  /// Hide the cursor if it's already being shown.
  virtual void hideCursor() = 0;

  /// Show the cursor if it's not already being shown.
  virtual void showCursor() = 0;

  /// Get the console window size.
  virtual void getSize(uint32_t& rows, uint32_t& cols) const = 0;

  /// Go to a new line.
  virtual void newLine() = 0;

  /// Clean the screen of characters with the current background color.
  virtual void clearScreen() = 0;

  /// Clean the complete current line of characters with the current background color.
  virtual void clearCurrentLine() = 0;

  /// Clean the current line from the current cursor position
  /// to the right with the current background color.
  virtual void clearRemainingCurrentLine() = 0;

  /// Sets the foreground color
  virtual void setFgColor(const Color& color) = 0;

  /// Sets the background color
  virtual void setBgColor(const Color& color) = 0;

  /// Print to the console specifying a text style.
  /// The default style is restored after printing is done.
  virtual void cprint(const Style& style, std::string_view str) = 0;

  /// Print to the console.
  virtual void print(std::string_view str) = 0;

  /// Print to the console.
  virtual void print(char c) = 0;

  /// Get the next character from the user.
  [[nodiscard]] virtual bool getchar(Key& key, char& character, Duration timeout) = 0;

  /// True if this terminal is a real one (not a file).
  [[nodiscard]] virtual bool isARealTerminal() const = 0;

public:
  /// Print to the console.
  void printf(const char* fmt, ...) SEN_PRINTF_FORMAT(2, 3);

  /// Same as print, but adds a newline
  void println(std::string_view str);

  /// Print to the console.
  void vprintf(const char* fmt, va_list args) { print(fromArgList(fmt, args)); }

  /// Same as cprint, but adds a newline
  void cprintln(const Style& style, std::string_view str);

  /// Print to the console specifying a text style.
  /// The default style is restored after printing is done.
  void cprintf(const Style& style, const char* fmt, ...) SEN_PRINTF_FORMAT(3, 4);

  /// Print to the console specifying a text style.
  /// The default style is restored after printing is done.
  void vcprintf(const Style& style, const char* fmt, va_list args) { cprint(style, fromArgList(fmt, args)); }
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline void Terminal::cprintln(const Style& style, std::string_view str)
{
  cprint(style, str);
  newLine();
}

inline void Terminal::println(std::string_view str)
{
  print(str);
  newLine();
}

inline void Terminal::printf(const char* fmt, ...)  // NOLINT
{
  va_list args;  // NOLINT
  va_start(args, fmt);
  std::string result = fromArgList(fmt, args);
  va_end(args);

  print(result);
}

inline void Terminal::cprintf(const Style& style, const char* fmt, ...)  // NOLINT
{
  va_list args;  // NOLINT
  va_start(args, fmt);
  std::string result = fromArgList(fmt, args);
  va_end(args);

  cprint(style, result);
}

}  // namespace sen::components::shell

#endif  // SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_TERMINAL_H
