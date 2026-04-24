// === mock_terminal.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_SHELL_TEST_MOCK_TERMINAL_H
#define SEN_COMPONENTS_SHELL_TEST_MOCK_TERMINAL_H

// component
#include "terminal.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"

// google test
#include <gmock/gmock.h>

// std
#include <cstdint>
#include <string>
#include <string_view>

namespace sen::components::shell::test
{

class MockTerminal: public Terminal
{
  SEN_NOCOPY_NOMOVE(MockTerminal)

public:
  MockTerminal() = default;
  ~MockTerminal() override = default;

  MOCK_METHOD(void, setWindowTitle, (std::string_view), (override));
  MOCK_METHOD(bool, getRawModeEnabled, (), (const, override));
  MOCK_METHOD(void, enableRawMode, (), (override));
  MOCK_METHOD(void, disableRawMode, (), (noexcept, override));  // NOLINT(bugprone-exception-escape)
  MOCK_METHOD(void, saveCursorPosition, (), (override));
  MOCK_METHOD(void, restoreCursorPosition, (), (override));
  MOCK_METHOD(void, moveCursorAllLeft, (), (override));
  MOCK_METHOD(void, moveCursorLeft, (uint32_t), (override));
  MOCK_METHOD(void, moveCursorRight, (uint32_t), (override));
  MOCK_METHOD(void, moveCursorUp, (uint32_t), (override));
  MOCK_METHOD(void, moveCursorDown, (uint32_t), (override));
  MOCK_METHOD(void, hideCursor, (), (override));
  MOCK_METHOD(void, showCursor, (), (override));
  MOCK_METHOD(void, getSize, (uint32_t&, uint32_t&), (const, override));

  MOCK_METHOD(void, clearScreen, (), (override));
  MOCK_METHOD(void, clearCurrentLine, (), (override));
  MOCK_METHOD(void, clearRemainingCurrentLine, (), (override));
  MOCK_METHOD(void, setFgColor, (const Color&), (override));
  MOCK_METHOD(void, setBgColor, (const Color&), (override));
  MOCK_METHOD(bool, getchar, (Key&, char&, Duration), (override));
  MOCK_METHOD(bool, isARealTerminal, (), (const, override));

  void newLine() override { outputBuffer_ += "\n"; }
  void print(const std::string_view str) override { outputBuffer_ += str; }
  void print(const char character) override { outputBuffer_ += character; }
  void cprint(const Style& /*style*/, const std::string_view str) override { outputBuffer_ += str; }

  [[nodiscard]] const std::string& getOutputBuffer() const { return outputBuffer_; }
  void clearOutputBuffer() { outputBuffer_.clear(); }

private:
  std::string outputBuffer_;
};

}  // namespace sen::components::shell::test

#endif  // SEN_COMPONENTS_SHELL_TEST_MOCK_TERMINAL_H
