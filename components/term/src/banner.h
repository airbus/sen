// === banner.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_BANNER_H
#define SEN_COMPONENTS_TERM_SRC_BANNER_H

// sen
#include "sen/core/base/span.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// std
#include <cstddef>
#include <string_view>

namespace sen::components::term
{

/// Maximum width (in columns) of the banner area. Quotes must fit within this width
/// (accounting for the 2-character indent). The color bar spans bannerWidth - 2 columns.
constexpr std::size_t bannerWidth = 80;

/// A quote shown in the startup banner.
struct BannerQuote
{
  std::string_view text;
  std::string_view author;
};

/// Get the full list of banner quotes. Exposed for testing.
[[nodiscard]] Span<const BannerQuote> getBannerQuotes();

/// Build the startup banner as an FTXUI element.
/// Compact layout: title with version, build info, color bar, and a random quote.
ftxui::Element renderBanner(std::string_view version, std::string_view compiler, std::string_view buildMode);

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_BANNER_H
