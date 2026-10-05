// === text_wrap_test.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "text_wrap.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <string>
#include <string_view>

namespace sen::components::term
{
namespace
{

/// The text one visual line covers.
std::string lineText(std::string_view text, const VisualLine& line)
{
  return std::string(
    text.substr(static_cast<std::size_t>(line.bufStart), static_cast<std::size_t>(line.bufEnd - line.bufStart)));
}

TEST(TextWrap, AnEmptyBufferStillHasTheFirstLine)
{
  auto lines = wrapToWidth("", 60, 80);
  ASSERT_EQ(lines.size(), 1U);
  EXPECT_EQ(lines[0].bufStart, 0);
  EXPECT_EQ(lines[0].bufEnd, 0);
  EXPECT_TRUE(lines[0].isFirst);
}

TEST(TextWrap, ShortAsciiFitsOnOneLine)
{
  auto lines = wrapToWidth("ls -l", 60, 80);
  ASSERT_EQ(lines.size(), 1U);
  EXPECT_EQ(lineText("ls -l", lines[0]), "ls -l");
}

TEST(TextWrap, AsciiBreaksAtTheBudgetAndTheFirstLineIsShorter)
{
  const std::string text(25, 'a');
  auto lines = wrapToWidth(text, 10, 8);
  ASSERT_EQ(lines.size(), 3U);
  EXPECT_EQ(lineText(text, lines[0]).size(), 10U);
  EXPECT_EQ(lineText(text, lines[1]).size(), 8U);
  EXPECT_EQ(lineText(text, lines[2]).size(), 7U);
  EXPECT_TRUE(lines[0].isFirst);
  EXPECT_FALSE(lines[1].isFirst);
}

TEST(TextWrap, WideCharactersCountTwoCellsEach)
{
  // Ten CJK characters are 30 bytes and 20 cells, so a 20-cell line holds exactly ten of them. A
  // byte count wrapped this into three lines and left two thirds of the terminal empty.
  std::string text;
  for (int i = 0; i < 10; ++i)
  {
    text += "日";  // 日, 3 bytes, 2 cells
  }
  ASSERT_EQ(text.size(), 30U);

  auto exact = wrapToWidth(text, 20, 20);
  ASSERT_EQ(exact.size(), 1U);
  EXPECT_EQ(exact[0].bufEnd, 30);

  auto split = wrapToWidth(text, 10, 10);
  ASSERT_EQ(split.size(), 2U);
  EXPECT_EQ(split[0].bufEnd, 15);  // five characters, not five bytes
}

TEST(TextWrap, TwoByteCharactersCountOneCellEach)
{
  // Cyrillic is 2 bytes and 1 cell, so a byte count wrapped at half the real width.
  std::string text;
  for (int i = 0; i < 40; ++i)
  {
    text += "д";  // д
  }
  ASSERT_EQ(text.size(), 80U);
  auto lines = wrapToWidth(text, 40, 40);
  ASSERT_EQ(lines.size(), 1U);
  EXPECT_EQ(lines[0].bufEnd, 80);
}

TEST(TextWrap, NoLineEverBreaksInsideACharacter)
{
  // The failure this guards: a break landing mid-sequence leaves ftxui two partial sequences that it
  // renders as nothing, so a character vanishes from the display while staying in the buffer.
  std::string text;
  for (int i = 0; i < 30; ++i)
  {
    text += "日";
  }

  for (int width = 1; width <= 40; ++width)
  {
    auto lines = wrapToWidth(text, width, width);
    for (const auto& line: lines)
    {
      EXPECT_EQ(line.bufStart % 3, 0) << "width " << width << " broke inside a character";
      EXPECT_EQ(line.bufEnd % 3, 0) << "width " << width << " broke inside a character";
    }
    // And the lines still cover the whole buffer, in order, with no gap.
    int expected = 0;
    for (const auto& line: lines)
    {
      EXPECT_EQ(line.bufStart, expected);
      expected = line.bufEnd;
    }
    EXPECT_EQ(expected, static_cast<int>(text.size()));
  }
}

TEST(TextWrap, ACharacterWiderThanTheBudgetStillAdvances)
{
  // A one-cell budget cannot hold a two-cell character; the line takes it anyway rather than looping.
  std::string text = "日日";
  auto lines = wrapToWidth(text, 1, 1);
  ASSERT_EQ(lines.size(), 2U);
  EXPECT_EQ(lines[0].bufEnd, 3);
  EXPECT_EQ(lines[1].bufEnd, 6);
}

TEST(TextWrap, ANonPositiveWidthIsTreatedAsOne)
{
  auto lines = wrapToWidth("abc", 0, -5);
  ASSERT_EQ(lines.size(), 3U);
  EXPECT_EQ(lineText("abc", lines[0]), "a");
  EXPECT_EQ(lineText("abc", lines[2]), "c");
}

}  // namespace
/// @test
/// With the cursor near the start, the window shows the beginning and hides the tail.
TEST(VisibleLines, CursorNearTheStartShowsTheBeginning)
{
  auto window = visibleLines(10, 1, 5);
  ASSERT_FALSE(window.indices.empty());
  EXPECT_EQ(window.indices.front(), 0) << "the first line is hidden with the cursor on the second";
  EXPECT_LE(window.indices.size(), 5U);
}

/// @test
/// A total of no lines still answers with one. The function's own comment calls this unreachable
/// from wrapToWidth, which always returns a line; it is here because a caller that got it wrong
/// should be given a prompt to draw rather than nothing, and that is worth holding to.
TEST(VisibleLines, NoLinesAtAllStillAnswersWithOne)
{
  auto window = visibleLines(0, 0, 5);
  ASSERT_EQ(window.indices.size(), 1U);
  EXPECT_EQ(window.indices.front(), 0);
}

}  // namespace sen::components::term
