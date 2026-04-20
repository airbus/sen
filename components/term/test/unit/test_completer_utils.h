// === test_completer_utils.h ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_TEST_UNIT_TEST_COMPLETER_UTILS_H
#define SEN_COMPONENTS_TERM_TEST_UNIT_TEST_COMPLETER_UTILS_H

// local
#include "completer.h"

// std
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace sen::components::term::test
{

/// Extract just the text fields from a completion result.
inline std::vector<std::string> texts(const CompletionResult& result)
{
  std::vector<std::string> out;
  out.reserve(result.candidates.size());
  for (const auto& c: result.candidates)
  {
    out.push_back(c.text);
  }
  return out;
}

/// Check if a text appears in the candidates.
inline bool hasCandidate(const CompletionResult& result, std::string_view text)
{
  return std::any_of(
    result.candidates.begin(), result.candidates.end(), [text](const auto& c) { return c.text == text; });
}

/// Find a candidate by text and return its display field.
inline std::string displayOf(const CompletionResult& result, std::string_view text)
{
  for (const auto& c: result.candidates)
  {
    if (c.text == text)
    {
      return c.display;
    }
  }
  return {};
}

}  // namespace sen::components::term::test

#endif
