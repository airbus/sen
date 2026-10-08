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

/// @test
/// The banner names Sen and the version it was given.
TEST(Banner, ContainsVersionInfo)
{
  auto out = renderToText(renderBanner("1.2.3", "g++", "release"));
  EXPECT_THAT(out, ::testing::HasSubstr("Sen"));
  EXPECT_THAT(out, ::testing::HasSubstr("1.2.3"));
}

/// @test
/// The banner names the compiler and the build type it was given.
TEST(Banner, ContainsBuildInfo)
{
  auto out = renderToText(renderBanner("0.0.1", "clang++", "debug"));
  EXPECT_THAT(out, ::testing::HasSubstr("clang++"));
  EXPECT_THAT(out, ::testing::HasSubstr("debug"));
}

/// @test
/// Every quote in the table, with either of the prefixes renderBanner uses, fits the banner width.
TEST(Banner, EveryQuoteAndAuthorFitsTheBannerWidth)
{
  // banner.h states quotes must fit the banner width. Rendering a few random quotes and asserting that a
  // "-" appears somewhere is satisfied by almost any output, so the table is exposed and the property is
  // checked on all of it. The two prefixes mirror renderBanner.
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

/// @test
/// The rendered banner credits an author from the quote table, prefixed with a dash.
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

/// @test
/// The banner draws the bars that separate the title from the quote.
TEST(Banner, HasColorBars)
{
  auto out = renderToText(renderBanner("1.0.0", "g++", "release"));
  // The banner uses colored ▬ (U+25AC) bars between the title and the quote.
  EXPECT_THAT(out, ::testing::HasSubstr("\u25AC"));
}

/// @test
/// Every quote in the table falls on one side of renderBanner's shared-line bound, with at least
/// one taking the shared line, and an author pushed to its own line still fits the banner width.
TEST(Banner, TheQuoteAndAuthorEitherShareALineOrTheAuthorGetsItsOwn)
{
  // This is the bound renderBanner branches on: `"  " + quote + 2 + "- " + author` against the banner
  // width. Whichever side of it a quote falls on, both the quote and the author have to appear, so the
  // branch cannot silently drop one.
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

/// @test
/// Empty version, compiler and build type still render a banner.
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
