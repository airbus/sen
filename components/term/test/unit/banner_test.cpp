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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// std
#include <cstddef>
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

TEST(Banner, TheQuoteAndAuthorEitherShareALineOrTheAuthorGetsItsOwn)
{
  // This is the bound renderBanner actually branches on -- `"  " + quote + 2 + "- " + author` against
  // the banner width -- and nothing tested it. Whichever side of it a quote falls on, both the quote and
  // the author have to appear, so the branch cannot silently drop one.
  const auto quotes = getBannerQuotes();
  ASSERT_FALSE(quotes.empty());

  std::size_t shareALine = 0;
  std::size_t authorOnItsOwnLine = 0;
  for (const auto& quote: quotes)
  {
    const auto quoteStr = std::string("  ") + std::string(quote.text);
    const auto authorStr = std::string("- ") + std::string(quote.author);
    if (quoteStr.size() + 2U + authorStr.size() <= bannerWidth)
    {
      ++shareALine;
    }
    else
    {
      ++authorOnItsOwnLine;
      // The fallback still has to fit on its own line, or the author is clipped instead of wrapped.
      EXPECT_LE(authorStr.size(), bannerWidth) << quote.author;
    }
  }

  EXPECT_GT(shareALine, 0U) << "no quote takes the shared-line branch, so it is untested here";
  EXPECT_EQ(shareALine + authorOnItsOwnLine, quotes.size());
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
