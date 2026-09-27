// === parse_utils.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_PARSE_UTILS_H
#define SEN_COMPONENTS_TERM_SRC_PARSE_UTILS_H

// ftxui
#include <ftxui/screen/string.hpp>

// std
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sen::components::term
{

/// Prefix test. Written out by hand eighteen times in the completer, which is C++17 and so has no
/// std::string_view::starts_with.
[[nodiscard]] inline bool startsWith(std::string_view text, std::string_view prefix)
{
  return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

/// Cuts `text` at or before `maxBytes` without splitting a UTF-8 sequence. Used wherever a value from
/// a peer is put on the screen: the renderers build one glyph per character, so the cut bounds memory
/// as well as width.
[[nodiscard]] inline std::string truncateUtf8(std::string_view text, std::size_t maxBytes)
{
  // A UTF-8 continuation byte is 10xxxxxx. Backing off over them lands on the lead byte of the
  // sequence the cut fell inside, which is then left out.
  constexpr unsigned int continuationMask = 0xC0U;
  constexpr unsigned int continuationMark = 0x80U;

  std::size_t cut = std::min(maxBytes, text.size());
  while (cut > 0U && (static_cast<unsigned char>(text[cut]) & continuationMask) == continuationMark)
  {
    --cut;
  }
  return std::string(text.substr(0, cut));
}

/// Byte offset one codepoint to the left of `pos`. Editing a byte at a time leaves a lone
/// continuation byte behind and makes the buffer invalid UTF-8.
[[nodiscard]] inline std::size_t prevCodepoint(std::string_view text, std::size_t pos)
{
  if (pos == 0U)
  {
    return 0U;
  }
  --pos;
  while (pos > 0U && (static_cast<unsigned char>(text[pos]) & 0xC0U) == 0x80U)
  {
    --pos;
  }
  return pos;
}

/// Byte offset one codepoint to the right of `pos`.
[[nodiscard]] inline std::size_t nextCodepoint(std::string_view text, std::size_t pos)
{
  if (pos >= text.size())
  {
    return text.size();
  }
  ++pos;
  while (pos < text.size() && (static_cast<unsigned char>(text[pos]) & 0xC0U) == 0x80U)
  {
    ++pos;
  }
  return pos;
}

/// The end of the grapheme starting at `pos`: one codepoint, plus any zero-width codepoints after it.
///
/// A combining mark takes no cells and ftxui attaches it to the character before it, discarding one that
/// begins an element. So anything that cuts a string for display has to cut between graphemes, not
/// between codepoints, or the mark is lost from the screen while it stays in the buffer. `wrapToWidth`
/// gets this right by breaking only where a codepoint occupies a cell; the cursor split needs the
/// boundary itself.
[[nodiscard]] inline std::size_t nextGrapheme(std::string_view text, std::size_t pos)
{
  auto at = nextCodepoint(text, pos);
  while (at < text.size())
  {
    const auto next = nextCodepoint(text, at);
    if (ftxui::string_width(text.substr(at, next - at)) > 0)
    {
      break;
    }
    at = next;
  }
  return at;
}

/// Word characters are alphanumerics and underscore, as readline has them, so a dotted object path
/// is several words and Ctrl+Left stops at each segment.
[[nodiscard]] inline bool isWordChar(char c)
{
  auto u = static_cast<unsigned char>(c);
  return (std::isalnum(u) != 0) || c == '_' || u >= 0x80U;
}

/// Byte offset of the start of the word at or before `pos`: skip any separators, then the word.
[[nodiscard]] inline std::size_t prevWord(std::string_view text, std::size_t pos)
{
  pos = std::min(pos, text.size());
  while (pos > 0U && !isWordChar(text[prevCodepoint(text, pos)]))
  {
    pos = prevCodepoint(text, pos);
  }
  while (pos > 0U && isWordChar(text[prevCodepoint(text, pos)]))
  {
    pos = prevCodepoint(text, pos);
  }
  return pos;
}

/// Byte offset just past the word at or after `pos`.
[[nodiscard]] inline std::size_t nextWord(std::string_view text, std::size_t pos)
{
  pos = std::min(pos, text.size());
  while (pos < text.size() && !isWordChar(text[pos]))
  {
    pos = nextCodepoint(text, pos);
  }
  while (pos < text.size() && isWordChar(text[pos]))
  {
    pos = nextCodepoint(text, pos);
  }
  return pos;
}

/// How many codepoints `text` holds. Counting bytes over-reports anything non-ASCII, and this is the
/// third place that knew the continuation-byte test; the others are the two functions above.
[[nodiscard]] inline std::size_t codepointCount(std::string_view text)
{
  std::size_t count = 0;
  for (const char c: text)
  {
    if ((static_cast<unsigned char>(c) & 0xC0U) != 0x80U)
    {
      ++count;
    }
  }
  return count;
}

/// Drop the last codepoint of `text`, if there is one.
inline void eraseLastCodepoint(std::string& text)
{
  if (!text.empty())
  {
    text.resize(prevCodepoint(text, text.size()));
  }
}

/// Which question is being asked of an `object.member` token, because the two answers differ and both
/// are right. Completion must split "obj." -- that is how a user asks for the member list with Tab, and
/// the completer is what puts the dot there in the first place. Execution must not, because "obj."
/// names no member to call. Written out separately, the two accept sets drifted and one carried the
/// other's comment word for word.
enum class TrailingSeparator
{
  reject,  ///< "obj." does not split: there is no member to act on
  accept   ///< "obj." splits, with an empty member half
};

/// Divide an `object.member` token at the last dot. Object paths contain dots and member names do not,
/// so the last one is the boundary.
[[nodiscard]] inline bool splitObjectMember(std::string_view token,
                                            TrailingSeparator trailing,
                                            std::string_view& object,
                                            std::string_view& member)
{
  const auto dot = token.rfind('.');
  if (dot == std::string_view::npos || dot == 0U)
  {
    return false;
  }
  if (trailing == TrailingSeparator::reject && dot == token.size() - 1U)
  {
    return false;
  }
  object = token.substr(0, dot);
  member = token.substr(dot + 1);
  return true;
}

/// Split a command line into the command name and the remaining arguments. Trims leading whitespace
/// from the input and leading and trailing whitespace from the arguments.
inline std::pair<std::string_view, std::string_view> splitCommand(std::string_view input)
{
  if (auto lead = input.find_first_not_of(' '); lead != std::string_view::npos)
  {
    input.remove_prefix(lead);
  }
  else
  {
    return {{}, {}};
  }

  auto pos = input.find_first_of(' ');
  if (pos == std::string_view::npos)
  {
    return {input, {}};
  }

  auto cmd = input.substr(0, pos);
  auto args = input.substr(pos + 1);

  auto start = args.find_first_not_of(' ');
  if (start == std::string_view::npos)
  {
    return {cmd, {}};
  }
  auto end = args.find_last_not_of(' ');
  return {cmd, args.substr(start, end - start + 1)};
}

/// Split arguments into top-level tokens. Respects JSON strings, nested brackets/braces.
/// Whitespace and commas are both valid separators.
inline std::vector<std::string_view> splitTopLevelArgs(std::string_view args)
{
  std::vector<std::string_view> tokens;
  std::size_t tokenStart = std::string_view::npos;
  int depth = 0;
  bool inString = false;
  bool escape = false;

  auto finish = [&](std::size_t end)
  {
    if (tokenStart != std::string_view::npos)
    {
      tokens.push_back(args.substr(tokenStart, end - tokenStart));
      tokenStart = std::string_view::npos;
    }
  };

  for (std::size_t i = 0; i < args.size(); ++i)
  {
    char c = args[i];

    if (escape)
    {
      escape = false;
      continue;
    }
    if (inString)
    {
      if (c == '\\')
      {
        escape = true;
      }
      else if (c == '"')
      {
        inString = false;
      }
      continue;
    }

    if (c == '"')
    {
      if (tokenStart == std::string_view::npos)
      {
        tokenStart = i;
      }
      inString = true;
      continue;
    }
    if (c == '[' || c == '{')
    {
      if (tokenStart == std::string_view::npos)
      {
        tokenStart = i;
      }
      ++depth;
      continue;
    }
    if (c == ']' || c == '}')
    {
      if (tokenStart == std::string_view::npos)
      {
        tokenStart = i;
      }
      // A stray closer used to drive depth negative, which made every later separator invisible and
      // collapsed the whole line into one unparseable token -- reported as a parse failure naming
      // the entire line. Keeping it in the token lets the JSON parser name the real problem.
      if (depth > 0)
      {
        --depth;
      }
      continue;
    }
    if (depth == 0 && (std::isspace(static_cast<unsigned char>(c)) != 0 || c == ','))
    {
      finish(i);
      continue;
    }
    if (tokenStart == std::string_view::npos)
    {
      tokenStart = i;
    }
  }
  finish(args.size());

  return tokens;
}

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_PARSE_UTILS_H
