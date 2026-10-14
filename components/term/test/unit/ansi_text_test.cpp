// === ansi_text_test.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

/// Rendering captured output that carries ANSI escapes.
///
/// Term captures descriptors rather than routing loggers, so a line arrives already formatted by
/// whoever wrote it. spdlog's console sink colours by level, and once nothing carries the level as
/// a value that colour is the only thing left saying a warning is a warning. These check that the
/// styling survives and the escapes themselves never reach the screen as characters.

#include "ansi_text.h"
#include "test_render_utils.h"

// ftxui
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/screen.hpp>

// google test
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// std
#include <string>
#include <tuple>

namespace sen::components::term
{
namespace
{

/// Renders one line and hands back the screen, so a test can ask about a cell's styling as well as
/// its character.
ftxui::Screen renderLine(const std::string& line, int width = 60)
{
  auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(1));
  auto element = renderAnsiLine(line);
  ftxui::Render(screen, element);
  return screen;
}

/// @test
/// A line with no escapes in it is drawn as it stands.
TEST(AnsiText, PlainTextIsDrawnUnchanged)
{
  EXPECT_THAT(test::renderToText(renderAnsiLine("a plain line"), 60, 1), ::testing::HasSubstr("a plain line"));
}

/// @test
/// The escape itself never reaches the screen as characters.
///
/// FTXUI ignores the control byte and draws the rest, so an un-translated `\x1b[32m` reaches the
/// screen as the four characters `[32m`.
TEST(AnsiText, AColourEscapeIsNotDrawnAsText)
{
  const auto drawn = test::renderToText(renderAnsiLine("[\x1b[32minfo\x1b[m] hello"), 60, 1);

  EXPECT_THAT(drawn, ::testing::HasSubstr("[info] hello"));
  EXPECT_THAT(drawn, ::testing::Not(::testing::HasSubstr("32m")));
}

/// @test
/// The colour arrives as styling on the cell rather than as text.
TEST(AnsiText, TheColourReachesTheCell)
{
  const auto screen = renderLine("\x1b[32mgreen");

  EXPECT_EQ(screen.PixelAt(0, 0).character, "g");
  EXPECT_EQ(screen.PixelAt(0, 0).foreground_color, ftxui::Color(ftxui::Color::Green));
}

/// @test
/// spdlog writes its reset as `ESC[m`, with no parameters at all.
///
/// Reading an empty parameter list as "nothing to do" rather than as a reset would leave every
/// line after the first error still red, which is the kind of defect that looks like a theme.
TEST(AnsiText, TheBareResetFormIsAReset)
{
  const auto screen = renderLine("\x1b[31mred\x1b[mplain");

  EXPECT_EQ(screen.PixelAt(0, 0).foreground_color, ftxui::Color(ftxui::Color::Red));
  EXPECT_EQ(screen.PixelAt(3, 0).character, "p");
  EXPECT_EQ(screen.PixelAt(3, 0).foreground_color, ftxui::Color(ftxui::Color::Default));
}

/// @test
/// Bold survives, which is how spdlog distinguishes error from warning.
TEST(AnsiText, BoldReachesTheCell)
{
  const auto screen = renderLine("\x1b[33m\x1b[1mwarn");

  EXPECT_TRUE(screen.PixelAt(0, 0).bold);
  EXPECT_EQ(screen.PixelAt(0, 0).foreground_color, ftxui::Color(ftxui::Color::Yellow));
}

/// @test
/// An escape that is not SGR is dropped with its parameters, not drawn.
///
/// Cursor movement and erase have no meaning inside one line of a pane, and obeying them would let
/// captured output move about in someone else's interface.
TEST(AnsiText, ANonSgrEscapeIsDropped)
{
  const auto drawn = test::renderToText(renderAnsiLine("\x1b[Khello"), 60, 1);

  EXPECT_THAT(drawn, ::testing::HasSubstr("hello"));
  EXPECT_THAT(drawn, ::testing::Not(::testing::HasSubstr("K")));
}

/// @test
/// A sequence cut off by the reader's line cap leaves nothing behind.
TEST(AnsiText, ATruncatedEscapeIsDropped)
{
  const auto drawn = test::renderToText(renderAnsiLine("text\x1b[3"), 60, 1);

  EXPECT_THAT(drawn, ::testing::HasSubstr("text"));
  EXPECT_THAT(drawn, ::testing::Not(::testing::HasSubstr("[3")));
}

/// @test
/// An extended colour's own parameters are not read as codes of their own.
///
/// `38;5;1` is "foreground, 256-colour, index 1". Walking the list naively turns the `5` into
/// blink and the `1` into bold, so a line nobody styled starts flashing.
TEST(AnsiText, AnExtendedColourIsNotReadAsSeparateCodes)
{
  const auto screen = renderLine("\x1b[38;5;1mx");

  EXPECT_EQ(screen.PixelAt(0, 0).character, "x");
  EXPECT_FALSE(screen.PixelAt(0, 0).blink);
  EXPECT_FALSE(screen.PixelAt(0, 0).bold);
}

/// @test
/// A line that is nothing but escapes still occupies its row.
///
/// The pane's line count is what the user scrolls through, so a line that renders to no element at
/// all would silently drop a row.
TEST(AnsiText, ALineOfOnlyEscapesStillRenders)
{
  EXPECT_NO_THROW(std::ignore = test::renderToText(renderAnsiLine("\x1b[32m\x1b[m"), 60, 1));
}

}  // namespace
}  // namespace sen::components::term
