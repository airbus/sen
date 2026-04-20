// === banner_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "banner.h"

// local
#include "test_render_utils.h"

// google test
#include <gtest/gtest.h>

namespace sen::components::term
{
namespace
{

using test::renderToText;

TEST(Banner, ContainsVersionInfo)
{
  auto out = renderToText(renderBanner("1.2.3", "g++", "release"));
  EXPECT_THAT(out, ::testing::HasSubstr("Sen"));
  EXPECT_THAT(out, ::testing::HasSubstr("1.2.3"));
}

TEST(Banner, ContainsBuildInfo)
{
  auto out = renderToText(renderBanner("0.0.1", "clang++", "debug"));
  EXPECT_THAT(out, ::testing::HasSubstr("clang++"));
  EXPECT_THAT(out, ::testing::HasSubstr("debug"));
}

TEST(Banner, ContainsQuote)
{
  // The banner always includes a quote with a dash-prefixed author.
  // Run it several times to increase chance of catching formatting issues.
  for (int i = 0; i < 5; ++i)
  {
    auto out = renderToText(renderBanner("1.0.0", "g++", "release"));
    EXPECT_THAT(out, ::testing::HasSubstr("-"));
  }
}

TEST(Banner, HasColorBars)
{
  auto out = renderToText(renderBanner("1.0.0", "g++", "release"));
  // The banner uses colored ▬ (U+25AC) bars between the title and the quote.
  EXPECT_THAT(out, ::testing::HasSubstr("\u25AC"));
}

TEST(Banner, AllQuotesFitWithinBannerWidth)
{
  constexpr std::size_t indent = 4;  // "  " prefix added by the renderer
  constexpr auto maxQuoteLen = bannerWidth - indent;
  for (const auto& q: getBannerQuotes())
  {
    EXPECT_LE(q.text.size(), maxQuoteLen)
      << "Quote too long (" << q.text.size() << " > " << maxQuoteLen << "): " << q.text;
  }
}

TEST(Banner, DoesNotCrashWithEmptyFields)
{
  // Should render without crashing even with empty strings
  auto element = renderBanner("", "", "");
  ASSERT_NE(element, nullptr);
  auto out = renderToText(element);
  EXPECT_THAT(out, ::testing::HasSubstr("Sen"));
}

}  // namespace
}  // namespace sen::components::term
