// === parse_utils.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_PARSE_UTILS_H
#define SEN_COMPONENTS_TERM_SRC_PARSE_UTILS_H

// std
#include <cctype>
#include <cstddef>
#include <string_view>
#include <utility>
#include <vector>

namespace sen::components::term
{

/// Split a command line into the command name and the remaining arguments.
/// Trims leading whitespace from the input and leading/trailing whitespace from the arguments.
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
      --depth;
      continue;
    }
    if (depth == 0 && (std::isspace(static_cast<unsigned char>(c)) || c == ','))
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
