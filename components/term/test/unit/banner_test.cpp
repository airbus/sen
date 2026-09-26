// === banner_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "banner.h"

// component
#include "test_render_utils.h"

// google test
#include <gtest/gtest.h>

// std
#include <string>

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

TEST(Banner, EveryQuoteAndAuthorFitsTheBannerWidth)
{
  // banner.h states quotes must fit the banner width. This used to be five renders of a random
  // quote asserting that a "-" appeared somewhere, which almost any output satisfies; the table is
  // exposed for testing, so check the property on all of it. The two prefixes mirror renderBanner.
  const auto quotes = getBannerQuotes();
  ASSERT_FALSE(quotes.empty());
  for (const auto& quote: quotes)
  {
    EXPECT_FALSE(quote.text.empty());
    EXPECT_FALSE(quote.author.empty());
    EXPECT_LE(std::string("  ").size() + quote.text.size(), bannerWidth) << quote.text;
    EXPECT_LE(std::string("- ").size() + quote.author.size(), bannerWidth) << quote.author;
  }
}

TEST(Banner, ShowsAQuoteFromTheTableWithItsAuthorDashPrefixed)
{
  auto out = renderToText(renderBanner("1.0.0", "g++", "release"));
  const auto quotes = getBannerQuotes();
  bool matched = false;
  for (const auto& quote: quotes)
  {
    if (out.find(std::string(quote.author)) != std::string::npos)
    {
      matched = true;
      EXPECT_THAT(out, ::testing::HasSubstr("- " + std::string(quote.author)));
      break;
    }
  }
  EXPECT_TRUE(matched) << "the banner showed no author from the quote table";
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
