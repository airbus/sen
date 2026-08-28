// === env_substitution.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "env_substitution.h"

// sen
#include "sen/core/base/assert.h"

// std
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <regex>
#include <string>

namespace sen::kernel::impl
{

std::string replaceEnvPattern(const std::string& content)
{
  // Horizontal whitespace around the comma is accepted: @env(VAR, fallback) is the
  // form the documentation shows, and rejecting it left the text in the configuration
  // with no diagnostic. Spaces only, so a pattern can never span lines.
  const std::regex pattern(R"((\\*)@env\(([a-zA-Z_$][\w]*)([ \t]*,[ \t]*([\w]+))?[ \t]*\))");

  auto begin = std::sregex_iterator(content.begin(), content.end(), pattern);
  auto end = std::sregex_iterator();

  std::string result;
  std::string::const_iterator last = content.begin();

  for (auto i = begin; i != end; ++i)
  {
    const std::smatch& match = *i;
    result.append(last, match[0].first);
    last = match[0].second;

    const std::string envVar = match[2].str();
    const std::size_t backslashCount = match[1].str().size();
    const auto halvedBackslashCount = static_cast<int>(std::round(static_cast<double>(backslashCount) * 0.5));

    // The escape is honoured before the variable is read. Looking it up first made an
    // unset variable abort startup on text that was going to be reproduced verbatim.
    if (backslashCount % 2 != 0)
    {
      result += std::string(halvedBackslashCount - 1, '\\') + "@env(" + envVar;
      if (match[4].matched)
      {
        result += "," + match[4].str();
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
  return result;
}

}  // namespace sen::kernel::impl
