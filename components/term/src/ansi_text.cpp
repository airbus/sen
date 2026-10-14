// === ansi_text.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "ansi_text.h"

// ftxui
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

// std
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace sen::components::term
{

namespace
{

/// What one run of text carries. A reset clears all of it at once.
struct Style
{
  std::optional<ftxui::Color> foreground;
  std::optional<ftxui::Color> background;
  bool bold = false;
  bool dim = false;
  bool underlined = false;
  bool blink = false;
  bool inverted = false;
};

/// Palette16 is in ANSI's own order, 0-7 the normal colours and 8-15 the bright ones, so a code
/// selects a colour by subtraction rather than through a table that could fall out of step.
ftxui::Color paletteColour(int offset) { return {static_cast<ftxui::Color::Palette16>(offset)}; }

ftxui::Element styled(const std::string& run, const Style& style)
{
  auto element = ftxui::text(run);
  if (style.foreground.has_value())
  {
    element = ftxui::color(style.foreground.value(), element);
  }
  if (style.background.has_value())
  {
    element = ftxui::bgcolor(style.background.value(), element);
  }
  if (style.bold)
  {
    element = ftxui::bold(element);
  }
  if (style.dim)
  {
    element = ftxui::dim(element);
  }
  if (style.underlined)
  {
    element = ftxui::underlined(element);
  }
  if (style.blink)
  {
    element = ftxui::blink(element);
  }
  if (style.inverted)
  {
    element = ftxui::inverted(element);
  }
  return element;
}

/// What one parameter did, since an extended colour abandons the rest of the sequence.
enum class SgrOutcome : std::uint8_t
{
  applied,
  unknown,
  abandonSequence,
};

/// The number a parameter field holds, or nothing when it is not a code this understands.
///
/// Four digits or more is refused rather than parsed: the widest code here is three digits, and
/// the input is whatever a component wrote to the descriptor, so a longer run would overflow the
/// accumulator.
std::optional<int> fieldCode(std::string_view field)
{
  constexpr std::size_t widestCode = 3U;
  if (field.empty() || field.size() > widestCode)
  {
    return std::nullopt;
  }

  int code = 0;
  for (const char character: field)
  {
    if (character < '0' || character > '9')
    {
      return std::nullopt;
    }
    code = (code * 10) + (character - '0');
  }
  return code;
}

/// Bold, dim, underline, blink and inverse, and the codes that turn each of them off again.
SgrOutcome applyAttribute(int code, Style& style)
{
  switch (code)
  {
    case 0:
      style = Style {};
      break;
    case 1:
      style.bold = true;
      break;
    case 2:
      style.dim = true;
      break;
    case 4:
      style.underlined = true;
      break;
    case 5:
      style.blink = true;
      break;
    case 7:
      style.inverted = true;
      break;
    case 22:
      style.bold = false;
      style.dim = false;
      break;
    case 24:
      style.underlined = false;
      break;
    case 25:
      style.blink = false;
      break;
    case 27:
      style.inverted = false;
      break;
    default:
      return SgrOutcome::unknown;
  }
  return SgrOutcome::applied;
}

/// The foreground and background colours, and the codes that restore the defaults.
///
/// 38 and 48 introduce an extended colour whose own parameters follow in the same list. Reading
/// those as separate codes would turn `38;5;1` into "blink, red". No console sink emits them, so
/// the sequence is abandoned rather than implemented against nothing.
SgrOutcome applyColour(int code, Style& style)
{
  constexpr int brightOffset = 8;

  if (code >= 30 && code <= 37)
  {
    style.foreground = paletteColour(code - 30);
  }
  else if (code >= 40 && code <= 47)
  {
    style.background = paletteColour(code - 40);
  }
  else if (code >= 90 && code <= 97)
  {
    style.foreground = paletteColour(code - 90 + brightOffset);
  }
  else if (code >= 100 && code <= 107)
  {
    style.background = paletteColour(code - 100 + brightOffset);
  }
  else if (code == 39)
  {
    style.foreground.reset();
  }
  else if (code == 49)
  {
    style.background.reset();
  }
  else if (code == 38 || code == 48)
  {
    return SgrOutcome::abandonSequence;
  }
  else
  {
    return SgrOutcome::unknown;
  }
  return SgrOutcome::applied;
}

/// Applies one SGR sequence's parameters. `parameters` is what sits between `ESC[` and `m`.
///
/// An empty parameter list is a reset: spdlog writes its reset as `ESC[m` rather than `ESC[0m`, so
/// reading "no parameters" as "nothing to do" would leave every line after the first error red.
void applySgr(std::string_view parameters, Style& style)
{
  if (parameters.empty())
  {
    style = Style {};
    return;
  }

  std::size_t cursor = 0U;
  while (cursor <= parameters.size())
  {
    const auto separator = parameters.find(';', cursor);
    const auto field = parameters.substr(cursor, separator - cursor);

    // An empty field inside a list means zero, which is a reset, by the same rule as `ESC[m`.
    if (field.empty())
    {
      style = Style {};
    }
    else if (const auto code = fieldCode(field); code.has_value())
    {
      auto outcome = applyAttribute(*code, style);
      if (outcome == SgrOutcome::unknown)
      {
        outcome = applyColour(*code, style);
      }
      if (outcome == SgrOutcome::abandonSequence)
      {
        return;
      }
      // An unknown code is left alone: it should lose its styling, not the line.
    }

    if (separator == std::string_view::npos)
    {
      return;
    }
    cursor = separator + 1U;
  }
}

}  // namespace

ftxui::Element renderAnsiLine(const std::string& line)
{
  ftxui::Elements runs;
  Style style;
  std::string pending;

  std::size_t index = 0U;
  while (index < line.size())
  {
    const bool startsEscape = line[index] == '\x1b' && index + 1U < line.size() && line[index + 1U] == '[';
    if (!startsEscape)
    {
      pending += line[index];
      ++index;
      continue;
    }

    // A CSI sequence runs to its first byte in the range @ to ~, which is what says where it ends.
    auto end = index + 2U;
    while (end < line.size() && (line[end] < '@' || line[end] > '~'))
    {
      ++end;
    }

    if (end >= line.size())
    {
      // Truncated, which happens when the reader splits a line at its length cap. Dropped rather
      // than shown: half an escape is not text anyone wants to read.
      break;
    }

    if (line[end] == 'm')
    {
      if (!pending.empty())
      {
        runs.push_back(styled(pending, style));
        pending.clear();
      }
      applySgr(std::string_view(line).substr(index + 2U, end - (index + 2U)), style);
    }
    // Any other final byte is a cursor move, an erase, or something else that has no meaning
    // inside one line of a pane, and is dropped with its parameters.

    index = end + 1U;
  }

  if (!pending.empty())
  {
    runs.push_back(styled(pending, style));
  }

  if (runs.empty())
  {
    // hbox of nothing renders nothing at all, and a line that was only escapes should still take
    // its row: the pane's line count is what the user scrolls through.
    runs.push_back(ftxui::text(""));
  }

  return ftxui::hbox(std::move(runs));
}

}  // namespace sen::components::term
