// === completer_test.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "completer.h"
#include "test_completer_utils.h"
#include "test_object_impl.h"
#include "util.h"

// sen
#include "sen/core/obj/object.h"

// google test
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sen::components::term
{

/// Test helper to access Completer internals.
class CompleterTestAccess
{
public:
  static void setChildren(Completer& c, std::vector<std::string> names)
  {
    c.childNames_ = std::set<std::string>(std::make_move_iterator(names.begin()), std::make_move_iterator(names.end()));
  }

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

  /// The scope depth the label arithmetic starts from. Only update() assigns it, and no test calls
  /// update(), so without this door every case runs at 0 and deleting `scopeDepth_ +` from the label
  /// arithmetic reddens nothing.
  static void setScopeDepth(Completer& c, int depth) { c.scopeDepth_ = depth; }

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

/// @test
/// An empty candidate list has an empty common prefix.
TEST(CompleterCommonPrefix, EmptyCandidates)
{
  std::vector<Completion> empty;
  EXPECT_EQ(Completer::commonPrefix(empty), "");
}

/// @test
/// One candidate is its own common prefix.
TEST(CompleterCommonPrefix, SingleCandidate)
{
  std::vector<Completion> c = {{"hello", ""}};
  EXPECT_EQ(Completer::commonPrefix(c), "hello");
}

/// @test
/// Identical candidates share the whole string.
TEST(CompleterCommonPrefix, FullMatch)
{
  std::vector<Completion> c = {{"abc", ""}, {"abc", ""}};
  EXPECT_EQ(Completer::commonPrefix(c), "abc");
}

/// @test
/// Candidates share the part they agree on.
TEST(CompleterCommonPrefix, PartialMatch)
{
  std::vector<Completion> c = {{"abcdef", ""}, {"abcxyz", ""}};
  EXPECT_EQ(Completer::commonPrefix(c), "abc");
}

/// @test
/// Candidates differing from the first character share nothing.
TEST(CompleterCommonPrefix, NoMatch)
{
  std::vector<Completion> c = {{"abc", ""}, {"xyz", ""}};
  EXPECT_EQ(Completer::commonPrefix(c), "");
}

/// @test
/// Several candidates share the longest prefix common to all of them.
TEST(CompleterCommonPrefix, MultipleWithCommonPrefix)
{
  std::vector<Completion> c = {{"getConfig", ""}, {"getCounter", ""}, {"getLastMessage", ""}};
  EXPECT_EQ(Completer::commonPrefix(c), "get");
}

/// @test
/// A single shared character is the common prefix.
TEST(CompleterCommonPrefix, SingleCharPrefix)
{
  std::vector<Completion> c = {{"abc", ""}, {"axyz", ""}};
  EXPECT_EQ(Completer::commonPrefix(c), "a");
}

//--------------------------------------------------------------------------------------------------------------
// splitObjectMethod tests
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A name with no dot is not an object and method pair.
TEST(SplitObjectMethod, NoDot)
{
  auto result = CompleterTestAccess::splitObjectMethod("slowLogger");
  EXPECT_FALSE(result.has_value());
}

/// @test
/// A name starting with a dot is not an object and method pair.
TEST(SplitObjectMethod, DotAtStart)
{
  auto result = CompleterTestAccess::splitObjectMethod(".getRate");
  EXPECT_FALSE(result.has_value());
}

/// @test
/// Empty input is not an object and method pair.
TEST(SplitObjectMethod, EmptyInput)
{
  auto result = CompleterTestAccess::splitObjectMethod("");
  EXPECT_FALSE(result.has_value());
}

/// @test
/// A name with one dot splits into the object and the method.
TEST(SplitObjectMethod, SimpleObjectMethod)
{
  auto result = CompleterTestAccess::splitObjectMethod("slowLogger.getRate");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->first, "slowLogger");
  EXPECT_EQ(result->second, "getRate");
}

/// @test
/// A dotted path splits at the last dot, since a method name holds none.
TEST(SplitObjectMethod, DottedObjectPath)
{
  // Splits on LAST dot, method names never contain dots
  auto result = CompleterTestAccess::splitObjectMethod("local.demo.slowLogger.getRate");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->first, "local.demo.slowLogger");
  EXPECT_EQ(result->second, "getRate");
}

/// @test
/// A name ending in a dot gives the object and an empty method.
TEST(SplitObjectMethod, TrailingDot)
{
  auto result = CompleterTestAccess::splitObjectMethod("slowLogger.");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->first, "slowLogger");
  EXPECT_EQ(result->second, "");
}

/// @test
/// A dotted path ending in a dot gives the whole path and an empty method.
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

/// @test
/// With nothing typed, commands are offered, including cd, ls, help, exit and shutdown.
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

/// @test
/// A prefix offers the commands starting with it.
TEST(CompleterCommand, PartialPrefix)
{
  Completer c;
  auto result = c.complete("cl", 2);
  auto t = texts(result);
  EXPECT_EQ(t.size(), 2U);
  EXPECT_TRUE(hasCandidate(result, "clear"));
  EXPECT_TRUE(hasCandidate(result, "close"));
}

/// @test
/// A prefix matching one command offers that command alone.
TEST(CompleterCommand, UniquePrefix)
{
  Completer c;
  auto result = c.complete("he", 2);
  EXPECT_EQ(result.candidates.size(), 1U);
  EXPECT_EQ(result.candidates[0].text, "help");
}

/// @test
/// A fully typed command is still offered.
TEST(CompleterCommand, ExactMatch)
{
  Completer c;
  auto result = c.complete("cd", 2);
  EXPECT_TRUE(hasCandidate(result, "cd"));
}

/// @test
/// A prefix matching no command offers nothing.
TEST(CompleterCommand, NoMatch)
{
  Completer c;
  auto result = c.complete("zzz", 3);
  EXPECT_EQ(result.candidates.size(), 0U);
}

/// @test
/// A command completion replaces the typed prefix and nothing else.
TEST(CompleterCommand, ReplaceRange)
{
  Completer c;
  auto result = c.complete("he", 2);
  EXPECT_EQ(result.replaceFrom, 0);
  EXPECT_EQ(result.replaceTo, 2);
}

/// @test
/// A command candidate carries a description to display.
TEST(CompleterCommand, CommandsHaveDescriptions)
{
  Completer c;
  auto result = c.complete("he", 2);
  ASSERT_EQ(result.candidates.size(), 1U);
  EXPECT_FALSE(result.candidates[0].display.empty());
}

/// @test
/// A dotted prefix offers no commands, since it names an object rather than a command.
TEST(CompleterCommand, CommandsNotShownForDottedPrefix)
{
  Completer c;
  auto result = c.complete("cd.", 3);
  EXPECT_FALSE(hasCandidate(result, "cd"));
}

//--------------------------------------------------------------------------------------------------------------
// cd argument completion tests
//--------------------------------------------------------------------------------------------------------------

/// @test
/// The cd command offers the up, root and back targets.
TEST(CompleterCd, SpecialTokens)
{
  Completer c;
  auto result = c.complete("cd ", 3);
  EXPECT_TRUE(hasCandidate(result, ".."));
  EXPECT_TRUE(hasCandidate(result, "/"));
  EXPECT_TRUE(hasCandidate(result, "-"));
}

/// @test
/// The cd command offers the children of the current scope.
TEST(CompleterCd, ChildNames)
{
  Completer c;
  CompleterTestAccess::setChildren(c, {"local", "remote"});
  auto result = c.complete("cd l", 4);
  EXPECT_EQ(result.candidates.size(), 1U);
  EXPECT_EQ(result.candidates[0].text, "local");
}

/// @test
/// The cd command offers every matching child.
TEST(CompleterCd, MultipleChildren)
{
  Completer c;
  CompleterTestAccess::setChildren(c, {"demo", "data", "log"});
  auto result = c.complete("cd d", 4);
  EXPECT_EQ(result.candidates.size(), 2U);
  EXPECT_TRUE(hasCandidate(result, "demo"));
  EXPECT_TRUE(hasCandidate(result, "data"));
}

/// @test
/// The cd command offers the query scopes.
TEST(CompleterCd, QueryScopes)
{
  Completer c;
  CompleterTestAccess::setQueryNames(c, {"sensors", "actuators"});
  auto result = c.complete("cd @", 4);
  EXPECT_TRUE(hasCandidate(result, "@sensors"));
  EXPECT_TRUE(hasCandidate(result, "@actuators"));
}

/// @test
/// The cd command offers session names from the sources that are available.
TEST(CompleterCd, AvailableSessionSources)
{
  // cd shows undotted sources (sessions) only; dotted bus addresses are excluded.
  Completer c;
  CompleterTestAccess::setAvailableSources(c, {"local", "remote"});
  auto result = c.complete("cd ", 3);
  EXPECT_TRUE(hasCandidate(result, "local"));
  EXPECT_TRUE(hasCandidate(result, "remote"));
}

/// @test
/// A prefix carrying a dot offers bus addresses under that session.
TEST(CompleterCd, DottedSourcesOfferedWhenPrefixHasDot)
{
  // Typing "cd local." should suggest buses like "local.main", "local.log".
  Completer c;
  CompleterTestAccess::setAvailableSources(c, {"local.main", "local.log"});
  auto result = c.complete("cd local.", 9);
  EXPECT_TRUE(hasCandidate(result, "local.main"));
  EXPECT_TRUE(hasCandidate(result, "local.log"));
}

/// @test
/// A prefix with no dot offers session names only, not bus addresses.
TEST(CompleterCd, DottedSourcesExcludedWhenPrefixUndotted)
{
  // Typing "cd l" should NOT suggest "local.main" (only undotted session names).
  Completer c;
  CompleterTestAccess::setAvailableSources(c, {"local.main", "local.log"});
  auto result = c.complete("cd l", 4);
  EXPECT_FALSE(hasCandidate(result, "local.main"));
  EXPECT_FALSE(hasCandidate(result, "local.log"));
}

/// @test
/// A cd completion replaces the typed argument and leaves the command alone.
TEST(CompleterCd, ReplaceRange)
{
  Completer c;
  CompleterTestAccess::setChildren(c, {"demo"});
  auto result = c.complete("cd de", 5);
  EXPECT_EQ(result.replaceFrom, 3);
  EXPECT_EQ(result.replaceTo, 5);
}

/// @test
/// With no argument typed, cd offers the special targets, the children and the queries.
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

/// @test
/// With both children and sources populated, the cd command offers the child and still excludes
/// the dotted bus address.
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

/// @test
/// The open command offers the sources starting with the typed prefix.
TEST(CompleterOpen, FiltersByPrefix)
{
  Completer c;
  CompleterTestAccess::setAvailableSources(c, {"local.main", "local.log", "remote.data"});
  auto result = c.complete("open local.", 11);
  EXPECT_EQ(result.candidates.size(), 2U);
  EXPECT_TRUE(hasCandidate(result, "local.main"));
  EXPECT_TRUE(hasCandidate(result, "local.log"));
}

/// @test
/// With nothing typed, open offers every available source.
TEST(CompleterOpen, EmptyPrefix)
{
  Completer c;
  CompleterTestAccess::setAvailableSources(c, {"local.main", "remote.data"});
  auto result = c.complete("open ", 5);
  EXPECT_EQ(result.candidates.size(), 2U);
}

/// @test
/// The open command offers nothing when no source is available.
TEST(CompleterOpen, NoSources)
{
  Completer c;
  auto result = c.complete("open ", 5);
  EXPECT_EQ(result.candidates.size(), 0U);
}

/// @test
/// An open completion replaces the typed argument and leaves the command alone.
TEST(CompleterOpen, ReplaceRange)
{
  Completer c;
  CompleterTestAccess::setAvailableSources(c, {"local.main"});
  auto result = c.complete("open lo", 7);
  EXPECT_EQ(result.replaceFrom, 5);
  EXPECT_EQ(result.replaceTo, 7);
}

/// @test
/// The close command offers the sources that are open.
TEST(CompleterClose, ClosesOpenSources)
{
  Completer c;
  CompleterTestAccess::setOpenSources(c, {"local.main", "local.log"});
  auto result = c.complete("close ", 6);
  EXPECT_EQ(result.candidates.size(), 2U);
  EXPECT_TRUE(hasCandidate(result, "local.main"));
  EXPECT_TRUE(hasCandidate(result, "local.log"));
}

/// @test
/// The close command offers the open sources starting with the typed prefix.
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

/// @test
/// The units command offers the category names it filters on, so the offer and the filter agree.
TEST(CompleterUnits, OffersTheNamesTheFilterMatches)
{
  // Two hand-written category tables had drifted: the completer offered "angularVelocity" while
  // cmdUnits filters on Unit::getCategoryString, which reports "angular velocity".
  Completer c;
  auto result = c.complete("units ", 6);
  EXPECT_TRUE(hasCandidate(result, "angular velocity"));
  EXPECT_FALSE(hasCandidate(result, "angularVelocity"));
}

/// @test
/// The units command offers torque, which is a registered category.
TEST(CompleterUnits, IncludesTorque)
{
  // Both tables omitted torque, so `units` could never list it though newton_meter is registered.
  Completer c;
  auto result = c.complete("units t", 7);
  EXPECT_TRUE(hasCandidate(result, "torque"));
}

/// @test
/// The units command offers every category the core defines, with none left out.
TEST(CompleterUnits, CoversEveryCategoryCoreDefines)
{
  Completer c;
  auto result = c.complete("units ", 6);
  EXPECT_EQ(result.candidates.size(), allUnitCategories.size());
}

/// @test
/// The log command offers its level subcommand.
TEST(CompleterLog, SubcommandLevel)
{
  Completer c;
  auto result = c.complete("log ", 4);
  EXPECT_EQ(result.candidates.size(), 1U);
  EXPECT_EQ(result.candidates[0].text, "level");
}

/// @test
/// A partial subcommand prefix offers the matching subcommand.
TEST(CompleterLog, SubcommandPartialPrefix)
{
  Completer c;
  auto result = c.complete("log l", 5);
  EXPECT_EQ(result.candidates.size(), 1U);
  EXPECT_EQ(result.candidates[0].text, "level");
}

/// @test
/// A subcommand prefix matching nothing offers nothing.
TEST(CompleterLog, SubcommandNoMatch)
{
  Completer c;
  auto result = c.complete("log x", 5);
  EXPECT_EQ(result.candidates.size(), 0U);
}

/// @test
/// After log level, every level name is offered.
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

/// @test
/// A partial level offers the levels starting with it.
TEST(CompleterLog, LevelPrefixFilter)
{
  Completer c;
  auto result = c.complete("log level tr", 12);
  EXPECT_EQ(result.candidates.size(), 1U);
  EXPECT_EQ(result.candidates[0].text, "trace");
}

/// @test
/// After log level, logger names are offered as well as levels.
TEST(CompleterLog, LevelAndLoggerNames)
{
  Completer c;
  CompleterTestAccess::setLoggerNames(c, {"kernel", "transport"});
  auto result = c.complete("log level ", 10);
  EXPECT_TRUE(hasCandidate(result, "info"));
  EXPECT_TRUE(hasCandidate(result, "kernel"));
  EXPECT_TRUE(hasCandidate(result, "transport"));
}

/// @test
/// After a logger name, log level offers the levels for that logger and not the logger again.
TEST(CompleterLog, PerLoggerLevel)
{
  Completer c;
  CompleterTestAccess::setLoggerNames(c, {"kernel"});
  auto result = c.complete("log level kernel ", 17);
  EXPECT_TRUE(hasCandidate(result, "trace"));
  EXPECT_TRUE(hasCandidate(result, "warn"));
  EXPECT_FALSE(hasCandidate(result, "kernel"));
}

/// @test
/// A log completion replaces the typed token and leaves the rest alone.
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

/// @test
/// The query rm command offers the names of the queries that exist.
TEST(CompleterQueryRm, QueryNames)
{
  Completer c;
  CompleterTestAccess::setQueryNames(c, {"sensors", "actuators"});
  auto result = c.complete("query rm ", 9);
  EXPECT_TRUE(hasCandidate(result, "sensors"));
  EXPECT_TRUE(hasCandidate(result, "actuators"));
}

/// @test
/// The query rm command offers the queries starting with the typed prefix.
TEST(CompleterQueryRm, FiltersByPrefix)
{
  Completer c;
  CompleterTestAccess::setQueryNames(c, {"sensors", "actuators"});
  auto result = c.complete("query rm s", 10);
  EXPECT_EQ(result.candidates.size(), 1U);
  EXPECT_EQ(result.candidates[0].text, "sensors");
}

/// @test
/// The query rm command offers nothing when no query exists.
TEST(CompleterQueryRm, NoQueries)
{
  Completer c;
  auto result = c.complete("query rm ", 9);
  EXPECT_EQ(result.candidates.size(), 0U);
}

/// @test
/// A query rm completion replaces the typed name and leaves the command alone.
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

/// @test
/// With nothing typed, only first segments are offered, alongside the commands.
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

/// @test
/// A dotted prefix offers the next segment, not the whole object name.
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

/// @test
/// A prefix naming a bus offers the objects on it.
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

/// @test
/// A simple prefix offers the leaf objects matching it.
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

/// @test
/// A segment shared by several objects is offered once.
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

/// @test
/// A path candidate carries a label saying what the segment is, and is of the path kind.
TEST(CompleterPath, PathCandidatesHaveDescriptiveDisplay)
{
  Completer c;
  CompleterTestAccess::setObjects(c, {{"local.demo.slowLogger", nullptr}});
  auto result = c.complete("", 0);
  // At root scope (default), first segment is a session
  EXPECT_EQ(displayOf(result, "local"), "session");
  const auto* candidate = test::findCandidate(result, "local");
  ASSERT_NE(candidate, nullptr);
  EXPECT_EQ(candidate->kind, CompletionKind::path);
}

/// @test
/// The label depends on the scope: the same segment is a session at the root, a bus from a
/// session, and a group from a bus or deeper.
TEST(CompleterPath, TheLabelIsRelativeToTheScopeYouAreStandingIn)
{
  // The same single segment is a session at root, a bus one level down and a group below that. Every
  // other case in this file runs at depth 0, which is the one depth where the `scopeDepth_ +` term
  // contributes nothing, so the arithmetic that matters in production would have no detector.
  const auto labelAtDepth = [](int depth)
  {
    Completer c;
    CompleterTestAccess::setScopeDepth(c, depth);
    CompleterTestAccess::setObjects(c, {{"alpha.beta.gamma", nullptr}});
    auto result = c.complete("", 0);
    return displayOf(result, "alpha");
  };

  EXPECT_EQ(labelAtDepth(0), "session") << "at root, a first segment is a session";
  EXPECT_EQ(labelAtDepth(1), "bus") << "standing on a session, the next segment is a bus";
  EXPECT_EQ(labelAtDepth(2), "group") << "standing on a bus, the next segment is a group";
  EXPECT_EQ(labelAtDepth(3), "group") << "deeper than a bus is still a group";
}

/// @test
/// A second segment is labelled as a bus.
TEST(CompleterPath, IntermediateSegmentLabelledAsBus)
{
  Completer c;
  CompleterTestAccess::setObjects(c, {{"local.demo.slowLogger", nullptr}});
  auto result = c.complete("local.", 6);
  // Second segment is a bus
  EXPECT_EQ(displayOf(result, "local.demo"), "bus");
}

/// @test
/// A leaf object is labelled with its class name.
TEST(CompleterPath, LeafCandidatesShowTheirClassName)
{
  // A real object, not a nullptr: "?" is the fallback for having no object at all, and asserting on it
  // would lock the degenerate output in as the expected one.
  Completer c;
  auto object = test::makeTestObject("term.local.main.slowLogger");
  ASSERT_TRUE(object);
  CompleterTestAccess::setObjects(c, {{"slowLogger", object}});
  auto result = c.complete("", 0);
  EXPECT_EQ(displayOf(result, "slowLogger"), object->getClass()->getName());
  EXPECT_NE(displayOf(result, "slowLogger"), "?");
}

/// @test
/// A trailing dot lists the object's methods, which is how a user asks for them.
TEST(CompleterMethod, ATrailingDotListsTheMethods)
{
  // The completer's split accepts a trailing dot where the engine's two copies reject it, because
  // typing "obj." and pressing Tab is how a user asks for the method list. That difference is
  // deliberate and easy to tidy away, so it is pinned here rather than only explained in a comment.
  Completer c;
  auto object = test::makeTestObject("term.local.main.slowLogger");
  ASSERT_TRUE(object);
  CompleterTestAccess::setObjects(c, {{"slowLogger", object}});

  auto result = c.complete("slowLogger.", 11);
  EXPECT_FALSE(result.candidates.empty()) << "a trailing dot produced no method candidates";
  for (const auto& candidate: result.candidates)
  {
    EXPECT_EQ(candidate.kind, CompletionKind::method) << candidate.text;
  }
}

/// @test
/// A leaf with no object behind it is labelled with the unknown marker.
TEST(CompleterPath, ALeafWithNoObjectShowsTheUnknownMarker)
{
  // The fallback, asserted on purpose and named for what it is.
  Completer c;
  CompleterTestAccess::setObjects(c, {{"slowLogger", nullptr}});
  auto result = c.complete("", 0);
  EXPECT_EQ(displayOf(result, "slowLogger"), "?");
}

/// @test
/// An intermediate segment is of the path kind.
TEST(CompleterPath, IntermediateSegmentsArePathKind)
{
  Completer c;
  CompleterTestAccess::setObjects(c, {{"local.demo.slowLogger", nullptr}});
  auto result = c.complete("local.", 6);
  const auto* candidate = test::findCandidate(result, "local.demo");
  ASSERT_NE(candidate, nullptr);
  EXPECT_EQ(candidate->kind, CompletionKind::path);
}

/// @test
/// A partial segment offers only the paths starting with it.
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

/// @test
/// With no objects, only commands are offered and no paths.
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

/// @test
/// Completing one segment leaves the prefix ready for the next.
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

/// @test
/// A simple prefix does not offer dotted object names.
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

/// @test
/// At the root every session is offered.
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

/// @test
/// A path completion replaces the whole typed path, back to the start of the line.
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

/// @test
/// A fully typed object name is not offered again, the completer having switched from object
/// names to method completion.
TEST(CompleterMethod, ExactObjectMatchShowsMethodsNotObject)
{
  // Exact object match switches to method completion, not object-name completion.
  Completer c;
  CompleterTestAccess::setObjects(c, {{"slowLogger", nullptr}});
  auto result = c.complete("slowLogger", 10);
  EXPECT_FALSE(hasCandidate(result, "slowLogger"));
}

/// @test
/// A trailing dot on a known object name offers neither commands nor the object itself.
TEST(CompleterMethod, DottedObjectPrefixGoesToMethods)
{
  // Trailing dot on a known object enters method completion, not path/command.
  Completer c;
  CompleterTestAccess::setObjects(c, {{"slowLogger", nullptr}});
  auto result = c.complete("slowLogger.", 11);
  EXPECT_FALSE(hasCandidate(result, "cd"));
  EXPECT_FALSE(hasCandidate(result, "slowLogger"));
}

/// @test
/// A dot-terminated path naming a known object no longer offers commands.
TEST(CompleterMethod, DottedPathObjectPrefixGoesToMethods)
{
  // Dotted path to a known object enters method completion.
  Completer c;
  CompleterTestAccess::setObjects(c, {{"local.demo.slowLogger", nullptr}});
  auto result = c.complete("local.demo.slowLogger.", 22);
  EXPECT_FALSE(hasCandidate(result, "cd"));
}

/// @test
/// A dot-terminated name matching no object offers nothing.
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

/// @test
/// With nothing typed the replace range covers nothing.
TEST(CompleterRange, EmptyInput)
{
  Completer c;
  auto result = c.complete("", 0);
  EXPECT_EQ(result.replaceFrom, 0);
  EXPECT_EQ(result.replaceTo, 0);
}

/// @test
/// A partial first token is the range to replace.
TEST(CompleterRange, FirstTokenPartial)
{
  Completer c;
  auto result = c.complete("hel", 3);
  EXPECT_EQ(result.replaceFrom, 0);
  EXPECT_EQ(result.replaceTo, 3);
}

/// @test
/// After a space the range starts at the second token.
TEST(CompleterRange, SecondTokenAfterSpace)
{
  Completer c;
  CompleterTestAccess::setAvailableSources(c, {"local.main"});
  auto result = c.complete("open ", 5);
  EXPECT_EQ(result.replaceFrom, 5);
  EXPECT_EQ(result.replaceTo, 5);
}

/// @test
/// A partial second token is the range to replace.
TEST(CompleterRange, SecondTokenPartial)
{
  Completer c;
  CompleterTestAccess::setAvailableSources(c, {"local.main"});
  auto result = c.complete("open lo", 7);
  EXPECT_EQ(result.replaceFrom, 5);
  EXPECT_EQ(result.replaceTo, 7);
}

/// @test
/// A third token is the range to replace.
TEST(CompleterRange, ThirdToken)
{
  Completer c;
  auto result = c.complete("log level wa", 12);
  EXPECT_EQ(result.replaceFrom, 10);
  EXPECT_EQ(result.replaceTo, 12);
}

/// @test
/// A dotted first token is replaced whole.
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

/// @test
/// No candidate is offered twice.
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

/// @test
/// A command candidate is of the command kind.
TEST(CompleterKind, CommandsHaveCommandKind)
{
  Completer c;
  auto result = c.complete("cd", 2);
  ASSERT_EQ(result.candidates.size(), 1U);
  EXPECT_EQ(result.candidates[0].kind, CompletionKind::command);
}

/// @test
/// A path candidate is of the path kind.
TEST(CompleterKind, PathsHavePathKind)
{
  Completer c;
  CompleterTestAccess::setObjects(c, {{"local.demo.obj", nullptr}});
  auto result = c.complete("", 0);
  const auto* candidate = test::findCandidate(result, "local");
  ASSERT_NE(candidate, nullptr);
  EXPECT_EQ(candidate->kind, CompletionKind::path);
}

/// @test
/// A leaf object candidate is of the object kind.
TEST(CompleterKind, LeafObjectsHaveObjectKind)
{
  Completer c;
  CompleterTestAccess::setObjects(c, {{"slowLogger", nullptr}});
  auto result = c.complete("", 0);
  const auto* candidate = test::findCandidate(result, "slowLogger");
  ASSERT_NE(candidate, nullptr);
  EXPECT_EQ(candidate->kind, CompletionKind::object);
}

/// @test
/// A value candidate is of the value kind.
TEST(CompleterKind, ValuesHaveValueKind)
{
  Completer c;
  auto result = c.complete("log level ", 10);
  for (const auto& candidate: result.candidates)
  {
    EXPECT_EQ(candidate.kind, CompletionKind::value);
  }
}

/// @test
/// An unknown command offers no argument completions.
TEST(CompleterContext, UnknownCommandNoCompletions)
{
  Completer c;
  auto result = c.complete("foobar ", 7);
  EXPECT_EQ(result.candidates.size(), 0U);
}

/// @test
/// After a query name, the SELECT keyword is offered.
TEST(CompleterContext, QueryAfterNameSuggestsSelect)
{
  Completer c;
  CompleterTestAccess::setQueryNames(c, {"sensors"});
  auto result = c.complete("query xyz ", 10);
  // After "query <name>", suggest SELECT keyword.
  EXPECT_TRUE(hasCandidate(result, "SELECT"));
}

/// @test
/// Repeated spaces between tokens do not change which token is completed.
TEST(CompleterContext, MultipleSpacesBetweenTokens)
{
  Completer c;
  CompleterTestAccess::setChildren(c, {"local"});
  auto result = c.complete("cd   lo", 7);
  EXPECT_TRUE(hasCandidate(result, "local"));
}

//--------------------------------------------------------------------------------------------------------------
// Query completion: SELECT <Type> FROM <bus> WHERE ...
//--------------------------------------------------------------------------------------------------------------

/// @test
/// A named query is followed by SELECT, which is the only thing that can come next.
TEST(CompleterQuery, ANamedQueryIsFollowedBySelect)
{
  Completer c;
  auto result = c.complete("query myq ", 10);
  EXPECT_TRUE(hasCandidate(result, "SELECT"));
}

/// @test
/// After SELECT, a star stands for any type. The completer offers it whether or not it knows of
/// any types, because a query over everything is the one a user writes without reading the STL.
TEST(CompleterQuery, SelectOffersTheAnyTypeStar)
{
  Completer c;
  auto result = c.complete("query myq SELECT ", 17);
  EXPECT_TRUE(hasCandidate(result, "*"));
}

/// @test
/// After FROM, a star stands for any bus. The completer offers it even when it knows of no open or
/// available source.
TEST(CompleterQuery, FromOffersTheAnyBusStar)
{
  Completer c;
  auto result = c.complete("query myq SELECT * FROM ", 24);
  EXPECT_TRUE(hasCandidate(result, "*"));
}

/// @test
/// Past WHERE the completer stops. The predicate is an expression over property names and values,
/// which it has no way to anticipate, and offering a bus or a type there would be worse than
/// offering nothing.
TEST(CompleterQuery, PastWhereNothingIsOffered)
{
  Completer c;
  auto result = c.complete("query myq SELECT * FROM demo WHERE ", 35);
  EXPECT_TRUE(result.candidates.empty());
}

/// @test
/// The keyword scan reads the tokens that are complete, not the one being typed, so a half-typed
/// FROM still leaves the completer in the state SELECT put it in.
TEST(CompleterQuery, AHalfTypedKeywordDoesNotChangeTheState)
{
  Completer c;
  auto result = c.complete("query myq SELECT * FRO", 22);
  // Still the SELECT state: FRO is the prefix being completed, not a keyword behind the cursor.
  EXPECT_FALSE(hasCandidate(result, "*")) << "a star is not a completion of 'FRO'";
}

/// @test
/// The inspect command takes an object and not a command, so the command names that share the
/// namespace are taken back out of what it offers.
TEST(CompleterInspect, InspectOffersObjectsAndNotCommands)
{
  Completer c;
  auto result = c.complete("inspect ", 8);
  for (const auto& candidate: result.candidates)
  {
    EXPECT_NE(candidate.kind, CompletionKind::command) << "inspect offered the command " << candidate.text;
  }
}

}  // namespace
}  // namespace sen::components::term
