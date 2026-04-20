// === completer_test.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "completer.h"
#include "test_completer_utils.h"

// google test
#include <gtest/gtest.h>

// std
#include <string>
#include <vector>

namespace sen::components::term
{

/// Test helper to access Completer internals.
class CompleterTestAccess
{
public:
  static void setChildren(Completer& c, std::vector<std::string> names) { c.childNames_ = std::move(names); }

  static void setOpenSources(Completer& c, std::vector<std::string> sources) { c.openSources_ = std::move(sources); }

  static void setAvailableSources(Completer& c, std::vector<std::string> sources)
  {
    c.availableSources_ = std::move(sources);
  }

  static void setQueryNames(Completer& c, std::vector<std::string> names) { c.queryNames_ = std::move(names); }

  static void setLoggerNames(Completer& c, std::vector<std::string> names) { c.loggerNames_ = std::move(names); }

  static void setObjects(Completer& c, std::unordered_map<std::string, std::shared_ptr<Object>> objects)
  {
    c.objectsByName_ = std::move(objects);
  }

  static auto splitObjectMethod(std::string_view token) { return Completer::splitObjectMethod(token); }
};

namespace
{

using test::displayOf;
using test::hasCandidate;
using test::texts;

//--------------------------------------------------------------------------------------------------------------
// commonPrefix tests
//--------------------------------------------------------------------------------------------------------------

TEST(CompleterCommonPrefix, EmptyCandidates)
{
  std::vector<Completion> empty;
  EXPECT_EQ(Completer::commonPrefix(empty), "");
}

TEST(CompleterCommonPrefix, SingleCandidate)
{
  std::vector<Completion> c = {{"hello", ""}};
  EXPECT_EQ(Completer::commonPrefix(c), "hello");
}

TEST(CompleterCommonPrefix, FullMatch)
{
  std::vector<Completion> c = {{"abc", ""}, {"abc", ""}};
  EXPECT_EQ(Completer::commonPrefix(c), "abc");
}

TEST(CompleterCommonPrefix, PartialMatch)
{
  std::vector<Completion> c = {{"abcdef", ""}, {"abcxyz", ""}};
  EXPECT_EQ(Completer::commonPrefix(c), "abc");
}

TEST(CompleterCommonPrefix, NoMatch)
{
  std::vector<Completion> c = {{"abc", ""}, {"xyz", ""}};
  EXPECT_EQ(Completer::commonPrefix(c), "");
}

TEST(CompleterCommonPrefix, MultipleWithCommonPrefix)
{
  std::vector<Completion> c = {{"getConfig", ""}, {"getCounter", ""}, {"getLastMessage", ""}};
  EXPECT_EQ(Completer::commonPrefix(c), "get");
}

TEST(CompleterCommonPrefix, SingleCharPrefix)
{
  std::vector<Completion> c = {{"abc", ""}, {"axyz", ""}};
  EXPECT_EQ(Completer::commonPrefix(c), "a");
}

//--------------------------------------------------------------------------------------------------------------
// splitObjectMethod tests
//--------------------------------------------------------------------------------------------------------------

TEST(SplitObjectMethod, NoDot)
{
  auto result = CompleterTestAccess::splitObjectMethod("slowLogger");
  EXPECT_FALSE(result.has_value());
}

TEST(SplitObjectMethod, DotAtStart)
{
  auto result = CompleterTestAccess::splitObjectMethod(".getRate");
  EXPECT_FALSE(result.has_value());
}

TEST(SplitObjectMethod, EmptyInput)
{
  auto result = CompleterTestAccess::splitObjectMethod("");
  EXPECT_FALSE(result.has_value());
}

TEST(SplitObjectMethod, SimpleObjectMethod)
{
  auto result = CompleterTestAccess::splitObjectMethod("slowLogger.getRate");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->first, "slowLogger");
  EXPECT_EQ(result->second, "getRate");
}

TEST(SplitObjectMethod, DottedObjectPath)
{
  // Splits on LAST dot, method names never contain dots
  auto result = CompleterTestAccess::splitObjectMethod("local.demo.slowLogger.getRate");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->first, "local.demo.slowLogger");
  EXPECT_EQ(result->second, "getRate");
}

TEST(SplitObjectMethod, TrailingDot)
{
  auto result = CompleterTestAccess::splitObjectMethod("slowLogger.");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->first, "slowLogger");
  EXPECT_EQ(result->second, "");
}

TEST(SplitObjectMethod, DottedPathTrailingDot)
{
  auto result = CompleterTestAccess::splitObjectMethod("local.demo.slowLogger.");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->first, "local.demo.slowLogger");
  EXPECT_EQ(result->second, "");
}

//--------------------------------------------------------------------------------------------------------------
// Command completion tests
//--------------------------------------------------------------------------------------------------------------

TEST(CompleterCommand, EmptyPrefix)
{
  Completer c;
  auto result = c.complete("", 0);
  EXPECT_GT(result.candidates.size(), 0U);
  EXPECT_TRUE(hasCandidate(result, "cd"));
  EXPECT_TRUE(hasCandidate(result, "ls"));
  EXPECT_TRUE(hasCandidate(result, "help"));
  EXPECT_TRUE(hasCandidate(result, "exit"));
  EXPECT_TRUE(hasCandidate(result, "shutdown"));
}

TEST(CompleterCommand, PartialPrefix)
{
  Completer c;
  auto result = c.complete("cl", 2);
  auto t = texts(result);
  EXPECT_EQ(t.size(), 2U);
  EXPECT_TRUE(hasCandidate(result, "clear"));
  EXPECT_TRUE(hasCandidate(result, "close"));
}

TEST(CompleterCommand, UniquePrefix)
{
  Completer c;
  auto result = c.complete("he", 2);
  EXPECT_EQ(result.candidates.size(), 1U);
  EXPECT_EQ(result.candidates[0].text, "help");
}

TEST(CompleterCommand, ExactMatch)
{
  Completer c;
  auto result = c.complete("cd", 2);
  EXPECT_TRUE(hasCandidate(result, "cd"));
}

TEST(CompleterCommand, NoMatch)
{
  Completer c;
  auto result = c.complete("zzz", 3);
  EXPECT_EQ(result.candidates.size(), 0U);
}

TEST(CompleterCommand, ReplaceRange)
{
  Completer c;
  auto result = c.complete("he", 2);
  EXPECT_EQ(result.replaceFrom, 0);
  EXPECT_EQ(result.replaceTo, 2);
}

TEST(CompleterCommand, CommandsHaveDescriptions)
{
  Completer c;
  auto result = c.complete("he", 2);
  ASSERT_EQ(result.candidates.size(), 1U);
  EXPECT_FALSE(result.candidates[0].display.empty());
}

TEST(CompleterCommand, CommandsNotShownForDottedPrefix)
{
  Completer c;
  auto result = c.complete("cd.", 3);
  EXPECT_FALSE(hasCandidate(result, "cd"));
}

//--------------------------------------------------------------------------------------------------------------
// cd argument completion tests
//--------------------------------------------------------------------------------------------------------------

TEST(CompleterCd, SpecialTokens)
{
  Completer c;
  auto result = c.complete("cd ", 3);
  EXPECT_TRUE(hasCandidate(result, ".."));
  EXPECT_TRUE(hasCandidate(result, "/"));
  EXPECT_TRUE(hasCandidate(result, "-"));
}

TEST(CompleterCd, ChildNames)
{
  Completer c;
  CompleterTestAccess::setChildren(c, {"local", "remote"});
  auto result = c.complete("cd l", 4);
  EXPECT_EQ(result.candidates.size(), 1U);
  EXPECT_EQ(result.candidates[0].text, "local");
}

TEST(CompleterCd, MultipleChildren)
{
  Completer c;
  CompleterTestAccess::setChildren(c, {"demo", "data", "log"});
  auto result = c.complete("cd d", 4);
  EXPECT_EQ(result.candidates.size(), 2U);
  EXPECT_TRUE(hasCandidate(result, "demo"));
  EXPECT_TRUE(hasCandidate(result, "data"));
}

TEST(CompleterCd, QueryScopes)
{
  Completer c;
  CompleterTestAccess::setQueryNames(c, {"sensors", "actuators"});
  auto result = c.complete("cd @", 4);
  EXPECT_TRUE(hasCandidate(result, "@sensors"));
  EXPECT_TRUE(hasCandidate(result, "@actuators"));
}

TEST(CompleterCd, AvailableSessionSources)
{
  // cd shows undotted sources (sessions) only; dotted bus addresses are excluded.
  Completer c;
  CompleterTestAccess::setAvailableSources(c, {"local", "remote"});
  auto result = c.complete("cd ", 3);
  EXPECT_TRUE(hasCandidate(result, "local"));
  EXPECT_TRUE(hasCandidate(result, "remote"));
}

TEST(CompleterCd, DottedSourcesOfferedWhenPrefixHasDot)
{
  // Typing "cd local." should suggest buses like "local.main", "local.log".
  Completer c;
  CompleterTestAccess::setAvailableSources(c, {"local.main", "local.log"});
  auto result = c.complete("cd local.", 9);
  EXPECT_TRUE(hasCandidate(result, "local.main"));
  EXPECT_TRUE(hasCandidate(result, "local.log"));
}

TEST(CompleterCd, DottedSourcesExcludedWhenPrefixUndotted)
{
  // Typing "cd l" should NOT suggest "local.main" (only undotted session names).
  Completer c;
  CompleterTestAccess::setAvailableSources(c, {"local.main", "local.log"});
  auto result = c.complete("cd l", 4);
  EXPECT_FALSE(hasCandidate(result, "local.main"));
  EXPECT_FALSE(hasCandidate(result, "local.log"));
}

TEST(CompleterCd, ReplaceRange)
{
  Completer c;
  CompleterTestAccess::setChildren(c, {"demo"});
  auto result = c.complete("cd de", 5);
  EXPECT_EQ(result.replaceFrom, 3);
  EXPECT_EQ(result.replaceTo, 5);
}

TEST(CompleterCd, EmptyArgShowsAll)
{
  Completer c;
  CompleterTestAccess::setChildren(c, {"local"});
  CompleterTestAccess::setQueryNames(c, {"q1"});
  auto result = c.complete("cd ", 3);
  EXPECT_TRUE(hasCandidate(result, ".."));
  EXPECT_TRUE(hasCandidate(result, "local"));
  EXPECT_TRUE(hasCandidate(result, "@q1"));
}

TEST(CompleterCd, MixedResults)
{
  Completer c;
  CompleterTestAccess::setChildren(c, {"local"});
  CompleterTestAccess::setAvailableSources(c, {"local.main"});
  auto result = c.complete("cd local", 8);
  EXPECT_TRUE(hasCandidate(result, "local"));
  // Dotted sources are excluded from cd completions
  EXPECT_FALSE(hasCandidate(result, "local.main"));
}

//--------------------------------------------------------------------------------------------------------------
// open/close argument completion tests
//--------------------------------------------------------------------------------------------------------------

TEST(CompleterOpen, FiltersByPrefix)
{
  Completer c;
  CompleterTestAccess::setAvailableSources(c, {"local.main", "local.log", "remote.data"});
  auto result = c.complete("open local.", 11);
  EXPECT_EQ(result.candidates.size(), 2U);
  EXPECT_TRUE(hasCandidate(result, "local.main"));
  EXPECT_TRUE(hasCandidate(result, "local.log"));
}

TEST(CompleterOpen, EmptyPrefix)
{
  Completer c;
  CompleterTestAccess::setAvailableSources(c, {"local.main", "remote.data"});
  auto result = c.complete("open ", 5);
  EXPECT_EQ(result.candidates.size(), 2U);
}

TEST(CompleterOpen, NoSources)
{
  Completer c;
  auto result = c.complete("open ", 5);
  EXPECT_EQ(result.candidates.size(), 0U);
}

TEST(CompleterOpen, ReplaceRange)
{
  Completer c;
  CompleterTestAccess::setAvailableSources(c, {"local.main"});
  auto result = c.complete("open lo", 7);
  EXPECT_EQ(result.replaceFrom, 5);
  EXPECT_EQ(result.replaceTo, 7);
}

TEST(CompleterClose, ClosesOpenSources)
{
  Completer c;
  CompleterTestAccess::setOpenSources(c, {"local.main", "local.log"});
  auto result = c.complete("close ", 6);
  EXPECT_EQ(result.candidates.size(), 2U);
  EXPECT_TRUE(hasCandidate(result, "local.main"));
  EXPECT_TRUE(hasCandidate(result, "local.log"));
}

TEST(CompleterClose, FiltersByPrefix)
{
  Completer c;
  CompleterTestAccess::setOpenSources(c, {"local.main", "local.log", "remote.data"});
  auto result = c.complete("close r", 7);
  EXPECT_EQ(result.candidates.size(), 1U);
  EXPECT_TRUE(hasCandidate(result, "remote.data"));
}

//--------------------------------------------------------------------------------------------------------------
// log argument completion tests
//--------------------------------------------------------------------------------------------------------------

TEST(CompleterLog, SubcommandLevel)
{
  Completer c;
  auto result = c.complete("log ", 4);
  EXPECT_EQ(result.candidates.size(), 1U);
  EXPECT_EQ(result.candidates[0].text, "level");
}

TEST(CompleterLog, SubcommandPartialPrefix)
{
  Completer c;
  auto result = c.complete("log l", 5);
  EXPECT_EQ(result.candidates.size(), 1U);
  EXPECT_EQ(result.candidates[0].text, "level");
}

TEST(CompleterLog, SubcommandNoMatch)
{
  Completer c;
  auto result = c.complete("log x", 5);
  EXPECT_EQ(result.candidates.size(), 0U);
}

TEST(CompleterLog, LevelNames)
{
  Completer c;
  auto result = c.complete("log level ", 10);
  EXPECT_TRUE(hasCandidate(result, "trace"));
  EXPECT_TRUE(hasCandidate(result, "debug"));
  EXPECT_TRUE(hasCandidate(result, "info"));
  EXPECT_TRUE(hasCandidate(result, "warn"));
  EXPECT_TRUE(hasCandidate(result, "error"));
  EXPECT_TRUE(hasCandidate(result, "critical"));
  EXPECT_TRUE(hasCandidate(result, "off"));
}

TEST(CompleterLog, LevelPrefixFilter)
{
  Completer c;
  auto result = c.complete("log level tr", 12);
  EXPECT_EQ(result.candidates.size(), 1U);
  EXPECT_EQ(result.candidates[0].text, "trace");
}

TEST(CompleterLog, LevelAndLoggerNames)
{
  Completer c;
  CompleterTestAccess::setLoggerNames(c, {"kernel", "transport"});
  auto result = c.complete("log level ", 10);
  EXPECT_TRUE(hasCandidate(result, "info"));
  EXPECT_TRUE(hasCandidate(result, "kernel"));
  EXPECT_TRUE(hasCandidate(result, "transport"));
}

TEST(CompleterLog, PerLoggerLevel)
{
  Completer c;
  CompleterTestAccess::setLoggerNames(c, {"kernel"});
  auto result = c.complete("log level kernel ", 17);
  EXPECT_TRUE(hasCandidate(result, "trace"));
  EXPECT_TRUE(hasCandidate(result, "warn"));
  EXPECT_FALSE(hasCandidate(result, "kernel"));
}

TEST(CompleterLog, ReplaceRange)
{
  Completer c;
  auto result = c.complete("log level wa", 12);
  EXPECT_EQ(result.replaceFrom, 10);
  EXPECT_EQ(result.replaceTo, 12);
}

//--------------------------------------------------------------------------------------------------------------
// query rm completion tests
//--------------------------------------------------------------------------------------------------------------

TEST(CompleterQueryRm, QueryNames)
{
  Completer c;
  CompleterTestAccess::setQueryNames(c, {"sensors", "actuators"});
  auto result = c.complete("query rm ", 9);
  EXPECT_TRUE(hasCandidate(result, "sensors"));
  EXPECT_TRUE(hasCandidate(result, "actuators"));
}

TEST(CompleterQueryRm, FiltersByPrefix)
{
  Completer c;
  CompleterTestAccess::setQueryNames(c, {"sensors", "actuators"});
  auto result = c.complete("query rm s", 10);
  EXPECT_EQ(result.candidates.size(), 1U);
  EXPECT_EQ(result.candidates[0].text, "sensors");
}

TEST(CompleterQueryRm, NoQueries)
{
  Completer c;
  auto result = c.complete("query rm ", 9);
  EXPECT_EQ(result.candidates.size(), 0U);
}

TEST(CompleterQueryRm, ReplaceRange)
{
  Completer c;
  CompleterTestAccess::setQueryNames(c, {"sensors"});
  auto result = c.complete("query rm se", 11);
  EXPECT_EQ(result.replaceFrom, 9);
  EXPECT_EQ(result.replaceTo, 11);
}

//--------------------------------------------------------------------------------------------------------------
// Object path navigation tests
//--------------------------------------------------------------------------------------------------------------

TEST(CompleterPath, EmptyPrefixShowsFirstSegments)
{
  Completer c;
  CompleterTestAccess::setObjects(c,
                                  {
                                    {"local.demo.slowLogger", nullptr},
                                    {"local.demo.consoleWriter", nullptr},
                                    {"local.kernel.KernelApi", nullptr},
                                  });
  auto result = c.complete("", 0);
  EXPECT_TRUE(hasCandidate(result, "local"));
  EXPECT_FALSE(hasCandidate(result, "local.demo.slowLogger"));
  EXPECT_FALSE(hasCandidate(result, "local.kernel.KernelApi"));
  EXPECT_TRUE(hasCandidate(result, "cd"));
}

TEST(CompleterPath, DottedPrefixShowsNextSegments)
{
  Completer c;
  CompleterTestAccess::setObjects(c,
                                  {
                                    {"local.demo.slowLogger", nullptr},
                                    {"local.demo.consoleWriter", nullptr},
                                    {"local.kernel.KernelApi", nullptr},
                                  });
  auto result = c.complete("local.", 6);
  EXPECT_TRUE(hasCandidate(result, "local.demo"));
  EXPECT_TRUE(hasCandidate(result, "local.kernel"));
  EXPECT_FALSE(hasCandidate(result, "local.demo.slowLogger"));
}

TEST(CompleterPath, DeepDottedPrefixShowsLeafObjects)
{
  Completer c;
  CompleterTestAccess::setObjects(c,
                                  {
                                    {"local.demo.slowLogger", nullptr},
                                    {"local.demo.consoleWriter", nullptr},
                                    {"local.kernel.KernelApi", nullptr},
                                  });
  auto result = c.complete("local.demo.", 11);
  EXPECT_TRUE(hasCandidate(result, "local.demo.slowLogger"));
  EXPECT_TRUE(hasCandidate(result, "local.demo.consoleWriter"));
  EXPECT_FALSE(hasCandidate(result, "local.kernel.KernelApi"));
}

TEST(CompleterPath, LeafObjectsShownForSimplePrefix)
{
  Completer c;
  CompleterTestAccess::setObjects(c,
                                  {
                                    {"slowLogger", nullptr},
                                    {"fastMixed", nullptr},
                                  });
  auto result = c.complete("sl", 2);
  EXPECT_TRUE(hasCandidate(result, "slowLogger"));
  EXPECT_FALSE(hasCandidate(result, "fastMixed"));
}

TEST(CompleterPath, SegmentsDeduplicatedAcrossObjects)
{
  Completer c;
  CompleterTestAccess::setObjects(c,
                                  {
                                    {"local.demo.obj1", nullptr},
                                    {"local.demo.obj2", nullptr},
                                    {"local.demo.obj3", nullptr},
                                  });
  auto result = c.complete("local.", 6);
  auto count = std::count_if(
    result.candidates.begin(), result.candidates.end(), [](const auto& c) { return c.text == "local.demo"; });
  EXPECT_EQ(count, 1);
}

TEST(CompleterPath, PathCandidatesHaveDescriptiveDisplay)
{
  Completer c;
  CompleterTestAccess::setObjects(c, {{"local.demo.slowLogger", nullptr}});
  auto result = c.complete("", 0);
  // At root scope (default), first segment is a session
  EXPECT_EQ(displayOf(result, "local"), "session");
  for (const auto& candidate: result.candidates)
  {
    if (candidate.text == "local")
    {
      EXPECT_EQ(candidate.kind, CompletionKind::path);
    }
  }
}

TEST(CompleterPath, IntermediateSegmentLabelledAsBus)
{
  Completer c;
  CompleterTestAccess::setObjects(c, {{"local.demo.slowLogger", nullptr}});
  auto result = c.complete("local.", 6);
  // Second segment is a bus
  EXPECT_EQ(displayOf(result, "local.demo"), "bus");
}

TEST(CompleterPath, LeafCandidatesHaveClassDisplay)
{
  // With nullptr objects, class name shows as "?"
  Completer c;
  CompleterTestAccess::setObjects(c, {{"slowLogger", nullptr}});
  auto result = c.complete("", 0);
  EXPECT_EQ(displayOf(result, "slowLogger"), "?");
}

TEST(CompleterPath, IntermediateSegmentsArePathKind)
{
  Completer c;
  CompleterTestAccess::setObjects(c, {{"local.demo.slowLogger", nullptr}});
  auto result = c.complete("local.", 6);
  for (const auto& candidate: result.candidates)
  {
    if (candidate.text == "local.demo")
    {
      EXPECT_EQ(candidate.kind, CompletionKind::path);
    }
  }
}

TEST(CompleterPath, PartialSegmentFiltering)
{
  Completer c;
  CompleterTestAccess::setObjects(c,
                                  {
                                    {"local.demo.obj1", nullptr},
                                    {"local.data.obj2", nullptr},
                                    {"remote.bus.obj3", nullptr},
                                  });
  auto result = c.complete("local.d", 7);
  EXPECT_TRUE(hasCandidate(result, "local.demo"));
  EXPECT_TRUE(hasCandidate(result, "local.data"));
  EXPECT_FALSE(hasCandidate(result, "remote.bus"));
}

TEST(CompleterPath, NoObjectsNoPathCandidates)
{
  Completer c;
  auto result = c.complete("", 0);
  // Only commands, no path candidates
  for (const auto& candidate: result.candidates)
  {
    EXPECT_NE(candidate.kind, CompletionKind::path) << "Unexpected path candidate: " << candidate.text;
  }
}

TEST(CompleterPath, ContinuationFlow)
{
  // Simulate the full path navigation: "" -> "local" -> "local.demo" -> "local.demo.slowLogger"
  Completer c;
  CompleterTestAccess::setObjects(c, {{"local.demo.slowLogger", nullptr}});

  auto r1 = c.complete("", 0);
  EXPECT_TRUE(hasCandidate(r1, "local"));

  auto r2 = c.complete("local.", 6);
  EXPECT_TRUE(hasCandidate(r2, "local.demo"));

  auto r3 = c.complete("local.demo.", 11);
  EXPECT_TRUE(hasCandidate(r3, "local.demo.slowLogger"));
}

TEST(CompleterPath, DottedObjectsHiddenForSimplePrefix)
{
  Completer c;
  CompleterTestAccess::setObjects(c,
                                  {
                                    {"local.demo.slowLogger", nullptr},
                                    {"local.demo.consoleWriter", nullptr},
                                  });
  auto result = c.complete("local", 5);
  EXPECT_FALSE(hasCandidate(result, "local.demo.slowLogger"));
  EXPECT_FALSE(hasCandidate(result, "local.demo.consoleWriter"));
}

TEST(CompleterPath, MultipleSessionsAtRoot)
{
  Completer c;
  CompleterTestAccess::setObjects(c,
                                  {
                                    {"local.demo.obj1", nullptr},
                                    {"remote.bus.obj2", nullptr},
                                  });
  auto result = c.complete("", 0);
  EXPECT_TRUE(hasCandidate(result, "local"));
  EXPECT_TRUE(hasCandidate(result, "remote"));
}

TEST(CompleterPath, ReplaceRangeForPath)
{
  Completer c;
  CompleterTestAccess::setObjects(c, {{"local.demo.slowLogger", nullptr}});
  auto result = c.complete("local.de", 8);
  EXPECT_EQ(result.replaceFrom, 0);
  EXPECT_EQ(result.replaceTo, 8);
}

//--------------------------------------------------------------------------------------------------------------
// Object method completion tests (exact match triggers method listing)
//--------------------------------------------------------------------------------------------------------------

TEST(CompleterMethod, ExactObjectMatchShowsMethodsNotObject)
{
  // Exact object match switches to method completion, not object-name completion.
  Completer c;
  CompleterTestAccess::setObjects(c, {{"slowLogger", nullptr}});
  auto result = c.complete("slowLogger", 10);
  EXPECT_FALSE(hasCandidate(result, "slowLogger"));
}

TEST(CompleterMethod, DottedObjectPrefixGoesToMethods)
{
  // Trailing dot on a known object enters method completion, not path/command.
  Completer c;
  CompleterTestAccess::setObjects(c, {{"slowLogger", nullptr}});
  auto result = c.complete("slowLogger.", 11);
  EXPECT_FALSE(hasCandidate(result, "cd"));
  EXPECT_FALSE(hasCandidate(result, "slowLogger"));
}

TEST(CompleterMethod, DottedPathObjectPrefixGoesToMethods)
{
  // Dotted path to a known object enters method completion.
  Completer c;
  CompleterTestAccess::setObjects(c, {{"local.demo.slowLogger", nullptr}});
  auto result = c.complete("local.demo.slowLogger.", 22);
  EXPECT_FALSE(hasCandidate(result, "cd"));
}

TEST(CompleterMethod, UnknownObjectFallsToPathCompletion)
{
  // Unknown object with trailing dot yields no completions.
  Completer c;
  CompleterTestAccess::setObjects(c, {{"slowLogger", nullptr}});
  auto result = c.complete("nonexistent.", 12);
  EXPECT_EQ(result.candidates.size(), 0U);
}

//--------------------------------------------------------------------------------------------------------------
// Replace range tests for various contexts
//--------------------------------------------------------------------------------------------------------------

TEST(CompleterRange, EmptyInput)
{
  Completer c;
  auto result = c.complete("", 0);
  EXPECT_EQ(result.replaceFrom, 0);
  EXPECT_EQ(result.replaceTo, 0);
}

TEST(CompleterRange, FirstTokenPartial)
{
  Completer c;
  auto result = c.complete("hel", 3);
  EXPECT_EQ(result.replaceFrom, 0);
  EXPECT_EQ(result.replaceTo, 3);
}

TEST(CompleterRange, SecondTokenAfterSpace)
{
  Completer c;
  CompleterTestAccess::setAvailableSources(c, {"local.main"});
  auto result = c.complete("open ", 5);
  EXPECT_EQ(result.replaceFrom, 5);
  EXPECT_EQ(result.replaceTo, 5);
}

TEST(CompleterRange, SecondTokenPartial)
{
  Completer c;
  CompleterTestAccess::setAvailableSources(c, {"local.main"});
  auto result = c.complete("open lo", 7);
  EXPECT_EQ(result.replaceFrom, 5);
  EXPECT_EQ(result.replaceTo, 7);
}

TEST(CompleterRange, ThirdToken)
{
  Completer c;
  auto result = c.complete("log level wa", 12);
  EXPECT_EQ(result.replaceFrom, 10);
  EXPECT_EQ(result.replaceTo, 12);
}

TEST(CompleterRange, DottedFirstToken)
{
  Completer c;
  CompleterTestAccess::setObjects(c, {{"local.demo.obj", nullptr}});
  auto result = c.complete("local.demo.", 11);
  EXPECT_EQ(result.replaceFrom, 0);
  EXPECT_EQ(result.replaceTo, 11);
}

//--------------------------------------------------------------------------------------------------------------
// Context switching tests
//--------------------------------------------------------------------------------------------------------------

TEST(CompleterDedup, NoDuplicatesInResults)
{
  // Even if the same text could appear from different sources, results should be deduplicated
  Completer c;
  CompleterTestAccess::setObjects(c, {{"local.demo.obj1", nullptr}});
  CompleterTestAccess::setChildren(c, {"local"});
  auto result = c.complete("", 0);
  auto count =
    std::count_if(result.candidates.begin(), result.candidates.end(), [](const auto& c) { return c.text == "local"; });
  EXPECT_LE(count, 1) << "Duplicate 'local' candidates found";
}

TEST(CompleterKind, CommandsHaveCommandKind)
{
  Completer c;
  auto result = c.complete("cd", 2);
  ASSERT_EQ(result.candidates.size(), 1U);
  EXPECT_EQ(result.candidates[0].kind, CompletionKind::command);
}

TEST(CompleterKind, PathsHavePathKind)
{
  Completer c;
  CompleterTestAccess::setObjects(c, {{"local.demo.obj", nullptr}});
  auto result = c.complete("", 0);
  for (const auto& candidate: result.candidates)
  {
    if (candidate.text == "local")
    {
      EXPECT_EQ(candidate.kind, CompletionKind::path);
    }
  }
}

TEST(CompleterKind, LeafObjectsHaveObjectKind)
{
  Completer c;
  CompleterTestAccess::setObjects(c, {{"slowLogger", nullptr}});
  auto result = c.complete("", 0);
  for (const auto& candidate: result.candidates)
  {
    if (candidate.text == "slowLogger")
    {
      EXPECT_EQ(candidate.kind, CompletionKind::object);
    }
  }
}

TEST(CompleterKind, ValuesHaveValueKind)
{
  Completer c;
  auto result = c.complete("log level ", 10);
  for (const auto& candidate: result.candidates)
  {
    EXPECT_EQ(candidate.kind, CompletionKind::value);
  }
}

TEST(CompleterContext, UnknownCommandNoCompletions)
{
  Completer c;
  auto result = c.complete("foobar ", 7);
  EXPECT_EQ(result.candidates.size(), 0U);
}

TEST(CompleterContext, QueryAfterNameSuggestsSelect)
{
  Completer c;
  CompleterTestAccess::setQueryNames(c, {"sensors"});
  auto result = c.complete("query xyz ", 10);
  // After "query <name>", suggest SELECT keyword.
  EXPECT_TRUE(hasCandidate(result, "SELECT"));
}

TEST(CompleterContext, MultipleSpacesBetweenTokens)
{
  Completer c;
  CompleterTestAccess::setChildren(c, {"local"});
  auto result = c.complete("cd   lo", 7);
  EXPECT_TRUE(hasCandidate(result, "local"));
}

}  // namespace
}  // namespace sen::components::term
