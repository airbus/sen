// === text_wrap.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_TEXT_WRAP_H
#define SEN_COMPONENTS_TERM_SRC_TEXT_WRAP_H

#include "parse_utils.h"

// ftxui
#include <ftxui/screen/string.hpp>

// std
#include <algorithm>
#include <string_view>
#include <vector>

namespace sen::components::term
{

/// One visual line of a soft-wrapped buffer, as a byte range into it.
struct VisualLine
{
  int bufStart;  // index into the buffer where this visual line starts
  int bufEnd;    // one past its end
  bool isFirst;  // the first visual line, which carries the prompt
};

/// Soft-wrap `text` into lines of `firstWidth` display cells for the first and `contWidth` for the
/// rest, breaking only between codepoints. Always returns at least one line, so an empty buffer
/// still draws a prompt and a cursor.
///
/// Widths are cells, not bytes: a byte count wraps a CJK or Cyrillic line about half a screen early,
/// and a break inside a multi-byte sequence leaves two partial sequences that ftxui renders as
/// nothing, so the character disappears from the display while staying in the buffer.
[[nodiscard]] inline std::vector<VisualLine> wrapToWidth(std::string_view text, int firstWidth, int contWidth)
{
  const int firstBudget = std::max(1, firstWidth);
  const int contBudget = std::max(1, contWidth);

  std::vector<VisualLine> lines;
  int lineStart = 0;
  int budget = firstBudget;
  int used = 0;
  bool isFirst = true;
  std::size_t at = 0;

  while (at < text.size())
  {
    const auto next = nextCodepoint(text, at);
    // A combining mark costs no cells and must never start a line: ftxui attaches a mark to the
    // character before it and discards one that begins a segment, so a break there loses the accent
    // from the display while it stays in the buffer. Breaking only where a codepoint occupies a cell
    // gives both: the pair is never split, and the mark is not charged for width it does not take. The
    // loop advances on `next` regardless, so a run of marks cannot spin it.
    const int cells = ftxui::string_width(text.substr(at, next - at));
    if (cells > 0 && used + cells > budget && at > static_cast<std::size_t>(lineStart))
    {
      lines.push_back({lineStart, static_cast<int>(at), isFirst});
      isFirst = false;
      lineStart = static_cast<int>(at);
      budget = contBudget;
      used = 0;
    }
    used += cells;
    at = next;
  }

  lines.push_back({lineStart, static_cast<int>(text.size()), isFirst});
  return lines;
}

/// Which visual lines to draw, and whether any were left out.
struct VisibleLines
{
  std::vector<int> indices;  ///< indices into the wrapped lines, in order
  bool elided = false;       ///< true when some line is not shown
};

/// Choose at most `maxRows` visual lines to draw out of `total`, always including the first, which
/// carries the prompt, and always the cursor's.
///
/// Drawing every line of a long input takes the whole terminal: ftxui's hard shrink then gives the output
/// pane zero rows, so the user sees no output, and no cursor line either, because the input is laid out
/// from the top of its box.
[[nodiscard]] inline VisibleLines visibleLines(int total, int cursorLine, int maxRows)
{
  VisibleLines out;
  if (total <= 0)
  {
    // No line at all would draw no prompt. wrapToWidth always returns one, so this is unreachable;
    // it is here because an empty answer is the wrong one for a caller that got it wrong.
    out.indices.push_back(0);
    return out;
  }

  // The prompt's line, one line of text, and the row saying what is hidden.
  maxRows = std::max(3, maxRows);

  if (total <= maxRows)
  {
    for (int i = 0; i < total; ++i)
    {
      out.indices.push_back(i);
    }
    return out;
  }

  cursorLine = std::clamp(cursorLine, 0, total - 1);
  const int window = maxRows - 1;  // one row goes to the indicator

  if (cursorLine < window)
  {
    // The cursor is near the start, so show the beginning and hide the tail.
    for (int i = 0; i < window; ++i)
    {
      out.indices.push_back(i);
    }
  }
  else
  {
    // Keep the prompt's line, then the rows ending at the cursor's.
    out.indices.push_back(0);
    for (int i = cursorLine - (window - 2); i <= cursorLine; ++i)
    {
      out.indices.push_back(i);
    }
  }

  out.elided = true;
  return out;
}

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_TEXT_WRAP_H
