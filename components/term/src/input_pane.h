// === input_pane.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_INPUT_PANE_H
#define SEN_COMPONENTS_TERM_SRC_INPUT_PANE_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/move_only_function.h"

// std
#include <cstddef>
#include <deque>
#include <filesystem>
#include <string>
#include <string_view>

namespace sen::components::term
{

/// Manages the input area of the terminal.
/// Provides a single-line input with prompt, command history, and submit callback.
class InputPane final
{
  SEN_NOCOPY_NOMOVE(InputPane)

public:
  using SubmitCallback = sen::std_util::move_only_function<void(const std::string&)>;

  explicit InputPane(SubmitCallback onSubmit);

  /// Set the prompt text (e.g., "sen:/local.main> ").
  void setPrompt(std::string_view prompt);

  /// Get the current prompt text.
  [[nodiscard]] std::string_view getPrompt() const noexcept;

  /// Get a mutable reference to the current input buffer (for FTXUI binding).
  [[nodiscard]] std::string& getBuffer() noexcept;

  /// Submit the current buffer and clear it.
  void submit();

  /// Navigate history up (previous command).
  void historyUp();

  /// Navigate history down (next command).
  void historyDown();

  /// Search history for a substring, starting from the most recent entry.
  /// Returns true if a match was found (and sets the buffer to it).
  [[nodiscard]] bool searchHistory(std::string_view query);

  /// Get the in-memory history entries (most recent first).
  [[nodiscard]] const std::deque<std::string>& getHistory() const noexcept;

  /// Add a line to the history (and append to the history file, if one was set).
  void addToHistory(std::string_view line);

  /// Configure the on-disk history file. Must be called before loadHistory().
  void setHistoryFile(std::filesystem::path path);

  /// Load history from the configured file. Silent no-op if the file doesn't exist yet.
  void loadHistory();

private:
  SubmitCallback onSubmit_;
  std::string prompt_ = "sen:/> ";
  std::string buffer_;
  std::deque<std::string> history_;
  int historyIndex_ = -1;
  std::string savedBuffer_;
  std::filesystem::path historyFile_;
  static constexpr std::size_t maxHistoryLines_ = 2000;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_INPUT_PANE_H
