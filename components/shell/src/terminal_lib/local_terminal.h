// === local_terminal.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_LOCAL_TERMINAL_H
#define SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_LOCAL_TERMINAL_H

#include "terminal.h"

// std
#include <memory>

namespace sen::components::shell
{

class LocalTerminalImpl;

/// Process terminal
class LocalTerminal: public Terminal
{
public:
  SEN_MOVE_ONLY(LocalTerminal)

public:
  LocalTerminal();
  ~LocalTerminal() override;

public:  // implements Terminal
  void setWindowTitle(std::string_view title) override;
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
  void restore();
  [[nodiscard]] bool getRawModeEnabled() const override;
  [[nodiscard]] bool getchar(Key& key, char& character, Duration timeout) override;
  [[nodiscard]] bool isARealTerminal() const override;

private:
  std::unique_ptr<LocalTerminalImpl> pimpl_;
};

}  // namespace sen::components::shell

#endif  // SEN_COMPONENTS_SHELL_SRC_TERMINAL_LIB_LOCAL_TERMINAL_H
