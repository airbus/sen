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

/// @test
/// Two identical strings are zero edits apart.
TEST(EditDistance, IdenticalIsZero) { EXPECT_EQ(editDistance("hello", "hello"), 0U); }

/// @test
/// An empty string is as many edits from another as that string is long.
TEST(EditDistance, EmptyVsNonEmpty)
{
  EXPECT_EQ(editDistance("", "abc"), 3U);
  EXPECT_EQ(editDistance("abc", ""), 3U);
  EXPECT_EQ(editDistance("", ""), 0U);
}

/// @test
/// Changing one character is one edit.
TEST(EditDistance, SingleSubstitution) { EXPECT_EQ(editDistance("cat", "bat"), 1U); }

/// @test
/// Adding one character is one edit.
TEST(EditDistance, SingleInsertion) { EXPECT_EQ(editDistance("cat", "cats"), 1U); }

/// @test
/// Removing one character is one edit.
TEST(EditDistance, SingleDeletion) { EXPECT_EQ(editDistance("cats", "cat"), 1U); }

/// @test
/// A string needing three changes is three edits away.
TEST(EditDistance, MultipleEdits) { EXPECT_EQ(editDistance("kitten", "sitting"), 3U); }

/// @test
/// Case is ignored, so two strings differing only in case are zero edits apart.
TEST(EditDistance, CaseInsensitive)
{
  EXPECT_EQ(editDistance("Hello", "hello"), 0U);
  EXPECT_EQ(editDistance("CAT", "cat"), 0U);
}

//--------------------------------------------------------------------------------------------------------------
// findSuggestions, core matches
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A command typed exactly is suggested first.
TEST(FindSuggestions, ExactMatchReturnsItself)
{
  auto out = findSuggestions("ls", testCommands());
  ASSERT_FALSE(out.empty());
  EXPECT_EQ(out[0], "ls");
}

/// @test
/// A command one edit away is suggested.
TEST(FindSuggestions, OneEditAwaySuggested)
{
  // 'lst' is one insertion away from 'ls'.
  auto out = findSuggestions("lst", testCommands());
  ASSERT_FALSE(out.empty());
  EXPECT_EQ(out[0], "ls");
}

/// @test
/// Two candidates within the threshold are both suggested, the nearer one first.
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

/// @test
/// A candidate containing the query is accepted whatever its edit distance.
TEST(FindSuggestions, SubstringMatchAlwaysAccepted)
{
  // 'shut' is a substring of 'shutdown', accepted regardless of edit distance.
  auto out = findSuggestions("shut", testCommands());
  ASSERT_FALSE(out.empty());
  EXPECT_EQ(out[0], "shutdown");
}

/// @test
/// A query in a different case still matches.
TEST(FindSuggestions, CaseInsensitiveMatch)
{
  auto out = findSuggestions("LOG", testCommands());
  ASSERT_FALSE(out.empty());
  EXPECT_EQ(out[0], "log");
}

/// @test
/// A query far from every candidate suggests nothing.
TEST(FindSuggestions, NoMatchForNonsense)
{
  // 'zzzzzzzz' is far from anything in the list.
  auto out = findSuggestions("zzzzzzzz", testCommands());
  EXPECT_TRUE(out.empty());
}

//--------------------------------------------------------------------------------------------------------------
// findSuggestions, limits and ordering
//--------------------------------------------------------------------------------------------------------------

/// @test
/// No more than the requested number of suggestions is returned.
TEST(FindSuggestions, RespectsMaxSuggestions)
{
  // 'q' is contained in 'query' and 'queries', both valid by substring, but maxSuggestions=1 caps it.
  auto out = findSuggestions("q", testCommands(), 1);
  EXPECT_LE(out.size(), 1U);
}

/// @test
/// A query one edit from shutdown and far from every other command gets shutdown as the first
/// suggestion.
TEST(FindSuggestions, OrdersByDistanceAscending)
{
  // 'shutdwn' → 'shutdown' is distance 1 (insertion of 'o'); other candidates are far.
  // So 'shutdown' should be the only, and first, suggestion.
  auto out = findSuggestions("shutdwn", testCommands());
  ASSERT_FALSE(out.empty());
  EXPECT_EQ(out[0], "shutdown");
}

/// @test
/// An empty query suggests nothing.
TEST(FindSuggestions, EmptyQueryYieldsNoSuggestions) { EXPECT_TRUE(findSuggestions("", testCommands()).empty()); }

/// @test
/// An empty candidate list suggests nothing.
TEST(FindSuggestions, EmptyCandidatesYieldsNothing)
{
  std::vector<std::string> empty;
  EXPECT_TRUE(findSuggestions("ls", empty).empty());
}

/// @test
/// Asking for no suggestions returns none.
TEST(FindSuggestions, MaxSuggestionsZeroYieldsNothing)
{
  EXPECT_TRUE(findSuggestions("ls", testCommands(), 0).empty());
}

/// @test
/// The two-letter query is contained in no command, and every command is at least two edits
/// away. No returned suggestion contains the query.
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

/// @test
/// No suggestions produce no hint.
TEST(FormatSuggestionHint, EmptyReturnsEmpty)
{
  std::vector<std::string> empty;
  EXPECT_EQ(formatSuggestionHint(empty), "");
}

/// @test
/// The hint for a single suggestion reads "Did you mean 'ls'?".
TEST(FormatSuggestionHint, OneSuggestion)
{
  std::vector<std::string> s = {"ls"};
  EXPECT_EQ(formatSuggestionHint(s), "Did you mean 'ls'?");
}

/// @test
/// Two suggestions are joined with or.
TEST(FormatSuggestionHint, TwoSuggestionsUseOr)
{
  std::vector<std::string> s = {"ls", "log"};
  EXPECT_EQ(formatSuggestionHint(s), "Did you mean 'ls' or 'log'?");
}

/// @test
/// Three suggestions are joined with commas and a final or.
TEST(FormatSuggestionHint, ThreeSuggestionsUseCommaThenOr)
{
  std::vector<std::string> s = {"ls", "log", "open"};
  EXPECT_EQ(formatSuggestionHint(s), "Did you mean 'ls', 'log' or 'open'?");
}

//--------------------------------------------------------------------------------------------------------------
// Shared primitives
//--------------------------------------------------------------------------------------------------------------

/// @test
/// The threshold is one edit for queries up to three characters, two up to six, and three from
/// seven characters on.
TEST(SuggesterPrimitives, SuggestionThresholdAdaptsToLength)
{
  EXPECT_EQ(suggestionThreshold(1), 1U);
  EXPECT_EQ(suggestionThreshold(3), 1U);
  EXPECT_EQ(suggestionThreshold(4), 2U);
  EXPECT_EQ(suggestionThreshold(6), 2U);
  EXPECT_EQ(suggestionThreshold(7), 3U);
  EXPECT_EQ(suggestionThreshold(100), 3U);
}

/// @test
/// A substring is found whatever the case, an empty needle is always found, and a needle longer
/// than the haystack is not.
TEST(SuggesterPrimitives, ContainsCaseInsensitiveBasics)
{
  EXPECT_TRUE(containsCaseInsensitive("slowLogger", "log"));
  EXPECT_TRUE(containsCaseInsensitive("slowLogger", "LOG"));
  EXPECT_TRUE(containsCaseInsensitive("slowLogger", "Slow"));
  EXPECT_FALSE(containsCaseInsensitive("slowLogger", "fast"));
  EXPECT_TRUE(containsCaseInsensitive("anything", ""));
  EXPECT_FALSE(containsCaseInsensitive("a", "anything"));
}

/// @test
/// A candidate containing the query scores zero.
TEST(SuggesterPrimitives, ScoreSuggestionSubstringIsZero)
{
  // 'logger' is contained in 'slowLogger' (case-insensitive), score should be 0.
  EXPECT_EQ(scoreSuggestion("logger", "slowLogger"), 0U);
}

/// @test
/// A candidate that does not contain the query scores its edit distance.
TEST(SuggesterPrimitives, ScoreSuggestionFallsBackToEditDistance)
{
  // 'setnxt' is not a substring of 'setNext'; should return edit distance (1, insertion of 'e').
  EXPECT_EQ(scoreSuggestion("setnxt", "setNext"), 1U);
}

}  // namespace
}  // namespace sen::components::term
