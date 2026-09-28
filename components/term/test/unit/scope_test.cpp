// === scope_test.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "scope.h"
#include "tree_view.h"

// google test
#include <gtest/gtest.h>

// std
#include <string>
#include <vector>

namespace sen::components::term
{
namespace
{

//--------------------------------------------------------------------------------------------------------------
// Navigation tests
//--------------------------------------------------------------------------------------------------------------

TEST(ScopeNavigate, DefaultIsRoot)
{
  Scope s;
  EXPECT_EQ(s.getKind(), Scope::Kind::root);
  EXPECT_EQ(s.getPath(), "/");
  EXPECT_TRUE(s.getSession().empty());
  EXPECT_TRUE(s.getBus().empty());
}

TEST(ScopeNavigate, ToSession)
{
  Scope s;
  EXPECT_TRUE(s.navigate("local"));
  EXPECT_EQ(s.getKind(), Scope::Kind::session);
  EXPECT_EQ(s.getSession(), "local");
  EXPECT_EQ(s.getPath(), "/local");
}

TEST(ScopeNavigate, ToBusWithDot)
{
  Scope s;
  EXPECT_TRUE(s.navigate("local.main"));
  EXPECT_EQ(s.getKind(), Scope::Kind::bus);
  EXPECT_EQ(s.getSession(), "local");
  EXPECT_EQ(s.getBus(), "main");
  EXPECT_EQ(s.getPath(), "/local.main");
}

TEST(ScopeNavigate, ToBusFromSession)
{
  Scope s;
  s.navigate("local");
  EXPECT_TRUE(s.navigate("main"));
  EXPECT_EQ(s.getKind(), Scope::Kind::bus);
  EXPECT_EQ(s.getBus(), "main");
}

TEST(ScopeNavigate, ToGroupFromBus)
{
  Scope s;
  s.navigate("local.main");
  EXPECT_TRUE(s.navigate("Sensors"));
  EXPECT_EQ(s.getKind(), Scope::Kind::group);
  EXPECT_EQ(s.getGroupPath(), "Sensors");
  EXPECT_EQ(s.getPath(), "/local.main/Sensors");
}

TEST(ScopeNavigate, NestedGroupsStoreADotAndDisplayASlash)
{
  // Two plain cds must not build "a/b" and compare it against the dotted path taken from an object's local
  // name: nothing matches and `ls` silently prints nothing.
  Scope s;
  s.navigate("local.main");
  ASSERT_TRUE(s.navigate("a"));
  ASSERT_TRUE(s.navigate("b"));
  EXPECT_EQ(s.getKind(), Scope::Kind::group);
  EXPECT_EQ(s.getGroupPath(), "a.b");
  EXPECT_EQ(s.getPath(), "/local.main/a/b");
  EXPECT_TRUE(s.contains("comp.local.main.a.b.obj"));
  EXPECT_EQ(s.relativeName("comp.local.main.a.b.obj"), "obj");
}

TEST(ScopeNavigate, TheDisplayedPathNavigatesBackToTheSameScope)
{
  // A dotted target is always a bus address, so a nested group displayed with dots could not be typed
  // back: it silently landed on session "a", bus "b".
  //
  // Typed into a scope that is standing somewhere else, not a fresh one. A fresh Scope is at the root,
  // which is the single scope where a rule about leading separators cannot fire. Asserting the property
  // there only would test it under the one condition that cannot break it, and would miss a regression that
  // refuses `/local` everywhere else.
  const std::vector<std::string> paths {"/", "/local", "/local.main", "/local.main/a/b"};
  for (const auto& path: paths)
  {
    Scope printed;
    ASSERT_TRUE(printed.navigate(path) || path == "/") << path;
    ASSERT_EQ(printed.getPath(), path) << "not the path it prints: " << path;

    // Stand somewhere unrelated, then type what the prompt showed.
    Scope elsewhere;
    elsewhere.navigate("other.bus");
    elsewhere.navigate("somegroup");
    ASSERT_TRUE(elsewhere.navigate(path)) << "the prompt's own path was refused: " << path;
    EXPECT_EQ(elsewhere.getPath(), path);
    EXPECT_EQ(elsewhere.getKind(), printed.getKind()) << path;
    EXPECT_EQ(elsewhere.getSession(), printed.getSession()) << path;
    EXPECT_EQ(elsewhere.getBus(), printed.getBus()) << path;
    EXPECT_EQ(elsewhere.getGroupPath(), printed.getGroupPath()) << path;
  }
}

TEST(ScopeNavigate, SlashSeparatedGroupsAreStoredWithDots)
{
  Scope s;
  ASSERT_TRUE(s.navigate("local.main/a/b"));
  EXPECT_EQ(s.getGroupPath(), "a.b");
  EXPECT_TRUE(s.contains("comp.local.main.a.b.obj"));

  Scope t;
  t.navigate("local.main");
  ASSERT_TRUE(t.navigate("a/b"));
  EXPECT_EQ(t.getGroupPath(), "a.b");
  EXPECT_TRUE(t.contains("comp.local.main.a.b.obj"));
}

TEST(ScopeNavigate, AGroupTargetWithAnEmptySegmentIsRejected)
{
  // "a..b" or a trailing separator would match no object at all, which reads as an empty `ls`.
  Scope s;
  s.navigate("local.main");
  EXPECT_FALSE(s.navigate("a//b"));
  EXPECT_FALSE(s.navigate("a/"));
  EXPECT_FALSE(s.navigate("local.main/a//b"));
  EXPECT_EQ(s.getKind(), Scope::Kind::bus);
  EXPECT_TRUE(s.getGroupPath().empty());
}

TEST(ScopeNavigate, ASessionOrBusNameCannotCarryTheSeparator)
{
  // The guard has to come before the dotted branch. After it, it sees only an undotted target, and
  // `local/a.b` lands on session "local/a", where nothing matches and `ls` is silently empty.
  Scope root;
  EXPECT_FALSE(root.navigate("local/a"));
  EXPECT_FALSE(root.navigate("local/a.b"));
  EXPECT_FALSE(root.navigate("local/a.main/g"));
  EXPECT_FALSE(root.navigate("//local.main"));
  EXPECT_EQ(root.getKind(), Scope::Kind::root);
  EXPECT_TRUE(root.getSession().empty());

  Scope session;
  session.navigate("local");
  EXPECT_FALSE(session.navigate("main/extra"));
  EXPECT_EQ(session.getKind(), Scope::Kind::session);
  EXPECT_TRUE(session.getBus().empty());
}

TEST(ScopeNavigate, ALeadingSeparatorIsAlwaysAbsolute)
{
  // Absolute means resolved against the root, from wherever the user stands. Dropping the separator makes
  // `/y` at a group scope a child of that group; refusing it outside the root turns `/local` into an error
  // from every scope but one, including from `/local` itself, whose prompt prints it.
  Scope s;
  s.navigate("local.main");
  ASSERT_TRUE(s.navigate("x"));
  ASSERT_TRUE(s.navigate("/y"));
  EXPECT_EQ(s.getKind(), Scope::Kind::session);
  EXPECT_EQ(s.getSession(), "y");
  EXPECT_TRUE(s.getGroupPath().empty()) << "a leading separator was read as relative to the group";

  Scope bus;
  bus.navigate("local.main");
  ASSERT_TRUE(bus.navigate("/local"));
  EXPECT_EQ(bus.getKind(), Scope::Kind::session);
  EXPECT_EQ(bus.getPath(), "/local");

  Scope group;
  group.navigate("local.main");
  group.navigate("a");
  ASSERT_TRUE(group.navigate("/local.other"));
  EXPECT_EQ(group.getBus(), "other");
  EXPECT_TRUE(group.getGroupPath().empty());
}

TEST(ScopeNavigate, ATrailingSeparatorIsRefusedInBothForms)
{
  // `local.main/` was accepted where `a/` was refused, because the empty-segment test was skipped
  // for the dotted form.
  Scope s;
  s.navigate("local.main");
  EXPECT_FALSE(s.navigate("a/"));
  EXPECT_FALSE(s.navigate("local.main/"));
  EXPECT_EQ(s.getKind(), Scope::Kind::bus);
  EXPECT_TRUE(s.getGroupPath().empty());
}

TEST(ScopeNavigate, TheRelativeFormsRejectALeadingSeparator)
{
  Scope s;
  s.navigate("local.main");
  ASSERT_TRUE(s.navigate("a"));
  EXPECT_FALSE(s.navigate("/.."));
  EXPECT_FALSE(s.navigate("/-"));
  EXPECT_FALSE(s.navigate("/@q"));
  EXPECT_EQ(s.getGroupPath(), "a");
  EXPECT_EQ(s.getKind(), Scope::Kind::group);
}

TEST(ScopeBusAddress, NamesTheSourceAScopeNeedsOpen)
{
  // What `cd` opens after navigating, so the bus is subscribed whichever route reached it.
  Scope s;
  EXPECT_TRUE(s.getBusAddress().empty());
  s.navigate("local");
  EXPECT_TRUE(s.getBusAddress().empty());
  s.navigate("main");
  EXPECT_EQ(s.getBusAddress(), "local.main");
  s.navigate("a");
  EXPECT_EQ(s.getBusAddress(), "local.main");
}

TEST(ScopeNavigate, UpFromNestedGroupUnwindsOneLevel)
{
  // Three deep, asserting the intermediate: with one level the join separator and navigateUp's
  // rfind agree with each other whichever separator they use, so the test could not fail.
  Scope s;
  s.navigate("local.main");
  s.navigate("a");
  s.navigate("b");
  s.navigate("c");
  ASSERT_TRUE(s.navigateUp());
  EXPECT_EQ(s.getKind(), Scope::Kind::group);
  EXPECT_EQ(s.getGroupPath(), "a.b");
  EXPECT_EQ(s.getPath(), "/local.main/a/b");
  ASSERT_TRUE(s.navigateUp());
  EXPECT_EQ(s.getGroupPath(), "a");
  ASSERT_TRUE(s.navigateUp());
  EXPECT_EQ(s.getKind(), Scope::Kind::bus);
  EXPECT_TRUE(s.getGroupPath().empty());
}

TEST(ScopeContains, GroupPrefixRespectsTheSeparator)
{
  // Group "a" must not swallow group "ab": the old test was a bare prefix compare.
  Scope s;
  s.navigate("local.main");
  ASSERT_TRUE(s.navigate("a"));
  EXPECT_TRUE(s.contains("comp.local.main.a.obj"));
  EXPECT_TRUE(s.contains("comp.local.main.a.deeper.obj"));
  EXPECT_FALSE(s.contains("comp.local.main.ab.obj"));
}

TEST(ScopeRelativeName, GroupPrefixStripRespectsTheSeparator)
{
  Scope s;
  s.navigate("local.main");
  s.navigate("a");
  // Without the boundary check this returned "b.obj" by chopping one character off "ab".
  EXPECT_EQ(s.relativeName("comp.local.main.ab.obj"), "ab.obj");
  EXPECT_EQ(s.relativeName("comp.local.main.a.obj"), "obj");
}

TEST(ScopeNavigate, ToRoot)
{
  Scope s;
  s.navigate("local.main");
  EXPECT_TRUE(s.navigate("/"));
  EXPECT_EQ(s.getKind(), Scope::Kind::root);
  EXPECT_EQ(s.getPath(), "/");
}

TEST(ScopeNavigate, ToQuery)
{
  Scope s;
  EXPECT_TRUE(s.navigate("@myquery"));
  EXPECT_EQ(s.getKind(), Scope::Kind::query);
  EXPECT_EQ(s.getQueryName(), "myquery");
  EXPECT_EQ(s.getPath(), "@myquery");
}

TEST(ScopeNavigate, UpFromBus)
{
  Scope s;
  s.navigate("local.main");
  EXPECT_TRUE(s.navigate(".."));
  EXPECT_EQ(s.getKind(), Scope::Kind::session);
  EXPECT_EQ(s.getSession(), "local");
}

TEST(ScopeNavigate, UpFromSession)
{
  Scope s;
  s.navigate("local");
  EXPECT_TRUE(s.navigate(".."));
  EXPECT_EQ(s.getKind(), Scope::Kind::root);
}

TEST(ScopeNavigate, UpFromRoot)
{
  Scope s;
  EXPECT_FALSE(s.navigate(".."));
  EXPECT_EQ(s.getKind(), Scope::Kind::root);
}

TEST(ScopeNavigate, UpFromGroup)
{
  Scope s;
  s.navigate("local.main");
  s.navigate("Sensors");
  EXPECT_TRUE(s.navigate(".."));
  EXPECT_EQ(s.getKind(), Scope::Kind::bus);
  EXPECT_EQ(s.getGroupPath(), "");
}

TEST(ScopeNavigate, Back)
{
  Scope s;
  s.navigate("local.main");
  s.navigate("/");
  EXPECT_TRUE(s.navigate("-"));
  EXPECT_EQ(s.getKind(), Scope::Kind::bus);
  EXPECT_EQ(s.getPath(), "/local.main");
}

TEST(ScopeNavigate, BackWithNoPrevious)
{
  Scope s;
  EXPECT_FALSE(s.navigate("-"));
}

TEST(ScopeNavigate, EmptyTarget)
{
  Scope s;
  EXPECT_FALSE(s.navigate(""));
}

TEST(ScopeNavigate, EmptyQueryName)
{
  Scope s;
  EXPECT_FALSE(s.navigate("@"));
}

TEST(ScopeNavigate, BusAddressWithGroupPath)
{
  Scope s;
  EXPECT_TRUE(s.navigate("local.main/Sensors"));
  EXPECT_EQ(s.getKind(), Scope::Kind::group);
  EXPECT_EQ(s.getSession(), "local");
  EXPECT_EQ(s.getBus(), "main");
  EXPECT_EQ(s.getGroupPath(), "Sensors");
}

//--------------------------------------------------------------------------------------------------------------
// contains tests
//--------------------------------------------------------------------------------------------------------------

TEST(ScopeContains, RootContainsValidObjects)
{
  Scope s;
  EXPECT_TRUE(s.contains("term.local.main.Sensor.GPS"));
  EXPECT_TRUE(s.contains("term.remote.bus.Object"));
  // Objects with fewer than 4 segments are rejected (not valid component.session.bus.name format)
  EXPECT_FALSE(s.contains("anything"));
  EXPECT_FALSE(s.contains("short.name"));
  EXPECT_FALSE(s.contains("only.three.segments"));
}

TEST(ScopeContains, SessionMatchesCorrectSession)
{
  Scope s;
  s.navigate("local");
  EXPECT_TRUE(s.contains("term.local.main.Sensor.GPS"));
  EXPECT_TRUE(s.contains("term.local.demo.Object"));
  EXPECT_FALSE(s.contains("term.remote.main.Object"));
}

TEST(ScopeContains, BusMatchesCorrectBus)
{
  Scope s;
  s.navigate("local.main");
  EXPECT_TRUE(s.contains("term.local.main.Sensor.GPS"));
  EXPECT_TRUE(s.contains("term.local.main.Object"));
  EXPECT_FALSE(s.contains("term.local.demo.Object"));
  EXPECT_FALSE(s.contains("term.remote.main.Object"));
}

TEST(ScopeContains, TooFewSegments)
{
  Scope s;
  s.navigate("local");
  EXPECT_FALSE(s.contains("term.local"));
  EXPECT_FALSE(s.contains("short"));
}

TEST(ScopeContains, QueryContainsEverything)
{
  Scope s;
  s.navigate("@myquery");
  EXPECT_TRUE(s.contains("term.local.main.Object"));
}

//--------------------------------------------------------------------------------------------------------------
// relativeName tests
//--------------------------------------------------------------------------------------------------------------

TEST(ScopeRelativeName, AtRoot)
{
  Scope s;
  EXPECT_EQ(s.relativeName("term.local.main.Sensor.GPS"), "local.main.Sensor.GPS");
}

TEST(ScopeRelativeName, AtSession)
{
  Scope s;
  s.navigate("local");
  EXPECT_EQ(s.relativeName("term.local.main.Sensor.GPS"), "main.Sensor.GPS");
}

TEST(ScopeRelativeName, AtBus)
{
  Scope s;
  s.navigate("local.main");
  EXPECT_EQ(s.relativeName("term.local.main.Sensor.GPS"), "Sensor.GPS");
}

TEST(ScopeRelativeName, AtBusLeafObject)
{
  Scope s;
  s.navigate("local.main");
  EXPECT_EQ(s.relativeName("term.local.main.GPS"), "GPS");
}

TEST(ScopeRelativeName, AtGroup)
{
  Scope s;
  s.navigate("local.main");
  s.navigate("Sensors");
  EXPECT_EQ(s.relativeName("term.local.main.Sensors.GPS"), "GPS");
}

TEST(ScopeRelativeName, TooFewSegments)
{
  Scope s;
  EXPECT_EQ(s.relativeName("short"), "short");
}

TEST(ScopeRelativeName, AtQuery)
{
  Scope s;
  s.navigate("@myquery");
  // Query scope uses same start index as root (1)
  EXPECT_EQ(s.relativeName("term.local.main.Object"), "local.main.Object");
}

//--------------------------------------------------------------------------------------------------------------
// Prompt and path tests
//--------------------------------------------------------------------------------------------------------------

TEST(ScopePrompt, Root)
{
  Scope s;
  EXPECT_EQ(s.makePrompt(), "sen:/\u276f ");
}

TEST(ScopePrompt, Session)
{
  Scope s;
  s.navigate("local");
  EXPECT_EQ(s.makePrompt(), "sen:/local\u276f ");
}

TEST(ScopePrompt, Bus)
{
  Scope s;
  s.navigate("local.main");
  EXPECT_EQ(s.makePrompt(), "sen:/local.main\u276f ");
}

TEST(ScopePrompt, Group)
{
  Scope s;
  s.navigate("local.main");
  s.navigate("Sensors");
  EXPECT_EQ(s.makePrompt(), "sen:/local.main/Sensors\u276f ");
}

TEST(ScopePrompt, Query)
{
  Scope s;
  s.navigate("@myquery");
  EXPECT_EQ(s.makePrompt(), "sen:@myquery\u276f ");
}

TEST(TreeNodeFindChild, EmptyPathReturnsThis)
{
  TreeNode root("root");
  std::vector<std::string> path;
  EXPECT_EQ(root.findChild(path), &root);
}

TEST(TreeNodeFindChild, FindsExistingChild)
{
  TreeNode root;
  std::vector<std::string> createPath = {"local", "demo", "obj"};
  root.getOrCreateChild(createPath);

  std::vector<std::string> findPath = {"local", "demo"};
  auto* found = root.findChild(findPath);
  ASSERT_NE(found, nullptr);

  std::vector<std::string> leaf = {"obj"};
  EXPECT_NE(found->findChild(leaf), nullptr);
}

TEST(TreeNodeFindChild, MissingPathReturnsNull)
{
  TreeNode root;
  std::vector<std::string> createPath = {"local", "demo"};
  root.getOrCreateChild(createPath);

  std::vector<std::string> badPath = {"local", "missing"};
  EXPECT_EQ(root.findChild(badPath), nullptr);
}

}  // namespace
}  // namespace sen::components::term
