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

TEST(ScopeBusAddress, AtBus)
{
  Scope s;
  s.navigate("local.main");
  EXPECT_EQ(s.getBusAddress(), "local.main");
}

TEST(ScopeBusAddress, AtRoot)
{
  Scope s;
  EXPECT_EQ(s.getBusAddress(), "");
}

TEST(ScopeBusAddress, AtSession)
{
  Scope s;
  s.navigate("local");
  EXPECT_EQ(s.getBusAddress(), "");
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
