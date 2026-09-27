// === unicode.h =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_UNICODE_H
#define SEN_COMPONENTS_TERM_SRC_UNICODE_H

// std
#include <array>

namespace sen::components::term::unicode
{

// Box drawing
constexpr auto* branchTee = "\u251c";  // ├
constexpr auto* cornerEnd = "\u2514";  // └

// Block elements
constexpr auto* blockBar = "\u25AC";  // ▬

// Arrows and pointers
constexpr auto* arrowRight = "\u2192";  // →

// Prompt and markers
constexpr auto* promptSymbol = "\u276f";  // ❯
constexpr auto* bullet = "\u2022";        // •
constexpr auto* ellipsis = "\u2026";      // …
constexpr auto* middleDot = "\u00B7";     // ·
constexpr auto* returnSymbol = "\u23CE";  // ⏎

// Status markers
constexpr auto* check = "\u2713";  // ✓
constexpr auto* cross = "\u2717";  // ✗

// Braille spinner frames for in-flight async indicators
constexpr std::array<const char*, 10> spinnerFrames =
  {"\u280B", "\u2819", "\u2839", "\u2838", "\u283C", "\u2834", "\u2826", "\u2827", "\u2807", "\u280F"};

}  // namespace sen::components::term::unicode

#endif  // SEN_COMPONENTS_TERM_SRC_UNICODE_H
