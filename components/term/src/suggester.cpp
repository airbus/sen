// === suggester.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "suggester.h"

// std
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <utility>
#include <vector>

namespace sen::components::term
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

char toLowerChar(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Public API
//--------------------------------------------------------------------------------------------------------------

bool containsCaseInsensitive(std::string_view haystack, std::string_view needle) noexcept
{
  if (needle.empty())
  {
    return true;
  }
  if (needle.size() > haystack.size())
  {
    return false;
  }
  for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i)
  {
    bool match = true;
    for (std::size_t j = 0; j < needle.size(); ++j)
    {
      if (toLowerChar(haystack[i + j]) != toLowerChar(needle[j]))
      {
        match = false;
        break;
      }
    }
    if (match)
    {
      return true;
    }
  }
  return false;
}

std::size_t suggestionThreshold(std::size_t queryLength) noexcept
{
  if (queryLength <= 3)
  {
    return 1;
  }
  if (queryLength <= 6)
  {
    return 2;
  }
  return 3;
}

std::size_t scoreSuggestion(std::string_view query, std::string_view candidate) noexcept
{
  if (containsCaseInsensitive(candidate, query))
  {
    return 0;
  }
  return editDistance(query, candidate);
}

std::size_t editDistance(std::string_view a, std::string_view b)
{
  const auto m = a.size();
  const auto n = b.size();

  if (m == 0)
  {
    return n;
  }
  if (n == 0)
  {
    return m;
  }

  // Two-row DP for minimum edit distance; O(min(m,n)) memory.
  std::vector<std::size_t> prev(n + 1);
  std::vector<std::size_t> curr(n + 1);
  for (std::size_t j = 0; j <= n; ++j)
  {
    prev[j] = j;
  }

  for (std::size_t i = 1; i <= m; ++i)
  {
    curr[0] = i;
    const auto ca = toLowerChar(a[i - 1]);
    for (std::size_t j = 1; j <= n; ++j)
    {
      const auto cb = toLowerChar(b[j - 1]);
      const auto substitutionCost = (ca == cb) ? 0U : 1U;
      curr[j] = std::min({
        prev[j] + 1U,                   // deletion
        curr[j - 1] + 1U,               // insertion
        prev[j - 1] + substitutionCost  // substitution
      });
    }
    std::swap(prev, curr);
  }

  return prev[n];
}

std::vector<std::string> findSuggestions(std::string_view query,
                                         Span<const std::string> candidates,
                                         std::size_t maxSuggestions)
{
  if (query.empty() || candidates.empty() || maxSuggestions == 0)
  {
    return {};
  }

  const auto threshold = suggestionThreshold(query.size());

  // Collect (distance, originalIndex, name) for each candidate that passes the filter.
  struct Scored
  {
    std::size_t distance;
    std::size_t order;  // preserve original candidate order for stable ties
    const std::string* name;
  };
  std::vector<Scored> scored;
  scored.reserve(candidates.size());

  for (std::size_t idx = 0; idx < candidates.size(); ++idx)
  {
    const auto& c = candidates[idx];
    if (c.empty())
    {
      continue;
    }
    auto d = scoreSuggestion(query, c);
    if (d <= threshold)
    {
      scored.push_back({d, idx, &c});
    }
  }

  std::sort(scored.begin(),
            scored.end(),
            [](const Scored& x, const Scored& y)
            {
              if (x.distance != y.distance)
              {
                return x.distance < y.distance;
              }
              return x.order < y.order;
            });

  if (scored.size() > maxSuggestions)
  {
    scored.resize(maxSuggestions);
  }

  std::vector<std::string> result;
  result.reserve(scored.size());
  for (const auto& s: scored)
  {
    result.push_back(*s.name);
  }
  return result;
}

std::string formatSuggestionHint(Span<const std::string> suggestions)
{
  if (suggestions.empty())
  {
    return {};
  }
  std::string out = "Did you mean ";
  for (std::size_t i = 0; i < suggestions.size(); ++i)
  {
    if (i > 0)
    {
      out += (i + 1 == suggestions.size()) ? " or " : ", ";
    }
    out += "'";
    out += suggestions[i];
    out += "'";
  }
  out += "?";
  return out;
}

}  // namespace sen::components::term
