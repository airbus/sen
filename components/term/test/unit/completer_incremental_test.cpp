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

// google test
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <string>

namespace sen::components::term
{

/// Test helper that exposes read access to the completer's private state, just what the incremental
/// path needs to assert on.
class CompleterIncrementalAccess
{
public:
  static std::vector<std::string> childNames(const Completer& c) { return c.childNames_; }
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
  void SetUp() override { CompleterIncrementalAccess::clearScopeDirty(c_); }

  Completer c_;
  Scope scope_;  // defaults to root
};

//--------------------------------------------------------------------------------------------------------------
// Add / remove invariants
//--------------------------------------------------------------------------------------------------------------

TEST_F(CompleterIncrementalTest, AddSurfacesChildName)
{
  // Object name "term.foo.bus.obj" → at root scope: rel = "foo.bus.obj", first segment = "foo".
  auto obj = makeScopedObject("foo", "bus", "obj");
  c_.onObjectAdded(scope_, obj);

  auto names = CompleterIncrementalAccess::childNames(c_);
  ASSERT_EQ(names.size(), 1U);
  EXPECT_EQ(names[0], "foo");
  EXPECT_EQ(CompleterIncrementalAccess::objectCount(c_), 1U);
}

TEST_F(CompleterIncrementalTest, RemoveRetractsChildName)
{
  auto obj = makeScopedObject("foo", "bus", "obj");
  c_.onObjectAdded(scope_, obj);
  c_.onObjectRemoved(scope_, obj);

  EXPECT_TRUE(CompleterIncrementalAccess::childNames(c_).empty());
  EXPECT_EQ(CompleterIncrementalAccess::objectCount(c_), 0U);
}

TEST_F(CompleterIncrementalTest, RemoveOnlyDropsChildWhenRefcountReachesZero)
{
  // Two objects share the same first segment, removing one should keep "foo" in childNames_.
  auto a = makeScopedObject("foo", "bus", "a");
  auto b = makeScopedObject("foo", "bus", "b");
  c_.onObjectAdded(scope_, a);
  c_.onObjectAdded(scope_, b);

  auto names = CompleterIncrementalAccess::childNames(c_);
  ASSERT_EQ(names.size(), 1U);
  EXPECT_EQ(names[0], "foo");

  c_.onObjectRemoved(scope_, a);
  names = CompleterIncrementalAccess::childNames(c_);
  ASSERT_EQ(names.size(), 1U);  // "foo" still present (b remains)
  EXPECT_EQ(names[0], "foo");

  c_.onObjectRemoved(scope_, b);
  EXPECT_TRUE(CompleterIncrementalAccess::childNames(c_).empty());
}

TEST_F(CompleterIncrementalTest, DoubleAddDoesNotDoubleCount)
{
  auto obj = makeScopedObject("foo", "bus", "obj");
  c_.onObjectAdded(scope_, obj);
  c_.onObjectAdded(scope_, obj);  // same object again, refcount should not inflate

  auto names = CompleterIncrementalAccess::childNames(c_);
  ASSERT_EQ(names.size(), 1U);
  EXPECT_EQ(names[0], "foo");
  EXPECT_EQ(CompleterIncrementalAccess::objectCount(c_), 1U);

  // A single remove is enough to clear (idempotent add was a no-op).
  c_.onObjectRemoved(scope_, obj);
  EXPECT_TRUE(CompleterIncrementalAccess::childNames(c_).empty());
}

TEST_F(CompleterIncrementalTest, AddedChildrenStaySorted)
{
  // Insert out of order, childNames_ should remain sorted.
  c_.onObjectAdded(scope_, makeScopedObject("charlie", "bus", "obj"));
  c_.onObjectAdded(scope_, makeScopedObject("alpha", "bus", "obj"));
  c_.onObjectAdded(scope_, makeScopedObject("bravo", "bus", "obj"));

  auto names = CompleterIncrementalAccess::childNames(c_);
  ASSERT_EQ(names.size(), 3U);
  EXPECT_EQ(names[0], "alpha");
  EXPECT_EQ(names[1], "bravo");
  EXPECT_EQ(names[2], "charlie");
}

TEST_F(CompleterIncrementalTest, RemoveUnknownIsNoop)
{
  auto a = makeScopedObject("alpha", "bus", "obj");
  c_.onObjectAdded(scope_, a);

  // Remove an object that was never added, should not corrupt state.
  auto bogus = makeScopedObject("neverAdded", "bus", "obj");
  c_.onObjectRemoved(scope_, bogus);

  auto names = CompleterIncrementalAccess::childNames(c_);
  ASSERT_EQ(names.size(), 1U);
  EXPECT_EQ(names[0], "alpha");
  EXPECT_EQ(CompleterIncrementalAccess::objectCount(c_), 1U);
}

TEST_F(CompleterIncrementalTest, FindObjectAfterAdd)
{
  auto obj = makeScopedObject("sesA", "bus", "target");
  c_.onObjectAdded(scope_, obj);

  // At root scope, relativeName strips the component prefix → "sesA.bus.target".
  auto found = CompleterIncrementalAccess::find(c_, "sesA.bus.target");
  EXPECT_EQ(found, obj);

  EXPECT_EQ(CompleterIncrementalAccess::find(c_, "sesA.bus.missing"), nullptr);
}

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

TEST_F(CompleterIncrementalTest, MethodCompletionsCarryArgCount)
{
  // Method completions carry the argument count (zero-arg method, multi-arg method, getter, setter).
  c_.onObjectAdded(scope_, makeScopedObject("ses", "bus", "obj"));

  const std::string path = "ses.bus.obj.";
  auto result = c_.complete(path, static_cast<int>(path.size()));
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

TEST_F(CompleterIncrementalTest, WatchCompletesObjectPropertyPairs)
{
  // Watch completion lists the object's properties with full path prefix.
  c_.onObjectAdded(scope_, makeScopedObject("ses", "bus", "obj"));

  auto result = c_.complete("watch ses.bus.obj.", 18);
  ASSERT_FALSE(result.candidates.empty());

  // TestObject declares `var status : string` and `var counter : i32`, both must come back.
  EXPECT_TRUE(hasCandidate(result, "ses.bus.obj.status"));
  EXPECT_TRUE(hasCandidate(result, "ses.bus.obj.counter"));
  // Property candidates are values, not methods.
  for (const auto& c: result.candidates)
  {
    EXPECT_EQ(c.kind, CompletionKind::value);
  }
}

TEST_F(CompleterIncrementalTest, UnwatchUsesSameCompletionAsWatch)
{
  // Unwatch uses the same completion as watch.
  c_.onObjectAdded(scope_, makeScopedObject("ses", "bus", "obj"));

  auto result = c_.complete("unwatch ses.bus.obj.", 20);
  EXPECT_FALSE(result.candidates.empty());
}

TEST_F(CompleterIncrementalTest, WatchNeverSuggestsCommandsOrMethods)
{
  // Watch/unwatch never suggests commands or methods, only object paths and properties.
  c_.onObjectAdded(scope_, makeScopedObject("ses", "bus", "obj"));

  auto exactObject = c_.complete("watch ses.bus.obj", 17);
  ASSERT_FALSE(exactObject.candidates.empty());
  for (const auto& c: exactObject.candidates)
  {
    EXPECT_NE(c.kind, CompletionKind::command);
    EXPECT_NE(c.kind, CompletionKind::method);
  }

  auto partial = c_.complete("watch ses.", 10);
  for (const auto& c: partial.candidates)
  {
    EXPECT_NE(c.kind, CompletionKind::command);
    EXPECT_NE(c.kind, CompletionKind::method);
  }

  auto dotPrefix = c_.complete("watch ses.bus.obj.st", 20);
  ASSERT_FALSE(dotPrefix.candidates.empty());
  for (const auto& c: dotPrefix.candidates)
  {
    EXPECT_NE(c.kind, CompletionKind::method);
    // Properties are `value` kind per completePropertyName.
    EXPECT_EQ(c.kind, CompletionKind::value);
  }
  // ...and the `status` property must appear.
  EXPECT_TRUE(hasCandidate(dotPrefix, "ses.bus.obj.status"));
}

TEST_F(CompleterIncrementalTest, FindObjectSuggestionsFromIncrementalState)
{
  c_.onObjectAdded(scope_, makeScopedObject("ses", "bus", "slowLogger"));
  c_.onObjectAdded(scope_, makeScopedObject("ses", "bus", "fastLogger"));
  c_.onObjectAdded(scope_, makeScopedObject("ses", "bus", "something"));

  // "logger" is a substring of two names → they should both turn up.
  auto sugg = c_.findObjectSuggestions("logger");
  EXPECT_FALSE(sugg.empty());
  auto hasSugg = [&](std::string_view needle)
  { return std::any_of(sugg.begin(), sugg.end(), [&](const auto& s) { return s.find(needle) != std::string::npos; }); };
  EXPECT_TRUE(hasSugg("slowLogger"));
  EXPECT_TRUE(hasSugg("fastLogger"));
}

TEST_F(CompleterIncrementalTest, ListenCompletesEventNames)
{
  // TestObject has no events, candidates should be empty, but the code path must not crash.
  c_.onObjectAdded(scope_, makeScopedObject("ses", "bus", "obj"));

  auto result = c_.complete("listen ses.bus.obj.", 19);
  for (const auto& c: result.candidates)
  {
    EXPECT_NE(c.kind, CompletionKind::command);
    EXPECT_NE(c.kind, CompletionKind::method);
  }
}

TEST_F(CompleterIncrementalTest, UnlistenAllCompletes)
{
  auto result = c_.complete("unlisten a", 10);
  EXPECT_TRUE(hasCandidate(result, "all"));
}

TEST_F(CompleterIncrementalTest, UnwatchAllCompletes)
{
  auto result = c_.complete("unwatch a", 9);
  EXPECT_TRUE(hasCandidate(result, "all"));
}

TEST_F(CompleterIncrementalTest, HelpCompletesCommandNames)
{
  auto result = c_.complete("help w", 6);
  EXPECT_TRUE(hasCandidate(result, "watch"));
}

}  // namespace
}  // namespace sen::components::term
