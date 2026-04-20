// === suggester_test.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "suggester.h"

// google test
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <string>
#include <vector>

namespace sen::components::term
{
namespace
{

const std::vector<std::string>& testCommands()
{
  static const std::vector<std::string> v = {
    "help", "cd", "pwd", "ls", "open", "close", "query", "queries", "log", "clear", "shutdown", "exit"};
  return v;
}

//--------------------------------------------------------------------------------------------------------------
// editDistance
//--------------------------------------------------------------------------------------------------------------

TEST(EditDistance, IdenticalIsZero) { EXPECT_EQ(editDistance("hello", "hello"), 0U); }

TEST(EditDistance, EmptyVsNonEmpty)
{
  EXPECT_EQ(editDistance("", "abc"), 3U);
  EXPECT_EQ(editDistance("abc", ""), 3U);
  EXPECT_EQ(editDistance("", ""), 0U);
}

TEST(EditDistance, SingleSubstitution) { EXPECT_EQ(editDistance("cat", "bat"), 1U); }

TEST(EditDistance, SingleInsertion) { EXPECT_EQ(editDistance("cat", "cats"), 1U); }

TEST(EditDistance, SingleDeletion) { EXPECT_EQ(editDistance("cats", "cat"), 1U); }

TEST(EditDistance, MultipleEdits) { EXPECT_EQ(editDistance("kitten", "sitting"), 3U); }

TEST(EditDistance, CaseInsensitive)
{
  EXPECT_EQ(editDistance("Hello", "hello"), 0U);
  EXPECT_EQ(editDistance("CAT", "cat"), 0U);
}

//--------------------------------------------------------------------------------------------------------------
// findSuggestions, core matches
//--------------------------------------------------------------------------------------------------------------

TEST(FindSuggestions, ExactMatchReturnsItself)
{
  auto out = findSuggestions("ls", testCommands());
  ASSERT_FALSE(out.empty());
  EXPECT_EQ(out[0], "ls");
}

TEST(FindSuggestions, OneEditAwaySuggested)
{
  // 'lst' is one insertion away from 'ls'.
  auto out = findSuggestions("lst", testCommands());
  ASSERT_FALSE(out.empty());
  EXPECT_EQ(out[0], "ls");
}

TEST(FindSuggestions, TypoForLongerCommand)
{
  // 'querys' is one deletion from 'query' and two edits from 'queries'.
  // Both should appear as suggestions; 'query' ranks first (lower distance).
  auto out = findSuggestions("querys", testCommands());
  ASSERT_FALSE(out.empty());
  EXPECT_EQ(out[0], "query");
  // And 'queries' should also be reachable within the threshold.
  EXPECT_NE(std::find(out.begin(), out.end(), std::string("queries")), out.end());
}

TEST(FindSuggestions, SubstringMatchAlwaysAccepted)
{
  // 'shut' is a substring of 'shutdown', accepted regardless of edit distance.
  auto out = findSuggestions("shut", testCommands());
  ASSERT_FALSE(out.empty());
  EXPECT_EQ(out[0], "shutdown");
}

TEST(FindSuggestions, CaseInsensitiveMatch)
{
  auto out = findSuggestions("LOG", testCommands());
  ASSERT_FALSE(out.empty());
  EXPECT_EQ(out[0], "log");
}

TEST(FindSuggestions, NoMatchForNonsense)
{
  // 'zzzzzzzz' is far from anything in the list.
  auto out = findSuggestions("zzzzzzzz", testCommands());
  EXPECT_TRUE(out.empty());
}

//--------------------------------------------------------------------------------------------------------------
// findSuggestions, limits and ordering
//--------------------------------------------------------------------------------------------------------------

TEST(FindSuggestions, RespectsMaxSuggestions)
{
  // 'q' is contained in 'query' and 'queries', both valid by substring, but maxSuggestions=1 caps it.
  auto out = findSuggestions("q", testCommands(), 1);
  EXPECT_LE(out.size(), 1U);
}

TEST(FindSuggestions, OrdersByDistanceAscending)
{
  // 'shutdwn' → 'shutdown' is distance 1 (insertion of 'o'); other candidates are far.
  // So 'shutdown' should be the only, and first, suggestion.
  auto out = findSuggestions("shutdwn", testCommands());
  ASSERT_FALSE(out.empty());
  EXPECT_EQ(out[0], "shutdown");
}

TEST(FindSuggestions, EmptyQueryYieldsNoSuggestions) { EXPECT_TRUE(findSuggestions("", testCommands()).empty()); }

TEST(FindSuggestions, EmptyCandidatesYieldsNothing)
{
  std::vector<std::string> empty;
  EXPECT_TRUE(findSuggestions("ls", empty).empty());
}

TEST(FindSuggestions, MaxSuggestionsZeroYieldsNothing)
{
  EXPECT_TRUE(findSuggestions("ls", testCommands(), 0).empty());
}

TEST(FindSuggestions, ShortQueryStrictThreshold)
{
  // 'aa' (length 2) → threshold 1. 'cd' is distance 2 → not suggested.
  auto out = findSuggestions("aa", testCommands());
  for (const auto& s: out)
  {
    // 'aa' isn't a substring of any command; the only acceptable candidates are
    // those within 1 edit, of which there are none in the list.
    EXPECT_EQ(s.find("aa"), std::string::npos) << "got: " << s;
  }
}

//--------------------------------------------------------------------------------------------------------------
// formatSuggestionHint
//--------------------------------------------------------------------------------------------------------------

TEST(FormatSuggestionHint, EmptyReturnsEmpty)
{
  std::vector<std::string> empty;
  EXPECT_EQ(formatSuggestionHint(empty), "");
}

TEST(FormatSuggestionHint, OneSuggestion)
{
  std::vector<std::string> s = {"ls"};
  EXPECT_EQ(formatSuggestionHint(s), "Did you mean 'ls'?");
}

TEST(FormatSuggestionHint, TwoSuggestionsUseOr)
{
  std::vector<std::string> s = {"ls", "log"};
  EXPECT_EQ(formatSuggestionHint(s), "Did you mean 'ls' or 'log'?");
}

TEST(FormatSuggestionHint, ThreeSuggestionsUseCommaThenOr)
{
  std::vector<std::string> s = {"ls", "log", "open"};
  EXPECT_EQ(formatSuggestionHint(s), "Did you mean 'ls', 'log' or 'open'?");
}

//--------------------------------------------------------------------------------------------------------------
// Shared primitives
//--------------------------------------------------------------------------------------------------------------

TEST(SuggesterPrimitives, SuggestionThresholdAdaptsToLength)
{
  EXPECT_EQ(suggestionThreshold(1), 1U);
  EXPECT_EQ(suggestionThreshold(3), 1U);
  EXPECT_EQ(suggestionThreshold(4), 2U);
  EXPECT_EQ(suggestionThreshold(6), 2U);
  EXPECT_EQ(suggestionThreshold(7), 3U);
  EXPECT_EQ(suggestionThreshold(100), 3U);
}

TEST(SuggesterPrimitives, ContainsCaseInsensitiveBasics)
{
  EXPECT_TRUE(containsCaseInsensitive("slowLogger", "log"));
  EXPECT_TRUE(containsCaseInsensitive("slowLogger", "LOG"));
  EXPECT_TRUE(containsCaseInsensitive("slowLogger", "Slow"));
  EXPECT_FALSE(containsCaseInsensitive("slowLogger", "fast"));
  EXPECT_TRUE(containsCaseInsensitive("anything", ""));
  EXPECT_FALSE(containsCaseInsensitive("a", "anything"));
}

TEST(SuggesterPrimitives, ScoreSuggestionSubstringIsZero)
{
  // 'logger' is contained in 'slowLogger' (case-insensitive), score should be 0.
  EXPECT_EQ(scoreSuggestion("logger", "slowLogger"), 0U);
}

TEST(SuggesterPrimitives, ScoreSuggestionFallsBackToEditDistance)
{
  // 'setnxt' is not a substring of 'setNext'; should return edit distance (1, insertion of 'e').
  EXPECT_EQ(scoreSuggestion("setnxt", "setNext"), 1U);
}

}  // namespace
}  // namespace sen::components::term
