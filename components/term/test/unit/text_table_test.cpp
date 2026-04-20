// === text_table_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "test_render_utils.h"
#include "text_table.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// google test
#include <gtest/gtest.h>

namespace sen::components::term
{
namespace
{

using textTable::Cell;
using textTable::Row;

std::string renderText(std::vector<Row> rows, int width = 80, int height = 10, int gap = 2)
{
  return test::renderToText(textTable::render(std::move(rows), gap), width, height);
}

//--------------------------------------------------------------------------------------------------------------
// Basic layout
//--------------------------------------------------------------------------------------------------------------

TEST(TextTable, EmptyRowsRenderAsEmpty)
{
  auto out = renderText({});
  // An empty vbox still produces a blank canvas, no cell text.
  EXPECT_THAT(out, ::testing::Not(::testing::HasSubstr("x")));
}

TEST(TextTable, SingleRowShowsAllCellsInOrder)
{
  std::vector<Row> rows {
    Row {Cell {"alpha"}, Cell {"beta"}, Cell {"gamma"}},
  };
  auto out = renderText(rows);
  auto pos_a = out.find("alpha");
  auto pos_b = out.find("beta");
  auto pos_g = out.find("gamma");
  ASSERT_NE(pos_a, std::string::npos);
  ASSERT_NE(pos_b, std::string::npos);
  ASSERT_NE(pos_g, std::string::npos);
  EXPECT_LT(pos_a, pos_b);
  EXPECT_LT(pos_b, pos_g);
}

//--------------------------------------------------------------------------------------------------------------
// Column alignment
//--------------------------------------------------------------------------------------------------------------

TEST(TextTable, ColumnsAlignToWidestCell)
{
  // Two rows, first column widths 3 and 7 -> longer row dictates alignment.
  std::vector<Row> rows {
    Row {Cell {"abc"}, Cell {"X"}},
    Row {Cell {"abcdefg"}, Cell {"Y"}},
  };
  auto out = renderText(rows, 40, 4, 2);

  // Second column must appear at the same column in both rows. Locate "X" and
  // "Y" and assert their column offset matches after dropping the leading line
  // padding added by the render canvas.
  auto xPos = out.find('X');
  auto yPos = out.find('Y');
  ASSERT_NE(xPos, std::string::npos);
  ASSERT_NE(yPos, std::string::npos);

  auto lineStart = [&](std::size_t pos) -> std::size_t
  {
    auto nl = out.rfind('\n', pos);
    return nl == std::string::npos ? 0U : nl + 1U;
  };
  EXPECT_EQ(xPos - lineStart(xPos), yPos - lineStart(yPos));
}

TEST(TextTable, ColumnGapSeparatesCells)
{
  std::vector<Row> rows {
    Row {Cell {"a"}, Cell {"b"}},
  };
  auto narrow = renderText(rows, 40, 2, 1);
  auto wide = renderText(rows, 40, 2, 5);

  // With gap 1: "a b"; with gap 5: "a     b".
  auto gapNarrow = narrow.find('b') - narrow.find('a') - 1U;
  auto gapWide = wide.find('b') - wide.find('a') - 1U;
  EXPECT_EQ(gapNarrow, 1U);
  EXPECT_EQ(gapWide, 5U);
}

//--------------------------------------------------------------------------------------------------------------
// Ragged rows / custom cells
//--------------------------------------------------------------------------------------------------------------

TEST(TextTable, RowsOfDifferentLengthStillRender)
{
  std::vector<Row> rows {
    Row {Cell {"a"}, Cell {"b"}, Cell {"c"}},
    Row {Cell {"d"}},
  };
  auto out = renderText(rows);
  EXPECT_THAT(out, ::testing::HasSubstr("a"));
  EXPECT_THAT(out, ::testing::HasSubstr("b"));
  EXPECT_THAT(out, ::testing::HasSubstr("c"));
  EXPECT_THAT(out, ::testing::HasSubstr("d"));
}

TEST(TextTable, EmptyRowRendersAsSeparator)
{
  // An empty Row becomes a horizontal separator.
  std::vector<Row> rows {
    Row {Cell {"top"}},
    Row {},
    Row {Cell {"bottom"}},
  };
  auto out = renderText(rows, 20, 5, 2);
  EXPECT_THAT(out, ::testing::HasSubstr("top"));
  EXPECT_THAT(out, ::testing::HasSubstr("bottom"));
  // FTXUI separators draw with the horizontal box-drawing character.
  EXPECT_THAT(out, ::testing::HasSubstr("\u2500"));
}

TEST(TextTable, CustomCellWidthSourceWidensOtherRows)
{
  // A custom cell declaring widthSource="abcdefg" (width 7) widens column 0
  // so that a shorter cell in another row is padded out to match. The
  // second column of the short row must end up at least `width + gap`
  // columns from the line start.
  auto composite = ftxui::hbox({ftxui::text("abc"), ftxui::text("defg")});
  std::vector<Row> rows {
    Row {Cell {"a"}, Cell {"tail"}},
    Row {Cell {"abcdefg", std::move(composite)}, Cell {"other"}},
  };
  auto out = renderText(rows, 40, 4, 2);

  auto tailPos = out.find("tail");
  ASSERT_NE(tailPos, std::string::npos);
  auto nl = out.rfind('\n', tailPos);
  auto col = tailPos - (nl == std::string::npos ? 0U : nl + 1U);
  // Column 0 must have been padded to width 7 (+ 2 for gap) before "tail".
  EXPECT_GE(col, 9U);
}

}  // namespace
}  // namespace sen::components::term
