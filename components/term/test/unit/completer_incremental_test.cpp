// === completer_incremental_test.cpp ==================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "completer.h"
#include "scope.h"
#include "test_completer_utils.h"
#include "test_object_impl.h"

// sen
#include "sen/core/obj/object.h"

// google test
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace sen::components::term
{

/// Test helper that exposes read access to the completer's private state, just what the incremental
/// path needs to assert on.
class CompleterIncrementalAccess
{
public:
  /// Returned as a vector so the order assertions below can index it. The completer holds a set now:
  /// a sorted vector made every arriving object memmove half of it during discovery.
  static std::vector<std::string> childNames(const Completer& c)
  {
    return {c.childNames_.begin(), c.childNames_.end()};
  }
  static size_t objectCount(const Completer& c) { return c.objectsByName_.size(); }
  static std::shared_ptr<Object> find(const Completer& c, std::string_view relName) { return c.findObject(relName); }
  /// Simulate "initial update has happened": the completer starts scopeDirty=true so the first
  /// update() does a full rebuild; tests that exercise the incremental path want to bypass that.
  static void clearScopeDirty(Completer& c) { c.scopeDirty_ = false; }
};

namespace
{

using test::hasCandidate;

/// Build an object with a fully-qualified name `component.session.bus[.suffix]`.
/// `scope.contains()` requires at least 4 segments so tests construct objects with that shape
/// (simplest way to get a meaningful relative name at root scope).
std::shared_ptr<Object> makeScopedObject(std::string_view session, std::string_view bus, std::string_view suffix)
{
  std::string name = "term.";
  name += session;
  name += '.';
  name += bus;
  if (!suffix.empty())
  {
    name += '.';
    name += suffix;
  }
  return test::makeTestObject(name);
}

/// Fixture: the Completer starts with scopeDirty=true (waiting for the first rebuild).
/// Most tests here exercise the *post-rebuild* incremental path, so we clear the flag up front.
class CompleterIncrementalTest: public ::testing::Test
{
protected:
  void SetUp() override { CompleterIncrementalAccess::clearScopeDirty(completer); }

  // A fixture's members are its tests' locals, so the encapsulation the check asks for has no
  // owner to protect them from. Same reading as libs/core's test fixtures.
  Completer completer;  // NOLINT(misc-non-private-member-variables-in-classes)
  Scope rootScope;      // NOLINT(misc-non-private-member-variables-in-classes) defaults to root
};

//--------------------------------------------------------------------------------------------------------------
// Add / remove invariants
//--------------------------------------------------------------------------------------------------------------

/// @test
/// Adding an object surfaces the first segment of its relative name as a child of the scope.
TEST_F(CompleterIncrementalTest, AddSurfacesChildName)
{
  // Object name "term.foo.bus.obj" → at root scope: rel = "foo.bus.obj", first segment = "foo".
  auto obj = makeScopedObject("foo", "bus", "obj");
  completer.onObjectAdded(rootScope, obj);

  auto names = CompleterIncrementalAccess::childNames(completer);
  ASSERT_EQ(names.size(), 1U);
  EXPECT_EQ(names[0], "foo");
  EXPECT_EQ(CompleterIncrementalAccess::objectCount(completer), 1U);
}

/// @test
/// Removing the only object under a name retracts that name.
TEST_F(CompleterIncrementalTest, RemoveRetractsChildName)
{
  auto obj = makeScopedObject("foo", "bus", "obj");
  completer.onObjectAdded(rootScope, obj);
  completer.onObjectRemoved(rootScope, obj);

  EXPECT_TRUE(CompleterIncrementalAccess::childNames(completer).empty());
  EXPECT_EQ(CompleterIncrementalAccess::objectCount(completer), 0U);
}

/// @test
/// A name shared by two objects survives the removal of one of them, and is retracted once the
/// second is removed as well.
TEST_F(CompleterIncrementalTest, RemoveOnlyDropsChildWhenRefcountReachesZero)
{
  // Two objects share the same first segment, removing one should keep "foo" in childNames_.
  auto a = makeScopedObject("foo", "bus", "a");
  auto b = makeScopedObject("foo", "bus", "b");
  completer.onObjectAdded(rootScope, a);
  completer.onObjectAdded(rootScope, b);

  auto names = CompleterIncrementalAccess::childNames(completer);
  ASSERT_EQ(names.size(), 1U);
  EXPECT_EQ(names[0], "foo");

  completer.onObjectRemoved(rootScope, a);
  names = CompleterIncrementalAccess::childNames(completer);
  ASSERT_EQ(names.size(), 1U);  // "foo" still present (b remains)
  EXPECT_EQ(names[0], "foo");

  completer.onObjectRemoved(rootScope, b);
  EXPECT_TRUE(CompleterIncrementalAccess::childNames(completer).empty());
}

/// @test
/// Adding the same object twice counts it once.
TEST_F(CompleterIncrementalTest, DoubleAddDoesNotDoubleCount)
{
  auto obj = makeScopedObject("foo", "bus", "obj");
  completer.onObjectAdded(rootScope, obj);
  completer.onObjectAdded(rootScope, obj);  // same object again, refcount should not inflate

  auto names = CompleterIncrementalAccess::childNames(completer);
  ASSERT_EQ(names.size(), 1U);
  EXPECT_EQ(names[0], "foo");
  EXPECT_EQ(CompleterIncrementalAccess::objectCount(completer), 1U);

  // A single remove is enough to clear (idempotent add was a no-op).
  completer.onObjectRemoved(rootScope, obj);
  EXPECT_TRUE(CompleterIncrementalAccess::childNames(completer).empty());
}

/// @test
/// Child names are kept sorted however the objects were added.
TEST_F(CompleterIncrementalTest, AddedChildrenStaySorted)
{
  // Insert out of order, childNames_ should remain sorted.
  completer.onObjectAdded(rootScope, makeScopedObject("charlie", "bus", "obj"));
  completer.onObjectAdded(rootScope, makeScopedObject("alpha", "bus", "obj"));
  completer.onObjectAdded(rootScope, makeScopedObject("bravo", "bus", "obj"));

  auto names = CompleterIncrementalAccess::childNames(completer);
  ASSERT_EQ(names.size(), 3U);
  EXPECT_EQ(names[0], "alpha");
  EXPECT_EQ(names[1], "bravo");
  EXPECT_EQ(names[2], "charlie");
}

/// @test
/// Removing an object that was never added leaves the state unchanged.
TEST_F(CompleterIncrementalTest, RemoveUnknownIsNoop)
{
  auto a = makeScopedObject("alpha", "bus", "obj");
  completer.onObjectAdded(rootScope, a);

  // Remove an object that was never added, should not corrupt state.
  auto bogus = makeScopedObject("neverAdded", "bus", "obj");
  completer.onObjectRemoved(rootScope, bogus);

  auto names = CompleterIncrementalAccess::childNames(completer);
  ASSERT_EQ(names.size(), 1U);
  EXPECT_EQ(names[0], "alpha");
  EXPECT_EQ(CompleterIncrementalAccess::objectCount(completer), 1U);
}

/// @test
/// An added object is found by its name relative to the scope, and an absent one is not.
TEST_F(CompleterIncrementalTest, FindObjectAfterAdd)
{
  auto obj = makeScopedObject("sesA", "bus", "target");
  completer.onObjectAdded(rootScope, obj);

  // At root scope, relativeName strips the component prefix → "sesA.bus.target".
  auto found = CompleterIncrementalAccess::find(completer, "sesA.bus.target");
  EXPECT_EQ(found, obj);

  EXPECT_EQ(CompleterIncrementalAccess::find(completer, "sesA.bus.missing"), nullptr);
}

/// @test
/// An add is ignored while the scope is dirty, because the next update rebuilds everything.
TEST(CompleterIncrementalDirty, AddIsDroppedWhileScopeDirty)
{
  Completer c;  // fresh, scopeDirty_ defaults to true (waiting for initial rebuild)
  Scope scope;

  auto obj = makeScopedObject("foo", "bus", "obj");
  c.onObjectAdded(scope, obj);

  // While scope is dirty, adds are ignored (the next update() does a full rebuild).
  EXPECT_TRUE(CompleterIncrementalAccess::childNames(c).empty());
  EXPECT_EQ(CompleterIncrementalAccess::objectCount(c), 0U);
}

/// @test
/// A method completion carries its argument count, for methods with none and with several, and
/// for a property's getter and setter.
TEST_F(CompleterIncrementalTest, MethodCompletionsCarryArgCount)
{
  // Method completions carry the argument count (zero-arg method, multi-arg method, getter, setter).
  completer.onObjectAdded(rootScope, makeScopedObject("ses", "bus", "obj"));

  const std::string path = "ses.bus.obj.";
  auto result = completer.complete(path, static_cast<int>(path.size()));
  ASSERT_FALSE(result.candidates.empty()) << "No completions for path '" << path << "'";
  auto findByText = [&](std::string_view suffix) -> const Completion*
  {
    for (const auto& c: result.candidates)
    {
      if (c.text.size() >= suffix.size() && c.text.compare(c.text.size() - suffix.size(), suffix.size(), suffix) == 0)
      {
        return &c;
      }
    }
    return nullptr;
  };

  // ping(), no args.
  const auto* ping = findByText(".ping");
  ASSERT_NE(ping, nullptr);
  EXPECT_EQ(ping->kind, CompletionKind::method);
  EXPECT_EQ(ping->argCount, 0U);

  // add(a, b), two args.
  const auto* add = findByText(".add");
  ASSERT_NE(add, nullptr);
  EXPECT_EQ(add->kind, CompletionKind::method);
  EXPECT_EQ(add->argCount, 2U);

  // Property getter, zero args.
  const auto* getter = findByText(".getStatus");
  ASSERT_NE(getter, nullptr);
  EXPECT_EQ(getter->kind, CompletionKind::method);
  EXPECT_EQ(getter->argCount, 0U);

  // Property setter, one arg.
  const auto* setter = findByText(".setNextCounter");
  ASSERT_NE(setter, nullptr);
  EXPECT_EQ(setter->kind, CompletionKind::method);
  EXPECT_EQ(setter->argCount, 1U);
}

/// @test
/// A suggestion query for a substring returns every object name holding it, both slowLogger and
/// fastLogger for logger.
TEST_F(CompleterIncrementalTest, FindObjectSuggestionsFromIncrementalState)
{
  completer.onObjectAdded(rootScope, makeScopedObject("ses", "bus", "slowLogger"));
  completer.onObjectAdded(rootScope, makeScopedObject("ses", "bus", "fastLogger"));
  completer.onObjectAdded(rootScope, makeScopedObject("ses", "bus", "something"));

  // "logger" is a substring of two names → they should both turn up.
  auto sugg = completer.findObjectSuggestions("logger");
  EXPECT_FALSE(sugg.empty());
  auto hasSugg = [&](std::string_view needle)
  { return std::any_of(sugg.begin(), sugg.end(), [&](const auto& s) { return s.find(needle) != std::string::npos; }); };
  EXPECT_TRUE(hasSugg("slowLogger"));
  EXPECT_TRUE(hasSugg("fastLogger"));
}

/// @test
/// The listen command offers the event names the object declares, thresholdCrossed and tick, and
/// nothing of command or method kind.
TEST_F(CompleterIncrementalTest, ListenCompletesEventNames)
{
  // TestObject declares thresholdCrossed and tick, so this asserts on named candidates. With no events on
  // the fixture the candidate list is empty and the loop below, which is the whole test, never runs.
  completer.onObjectAdded(rootScope, makeScopedObject("ses", "bus", "obj"));

  auto result = completer.complete("listen ses.bus.obj.", 19);
  ASSERT_FALSE(result.candidates.empty()) << "no event names were offered";

  auto offers = [&result](std::string_view needle)
  {
    return std::any_of(result.candidates.begin(),
                       result.candidates.end(),
                       [&needle](const Completion& c) { return c.text.find(needle) != std::string::npos; });
  };
  EXPECT_TRUE(offers("thresholdCrossed")) << "the event with arguments was not offered";
  EXPECT_TRUE(offers("tick")) << "the event without arguments was not offered";

  for (const auto& c: result.candidates)
  {
    EXPECT_NE(c.kind, CompletionKind::command);
    EXPECT_NE(c.kind, CompletionKind::method);
  }
}

/// @test
/// The unlisten command offers the literal all as a candidate for its argument.
TEST_F(CompleterIncrementalTest, UnlistenAllCompletes)
{
  auto result = completer.complete("unlisten a", 10);
  EXPECT_TRUE(hasCandidate(result, "all"));
}

/// @test
/// The help command completes command names, offering listen and ls for the prefix l.
TEST_F(CompleterIncrementalTest, HelpCompletesCommandNames)
{
  auto result = completer.complete("help l", 6);
  EXPECT_TRUE(hasCandidate(result, "listen"));
  EXPECT_TRUE(hasCandidate(result, "ls"));
}

}  // namespace
}  // namespace sen::components::term
