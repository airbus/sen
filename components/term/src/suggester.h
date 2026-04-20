// === suggester.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_SUGGESTER_H
#define SEN_COMPONENTS_TERM_SRC_SUGGESTER_H

// sen
#include "sen/core/base/span.h"

// std
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace sen::components::term
{

/// Compute the Levenshtein edit distance between two strings (case-insensitive).
[[nodiscard]] std::size_t editDistance(std::string_view a, std::string_view b);

/// Pick the adaptive edit-distance threshold we allow when suggesting alternatives for a query.
///   len <= 3 -> 1 edit,  len <= 6 -> 2 edits,  len > 6 -> 3 edits.
[[nodiscard]] std::size_t suggestionThreshold(std::size_t queryLength) noexcept;

/// Case-insensitive substring search. Returns true when `needle` appears anywhere in `haystack`.
[[nodiscard]] bool containsCaseInsensitive(std::string_view haystack, std::string_view needle) noexcept;

/// Score a single candidate against a query:
///   - returns 0 if the candidate contains the query (case-insensitive).
///   - otherwise returns the edit distance, or a sentinel > threshold if the candidate should be rejected.
/// The caller inspects the returned score against `threshold` to decide whether to keep the candidate.
[[nodiscard]] std::size_t scoreSuggestion(std::string_view query, std::string_view candidate) noexcept;

/// Return up to `maxSuggestions` candidates closest to `query`, filtered by a distance threshold.
///
/// The threshold is chosen adaptively from the query length:
///   len <= 3 -> 1 edit,  len <= 6 -> 2 edits,  len > 6 -> 3 edits.
/// A candidate that *contains* the query (substring match) is always accepted regardless of distance.
///
/// Results are ordered by distance ascending; ties broken by original order.
[[nodiscard]] std::vector<std::string> findSuggestions(std::string_view query,
                                                       Span<const std::string> candidates,
                                                       std::size_t maxSuggestions = 3);

/// Format a "Did you mean" hint line. Returns empty string when there are no suggestions.
/// Output example:
///   "Did you mean 'ls', 'log'?"
[[nodiscard]] std::string formatSuggestionHint(Span<const std::string> suggestions);

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_SUGGESTER_H
