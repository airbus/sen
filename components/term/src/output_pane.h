// === output_pane.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_OUTPUT_PANE_H
#define SEN_COMPONENTS_TERM_SRC_OUTPUT_PANE_H

// sen
#include "sen/core/base/compiler_macros.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// std
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sen::components::term
{

/// Manages the scrollable output area of the terminal.
/// All content is append-only; new lines are added at the bottom.
class OutputPane final
{
  SEN_NOCOPY_NOMOVE(OutputPane)

public:
  OutputPane() = default;

  /// Append a plain text line.
  void appendText(std::string_view text);

  /// Append a styled information line (dimmed, with marker).
  void appendInfo(std::string_view text);

  /// Append a command echo line (shows what the user typed).
  void appendCommand(std::string_view command);

  /// Append a pre-built FTXUI element directly.
  void appendElement(ftxui::Element element);

  /// Append a pending call indicator. Will show an animated spinner until
  /// replacePendingCall() is called with the same id.
  void appendPendingCall(std::size_t id, std::string description);

  /// Replace a pending call indicator with its final result.
  /// Does nothing if the id is not found (e.g., trimmed by maxLines cap).
  void replacePendingCall(std::size_t id, ftxui::Element result);

  /// Returns true if there are any pending calls that still need rendering.
  [[nodiscard]] bool hasPendingCalls() const;

  /// Clear all output.
  void clear();

  /// Scroll up by a number of lines.
  void scrollUp(int lines = 3);

  /// Scroll down by a number of lines.
  void scrollDown(int lines = 3);

  /// Scroll to the bottom (follow mode).
  void scrollToBottom();

  /// Set the maximum number of lines to retain. 0 means unlimited.
  void setMaxLines(std::size_t maxLines);

  /// When true, short content is pushed to the bottom of the pane (REPL style).
  void setBottomAligned(bool bottomAligned) noexcept;

  /// Render the output pane as an FTXUI element.
  [[nodiscard]] ftxui::Element render();

private:
  void trimLines();

private:
  struct PendingEntry
  {
    std::size_t id;
    std::string description;
    std::chrono::steady_clock::time_point startTime;
  };
  using Entry = std::variant<ftxui::Element, PendingEntry>;

  std::vector<Entry> lines_;
  float scrollPosition_ = 1.0F;  // 0.0 = top, 1.0 = bottom
  bool followBottom_ = true;
  std::size_t maxLines_ = 0;  // 0 = unlimited
  std::size_t pendingCount_ = 0;
  int contentHeight_ = 1;  // total rows from last render, for scroll step computation
  bool bottomAligned_ = false;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_OUTPUT_PANE_H
