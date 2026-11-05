// === env_substitution.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "env_substitution.h"

// sen
#include "sen/core/base/assert.h"

// spdlog
#include <spdlog/spdlog.h>

// std
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace sen::kernel::impl
{

std::string replaceEnvPattern(const std::string& content)
{
  // Horizontal whitespace around the comma is accepted: @env(VAR, fallback) is the
  // form the documentation shows, and rejecting it left the text in the configuration
  // with no diagnostic. Spaces only, so a pattern can never span lines.
  //
  // A default holds anything up to the closing parenthesis. Word characters only lost more
  // than the fallback: the group is optional, so /data/repo stopped the whole pattern
  // matching and the variable went unread. Lazy, so trailing spaces stay with the separator.
  const std::regex pattern(R"((\\*)@env\(([a-zA-Z_$][\w]*)([ \t]*,[ \t]*([^)\r\n]*?))?[ \t]*\))");

  auto begin = std::sregex_iterator(content.begin(), content.end(), pattern);
  auto end = std::sregex_iterator();

  std::string result;
  std::string::const_iterator last = content.begin();
  std::vector<std::size_t> expanded;

  for (auto i = begin; i != end; ++i)
  {
    const std::smatch& match = *i;
    result.append(last, match[0].first);
    last = match[0].second;

    const std::string envVar = match[2].str();
    const std::size_t backslashCount = match[1].str().size();
    expanded.push_back(static_cast<std::size_t>(match[0].first - content.begin()) + backslashCount);
    const auto halvedBackslashCount = static_cast<int>(std::round(static_cast<double>(backslashCount) * 0.5));

    // The escape is honoured before the variable is read. Looking it up first made an
    // unset variable abort startup on text that was going to be reproduced verbatim.
    if (backslashCount % 2 != 0)
    {
      result += std::string(halvedBackslashCount - 1, '\\') + "@env(" + envVar;
      if (match[4].matched)
      {
        // Appended separately rather than concatenated: gcc 12 at -O3 -std=c++20
        // reports a false -Wrestrict overlap through operator+(const char*, string&&).
        result += ',';
        result += match[4].str();
      }
      result += ")";
      continue;
    }

    std::string value;
    if (const char* envValue = std::getenv(envVar.c_str()); envValue != nullptr)
    {
      value = std::string(envValue);
    }
    else if (match[4].matched)
    {
      value = match[4].str();
    }
    else
    {
      throwRuntimeError("environment variable " + envVar + " not found and no default value provided");
    }

    result += std::string(halvedBackslashCount, '\\') + value;
  }

  result.append(last, content.cend());

  // Text the expression cannot read stays as it is, because a comment naming the pattern is
  // legitimate and a configuration that loads has to go on loading. It is named in a warning
  // so a mistyped default does not become the value in silence. An escape asks for the text.
  constexpr std::string_view marker = "@env(";
  for (std::size_t pos = content.find(marker); pos != std::string::npos; pos = content.find(marker, pos + 1))
  {
    if (std::binary_search(expanded.begin(), expanded.end(), pos))
    {
      continue;
    }

    std::size_t backslashes = 0;
    while (backslashes < pos && content[pos - backslashes - 1] == '\\')
    {
      ++backslashes;
    }
    if (backslashes % 2 != 0)
    {
      continue;
    }

    const std::size_t lineEnd = std::min(content.find_first_of("\r\n", pos), content.size());
    spdlog::warn("configuration keeps an environment pattern that cannot be read: {}",
                 content.substr(pos, lineEnd - pos));
  }

  return result;
}

}  // namespace sen::kernel::impl
