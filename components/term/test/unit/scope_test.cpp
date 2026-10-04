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

/// @test
/// A new scope is at the root, with no session and no bus.
TEST(ScopeNavigate, DefaultIsRoot)
{
  Scope s;
  EXPECT_EQ(s.getKind(), Scope::Kind::root);
  EXPECT_EQ(s.getPath(), "/");
  EXPECT_TRUE(s.getSession().empty());
  EXPECT_TRUE(s.getBus().empty());
}

/// @test
/// Navigating to a name from the root reaches that session.
TEST(ScopeNavigate, ToSession)
{
  Scope s;
  EXPECT_TRUE(s.navigate("local"));
  EXPECT_EQ(s.getKind(), Scope::Kind::session);
  EXPECT_EQ(s.getSession(), "local");
  EXPECT_EQ(s.getPath(), "/local");
}

/// @test
/// A dotted target from the root reaches a bus, splitting session from bus.
TEST(ScopeNavigate, ToBusWithDot)
{
  Scope s;
  EXPECT_TRUE(s.navigate("local.main"));
  EXPECT_EQ(s.getKind(), Scope::Kind::bus);
  EXPECT_EQ(s.getSession(), "local");
  EXPECT_EQ(s.getBus(), "main");
  EXPECT_EQ(s.getPath(), "/local.main");
}

/// @test
/// A plain name from a session reaches a bus of that session.
TEST(ScopeNavigate, ToBusFromSession)
{
  Scope s;
  s.navigate("local");
  EXPECT_TRUE(s.navigate("main"));
  EXPECT_EQ(s.getKind(), Scope::Kind::bus);
  EXPECT_EQ(s.getBus(), "main");
}

/// @test
/// A plain name from a bus reaches a group, shown after a slash in the path.
TEST(ScopeNavigate, ToGroupFromBus)
{
  Scope s;
  s.navigate("local.main");
  EXPECT_TRUE(s.navigate("Sensors"));
  EXPECT_EQ(s.getKind(), Scope::Kind::group);
  EXPECT_EQ(s.getGroupPath(), "Sensors");
  EXPECT_EQ(s.getPath(), "/local.main/Sensors");
}

/// @test
/// Nested groups are stored with dots, matching an object's local name, and displayed with
/// slashes.
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

/// @test
/// The path a scope displays navigates back to that same scope, from a scope standing elsewhere.
TEST(ScopeNavigate, TheDisplayedPathNavigatesBackToTheSameScope)
{
  // A dotted target is always a bus address, so a nested group displayed with dots must not be read
  // back as session "a", bus "b".
  //
  // Typed into a scope standing somewhere else, not a fresh one: a fresh Scope is at the root, the one
  // scope where a rule about leading separators cannot fire, so asserting there would test the property
  // under the only condition that cannot break it.
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

/// @test
/// A slash separated group target is stored with dots and matches objects under it.
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

/// @test
/// A group target with an empty segment is refused, since it would match nothing.
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

/// @test
/// A session or bus name containing a separator is refused, rather than landing on a name that
/// matches nothing.
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

/// @test
/// A target starting with a separator resolves against the root, from any scope.
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

/// @test
/// A trailing separator is refused in the plain and the dotted form alike.
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

/// @test
/// Up, back and query targets are refused when written with a leading separator.
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

/// @test
/// A scope names the bus it needs open once it reaches one, and nothing before that.
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

/// @test
/// Going up from a nested group unwinds one level at a time.
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

/// @test
/// A group contains its own objects and those of its children, but not those of a group whose
/// name merely starts with the same letters.
TEST(ScopeContains, GroupPrefixRespectsTheSeparator)
{
  // Group "a" must not swallow group "ab", which a bare prefix compare would.
  Scope s;
  s.navigate("local.main");
  ASSERT_TRUE(s.navigate("a"));
  EXPECT_TRUE(s.contains("comp.local.main.a.obj"));
  EXPECT_TRUE(s.contains("comp.local.main.a.deeper.obj"));
  EXPECT_FALSE(s.contains("comp.local.main.ab.obj"));
}

/// @test
/// A relative name strips the group prefix only at a segment boundary.
TEST(ScopeRelativeName, GroupPrefixStripRespectsTheSeparator)
{
  Scope s;
  s.navigate("local.main");
  s.navigate("a");
  // Without the boundary check this returned "b.obj" by chopping one character off "ab".
  EXPECT_EQ(s.relativeName("comp.local.main.ab.obj"), "ab.obj");
  EXPECT_EQ(s.relativeName("comp.local.main.a.obj"), "obj");
}

/// @test
/// A separator alone returns to the root.
TEST(ScopeNavigate, ToRoot)
{
  Scope s;
  s.navigate("local.main");
  EXPECT_TRUE(s.navigate("/"));
  EXPECT_EQ(s.getKind(), Scope::Kind::root);
  EXPECT_EQ(s.getPath(), "/");
}

/// @test
/// An at-prefixed target reaches a query scope.
TEST(ScopeNavigate, ToQuery)
{
  Scope s;
  EXPECT_TRUE(s.navigate("@myquery"));
  EXPECT_EQ(s.getKind(), Scope::Kind::query);
  EXPECT_EQ(s.getQueryName(), "myquery");
  EXPECT_EQ(s.getPath(), "@myquery");
}

/// @test
/// Going up from a bus returns to its session.
TEST(ScopeNavigate, UpFromBus)
{
  Scope s;
  s.navigate("local.main");
  EXPECT_TRUE(s.navigate(".."));
  EXPECT_EQ(s.getKind(), Scope::Kind::session);
  EXPECT_EQ(s.getSession(), "local");
}

/// @test
/// Going up from a session returns to the root.
TEST(ScopeNavigate, UpFromSession)
{
  Scope s;
  s.navigate("local");
  EXPECT_TRUE(s.navigate(".."));
  EXPECT_EQ(s.getKind(), Scope::Kind::root);
}

/// @test
/// Going up from the root is refused and leaves the scope at the root.
TEST(ScopeNavigate, UpFromRoot)
{
  Scope s;
  EXPECT_FALSE(s.navigate(".."));
  EXPECT_EQ(s.getKind(), Scope::Kind::root);
}

/// @test
/// Going up from a group returns to the bus.
TEST(ScopeNavigate, UpFromGroup)
{
  Scope s;
  s.navigate("local.main");
  s.navigate("Sensors");
  EXPECT_TRUE(s.navigate(".."));
  EXPECT_EQ(s.getKind(), Scope::Kind::bus);
  EXPECT_EQ(s.getGroupPath(), "");
}

/// @test
/// The back target returns to the previous scope.
TEST(ScopeNavigate, Back)
{
  Scope s;
  s.navigate("local.main");
  s.navigate("/");
  EXPECT_TRUE(s.navigate("-"));
  EXPECT_EQ(s.getKind(), Scope::Kind::bus);
  EXPECT_EQ(s.getPath(), "/local.main");
}

/// @test
/// The back target is refused when there is no previous scope.
TEST(ScopeNavigate, BackWithNoPrevious)
{
  Scope s;
  EXPECT_FALSE(s.navigate("-"));
}

/// @test
/// An empty target is refused.
TEST(ScopeNavigate, EmptyTarget)
{
  Scope s;
  EXPECT_FALSE(s.navigate(""));
}

/// @test
/// A query target with no name is refused.
TEST(ScopeNavigate, EmptyQueryName)
{
  Scope s;
  EXPECT_FALSE(s.navigate("@"));
}

/// @test
/// A target naming a bus and a group reaches the group and keeps the session and bus.
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

/// @test
/// The root contains every well formed object name.
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

/// @test
/// A session contains the objects of that session only.
TEST(ScopeContains, SessionMatchesCorrectSession)
{
  Scope s;
  s.navigate("local");
  EXPECT_TRUE(s.contains("term.local.main.Sensor.GPS"));
  EXPECT_TRUE(s.contains("term.local.demo.Object"));
  EXPECT_FALSE(s.contains("term.remote.main.Object"));
}

/// @test
/// A bus contains the objects of that bus only.
TEST(ScopeContains, BusMatchesCorrectBus)
{
  Scope s;
  s.navigate("local.main");
  EXPECT_TRUE(s.contains("term.local.main.Sensor.GPS"));
  EXPECT_TRUE(s.contains("term.local.main.Object"));
  EXPECT_FALSE(s.contains("term.local.demo.Object"));
  EXPECT_FALSE(s.contains("term.remote.main.Object"));
}

/// @test
/// A name with too few segments is contained by nothing.
TEST(ScopeContains, TooFewSegments)
{
  Scope s;
  s.navigate("local");
  EXPECT_FALSE(s.contains("term.local"));
  EXPECT_FALSE(s.contains("short"));
}

/// @test
/// A query scope contains every object, since the query decides membership.
TEST(ScopeContains, QueryContainsEverything)
{
  Scope s;
  s.navigate("@myquery");
  EXPECT_TRUE(s.contains("term.local.main.Object"));
}

//--------------------------------------------------------------------------------------------------------------
// relativeName tests
//--------------------------------------------------------------------------------------------------------------

/// @test
/// At the root a relative name keeps the session and the bus.
TEST(ScopeRelativeName, AtRoot)
{
  Scope s;
  EXPECT_EQ(s.relativeName("term.local.main.Sensor.GPS"), "local.main.Sensor.GPS");
}

/// @test
/// At a session the session is stripped.
TEST(ScopeRelativeName, AtSession)
{
  Scope s;
  s.navigate("local");
  EXPECT_EQ(s.relativeName("term.local.main.Sensor.GPS"), "main.Sensor.GPS");
}

/// @test
/// At a bus the session and bus are stripped.
TEST(ScopeRelativeName, AtBus)
{
  Scope s;
  s.navigate("local.main");
  EXPECT_EQ(s.relativeName("term.local.main.Sensor.GPS"), "Sensor.GPS");
}

/// @test
/// At a bus a leaf object is named by itself.
TEST(ScopeRelativeName, AtBusLeafObject)
{
  Scope s;
  s.navigate("local.main");
  EXPECT_EQ(s.relativeName("term.local.main.GPS"), "GPS");
}

/// @test
/// At a group the group prefix is stripped as well.
TEST(ScopeRelativeName, AtGroup)
{
  Scope s;
  s.navigate("local.main");
  s.navigate("Sensors");
  EXPECT_EQ(s.relativeName("term.local.main.Sensors.GPS"), "GPS");
}

/// @test
/// A name with too few segments is returned unchanged.
TEST(ScopeRelativeName, TooFewSegments)
{
  Scope s;
  EXPECT_EQ(s.relativeName("short"), "short");
}

/// @test
/// At a query a relative name keeps the session and the bus, as at the root.
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

/// @test
/// The root prompt shows the root path.
TEST(ScopePrompt, Root)
{
  Scope s;
  EXPECT_EQ(s.makePrompt(), "sen:/\u276f ");
}

/// @test
/// A session prompt shows the session.
TEST(ScopePrompt, Session)
{
  Scope s;
  s.navigate("local");
  EXPECT_EQ(s.makePrompt(), "sen:/local\u276f ");
}

/// @test
/// A bus prompt shows the session and the bus.
TEST(ScopePrompt, Bus)
{
  Scope s;
  s.navigate("local.main");
  EXPECT_EQ(s.makePrompt(), "sen:/local.main\u276f ");
}

/// @test
/// A group prompt shows the bus and the group.
TEST(ScopePrompt, Group)
{
  Scope s;
  s.navigate("local.main");
  s.navigate("Sensors");
  EXPECT_EQ(s.makePrompt(), "sen:/local.main/Sensors\u276f ");
}

/// @test
/// A query prompt shows the query name.
TEST(ScopePrompt, Query)
{
  Scope s;
  s.navigate("@myquery");
  EXPECT_EQ(s.makePrompt(), "sen:@myquery\u276f ");
}

/// @test
/// An empty path finds the node itself.
TEST(TreeNodeFindChild, EmptyPathReturnsThis)
{
  TreeNode root("root");
  std::vector<std::string> path;
  EXPECT_EQ(root.findChild(path), &root);
}

/// @test
/// A path that exists finds that child.
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

/// @test
/// A path that does not exist finds nothing.
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
