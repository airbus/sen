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
}  // namespace sen::components::term
