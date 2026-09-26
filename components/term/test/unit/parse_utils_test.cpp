// === parse_utils_test.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "parse_utils.h"

// google test
#include <gtest/gtest.h>

namespace sen::components::term
{
namespace
{

TEST(SplitCommand, SimpleCommand)
{
  auto [cmd, args] = splitCommand("help");
  EXPECT_EQ(cmd, "help");
  EXPECT_EQ(args, "");
}

TEST(SplitCommand, CommandWithArg)
{
  auto [cmd, args] = splitCommand("cd local.main");
  EXPECT_EQ(cmd, "cd");
  EXPECT_EQ(args, "local.main");
}

TEST(SplitCommand, CommandWithMultipleArgs)
{
  auto [cmd, args] = splitCommand("query sensors SELECT * FROM local.main");
  EXPECT_EQ(cmd, "query");
  EXPECT_EQ(args, "sensors SELECT * FROM local.main");
}

TEST(SplitCommand, TrailingWhitespace)
{
  auto [cmd, args] = splitCommand("cd local.demo   ");
  EXPECT_EQ(cmd, "cd");
  EXPECT_EQ(args, "local.demo");
}

TEST(SplitCommand, LeadingWhitespaceInArgs)
{
  auto [cmd, args] = splitCommand("cd   local.demo");
  EXPECT_EQ(cmd, "cd");
  EXPECT_EQ(args, "local.demo");
}

TEST(SplitCommand, BothLeadingAndTrailingWhitespace)
{
  auto [cmd, args] = splitCommand("cd   local.demo   ");
  EXPECT_EQ(cmd, "cd");
  EXPECT_EQ(args, "local.demo");
}

TEST(SplitCommand, OnlySpacesAfterCommand)
{
  auto [cmd, args] = splitCommand("ls   ");
  EXPECT_EQ(cmd, "ls");
  EXPECT_EQ(args, "");
}

TEST(SplitCommand, EmptyInput)
{
  auto [cmd, args] = splitCommand("");
  EXPECT_EQ(cmd, "");
  EXPECT_EQ(args, "");
}

TEST(SplitCommand, DotInCommand)
{
  auto [cmd, args] = splitCommand("local.demo.slowLogger.getRate");
  EXPECT_EQ(cmd, "local.demo.slowLogger.getRate");
  EXPECT_EQ(args, "");
}

TEST(SplitCommand, DotInCommandWithArgs)
{
  auto [cmd, args] = splitCommand("obj.method arg1 arg2");
  EXPECT_EQ(cmd, "obj.method");
  EXPECT_EQ(args, "arg1 arg2");
}

TEST(SplitCommand, DoubleDot)
{
  auto [cmd, args] = splitCommand("cd ..");
  EXPECT_EQ(cmd, "cd");
  EXPECT_EQ(args, "..");
}

//--------------------------------------------------------------------------------------------------------------
// splitTopLevelArgs, accepts both space and comma separators
//--------------------------------------------------------------------------------------------------------------

TEST(SplitTopLevelArgs, EmptyString) { EXPECT_TRUE(splitTopLevelArgs("").empty()); }

TEST(SplitTopLevelArgs, SingleToken)
{
  auto t = splitTopLevelArgs("42");
  ASSERT_EQ(t.size(), 1U);
  EXPECT_EQ(t[0], "42");
}

TEST(SplitTopLevelArgs, SpaceSeparated)
{
  auto t = splitTopLevelArgs("1 2 3");
  ASSERT_EQ(t.size(), 3U);
  EXPECT_EQ(t[0], "1");
  EXPECT_EQ(t[1], "2");
  EXPECT_EQ(t[2], "3");
}

TEST(SplitTopLevelArgs, CommaSeparated)
{
  auto t = splitTopLevelArgs("1,2,3");
  ASSERT_EQ(t.size(), 3U);
  EXPECT_EQ(t[0], "1");
  EXPECT_EQ(t[1], "2");
  EXPECT_EQ(t[2], "3");
}

TEST(SplitTopLevelArgs, MixedSeparators)
{
  auto t = splitTopLevelArgs("1, 2 ,3 4");
  ASSERT_EQ(t.size(), 4U);
  EXPECT_EQ(t[0], "1");
  EXPECT_EQ(t[3], "4");
}

TEST(SplitTopLevelArgs, StringWithSpacesKeepsTogether)
{
  auto t = splitTopLevelArgs(R"(42 "hello world" true)");
  ASSERT_EQ(t.size(), 3U);
  EXPECT_EQ(t[0], "42");
  EXPECT_EQ(t[1], R"("hello world")");
  EXPECT_EQ(t[2], "true");
}

TEST(SplitTopLevelArgs, StringWithEscapedQuote)
{
  auto t = splitTopLevelArgs(R"("a\"b" 5)");
  ASSERT_EQ(t.size(), 2U);
  EXPECT_EQ(t[0], R"("a\"b")");
  EXPECT_EQ(t[1], "5");
}

TEST(SplitTopLevelArgs, BracketedContentStaysOneToken)
{
  auto t = splitTopLevelArgs("[1, 2, 3] next");
  ASSERT_EQ(t.size(), 2U);
  EXPECT_EQ(t[0], "[1, 2, 3]");
  EXPECT_EQ(t[1], "next");
}

TEST(SplitTopLevelArgs, NestedBracesStayOneToken)
{
  auto t = splitTopLevelArgs(R"({"x": 1, "y": 2} "end")");
  ASSERT_EQ(t.size(), 2U);
  EXPECT_EQ(t[0], R"({"x": 1, "y": 2})");
  EXPECT_EQ(t[1], R"("end")");
}

TEST(SplitTopLevelArgs, LeadingAndTrailingSeparatorsIgnored)
{
  auto t = splitTopLevelArgs("  ,  1  ,  2  ,  ");
  ASSERT_EQ(t.size(), 2U);
  EXPECT_EQ(t[0], "1");
  EXPECT_EQ(t[1], "2");
}

}  // namespace
//--------------------------------------------------------------------------------------------------------------
// Codepoint and word motion
//--------------------------------------------------------------------------------------------------------------

TEST(ParseUtilsCodepoint, StepsOverMultiByteCharacters)
{
  // "é" is two bytes, "€" three, "𝄞" four.
  std::string_view text = "aé€𝄞b";
  EXPECT_EQ(prevCodepoint(text, text.size()), text.size() - 1U);  // before 'b'
  auto atEuroEnd = prevCodepoint(text, text.size() - 1U);         // start of the 4-byte glyph
  EXPECT_EQ(nextCodepoint(text, atEuroEnd), text.size() - 1U);
  EXPECT_EQ(prevCodepoint(text, 0U), 0U);
  EXPECT_EQ(nextCodepoint(text, text.size()), text.size());
  // Stepping left from the end, one codepoint at a time, must land exactly on 0 after 5 steps.
  std::size_t pos = text.size();
  for (int i = 0; i < 5; ++i)
  {
    pos = prevCodepoint(text, pos);
  }
  EXPECT_EQ(pos, 0U);
}

TEST(ParseUtilsCodepoint, ErasingOneCodepointLeavesValidText)
{
  std::string buf = "abé";
  auto from = prevCodepoint(buf, buf.size());
  buf.erase(from, buf.size() - from);
  EXPECT_EQ(buf, "ab");  // not "ab\xc3"
}

TEST(ParseUtilsWord, StopsAtEachDottedSegment)
{
  std::string_view text = "local.demo.showcase";
  auto p1 = prevWord(text, text.size());
  EXPECT_EQ(text.substr(p1), "showcase");
  auto p2 = prevWord(text, p1);
  EXPECT_EQ(text.substr(p2, 4), "demo");
  EXPECT_EQ(prevWord(text, 0U), 0U);
}

TEST(ParseUtilsWord, SkipsSeparatorsBeforeTheWord)
{
  std::string_view text = "listen obj   ";
  EXPECT_EQ(prevWord(text, text.size()), 7U);  // skips the trailing spaces, then "obj"
  EXPECT_EQ(nextWord(text, 0U), 6U);           // past "listen"
  EXPECT_EQ(nextWord(text, text.size()), text.size());
}

TEST(ParseUtilsWord, TreatsNonAsciiAsWordCharacters)
{
  std::string_view text = "café bar";
  EXPECT_EQ(prevWord(text, 5U), 0U);  // "café" is one word, accent included
}

TEST(SplitTopLevelArgs, AStrayCloserStaysInItsTokenAndKeepsTheSeparators)
{
  // An unguarded --depth went negative, which made every later separator invisible and collapsed the
  // rest of the line into one token, reported as a parse failure naming the whole line.
  auto t = splitTopLevelArgs("1} 2 3");
  ASSERT_EQ(t.size(), 3U);
  EXPECT_EQ(t[0], "1}");
  EXPECT_EQ(t[1], "2");
  EXPECT_EQ(t[2], "3");
}

TEST(SplitTopLevelArgs, ALeadingCloserIsNotDropped)
{
  auto t = splitTopLevelArgs("]abc def");
  ASSERT_EQ(t.size(), 2U);
  EXPECT_EQ(t[0], "]abc");
  EXPECT_EQ(t[1], "def");

  auto u = splitTopLevelArgs("} 1 2");
  ASSERT_EQ(u.size(), 3U);
  EXPECT_EQ(u[0], "}");
  EXPECT_EQ(u[1], "1");
  EXPECT_EQ(u[2], "2");
}

TEST(SplitTopLevelArgs, AnUnbalancedCloserDoesNotUnbalanceTheRest)
{
  auto t = splitTopLevelArgs("} {1 2} 3");
  ASSERT_EQ(t.size(), 3U);
  EXPECT_EQ(t[0], "}");
  EXPECT_EQ(t[1], "{1 2}");
  EXPECT_EQ(t[2], "3");
}

}  // namespace sen::components::term
