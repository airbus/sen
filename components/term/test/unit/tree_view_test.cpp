// === tree_view_test.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "test_render_utils.h"
#include "tree_view.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// google test
#include <gtest/gtest.h>

// std
#include <string>
#include <vector>

namespace sen::components::term
{
namespace
{

std::vector<std::string> pathOf(std::initializer_list<const char*> segments)
{
  std::vector<std::string> out;
  out.reserve(segments.size());
  for (const auto* s: segments)
  {
    out.emplace_back(s);
  }
  return out;
}

std::string renderTree(const TreeNode& root, int width = 60, int height = 20)
{
  ftxui::Elements lines;
  root.render([&](ftxui::Element e) { lines.push_back(std::move(e)); });
  return test::renderToText(ftxui::vbox(std::move(lines)), width, height);
}

//--------------------------------------------------------------------------------------------------------------
// Construction and child management
//--------------------------------------------------------------------------------------------------------------

TEST(TreeView, EmptyRootHasNoChildren)
{
  TreeNode root;
  auto path = pathOf({"missing"});
  EXPECT_EQ(root.findChild(path), nullptr);
}

TEST(TreeView, GetOrCreateAddsNewChild)
{
  TreeNode root;
  auto path = pathOf({"alpha"});
  auto* child = root.getOrCreateChild(path);
  ASSERT_NE(child, nullptr);
  EXPECT_EQ(child, root.findChild(path));
}

TEST(TreeView, GetOrCreateIsIdempotent)
{
  TreeNode root;
  auto path = pathOf({"alpha"});
  auto* first = root.getOrCreateChild(path);
  auto* second = root.getOrCreateChild(path);
  EXPECT_EQ(first, second);
}

TEST(TreeView, GetOrCreateBuildsIntermediatePath)
{
  TreeNode root;
  auto leaf = pathOf({"a", "b", "c"});
  auto* c = root.getOrCreateChild(leaf);
  ASSERT_NE(c, nullptr);
  EXPECT_NE(root.findChild(pathOf({"a"})), nullptr);
  EXPECT_NE(root.findChild(pathOf({"a", "b"})), nullptr);
  EXPECT_EQ(root.findChild(leaf), c);
}

TEST(TreeView, ParentPointersAreSet)
{
  TreeNode root;
  auto* a = root.getOrCreateChild(pathOf({"a"}));
  auto* b = root.getOrCreateChild(pathOf({"a", "b"}));
  EXPECT_EQ(a->getParent(), &root);
  EXPECT_EQ(b->getParent(), a);
  EXPECT_EQ(root.getParent(), nullptr);
}

TEST(TreeView, ClearDropsChildrenAndAnnotation)
{
  TreeNode root;
  root.getOrCreateChild(pathOf({"a"}));
  root.setAnnotation("hello");
  root.setKind(TreeNode::Kind::group);
  EXPECT_NE(root.findChild(pathOf({"a"})), nullptr);

  root.clear();
  EXPECT_EQ(root.findChild(pathOf({"a"})), nullptr);
  EXPECT_TRUE(root.getAnnotation().empty());
}

TEST(TreeView, AnnotationRoundTrip)
{
  TreeNode node("name");
  EXPECT_TRUE(node.getAnnotation().empty());
  node.setAnnotation("[Type, Remote]");
  EXPECT_EQ(node.getAnnotation(), "[Type, Remote]");
}

//--------------------------------------------------------------------------------------------------------------
// Rendering
//--------------------------------------------------------------------------------------------------------------

TEST(TreeView, RenderShowsChildNames)
{
  TreeNode root;
  root.getOrCreateChild(pathOf({"alpha"}));
  root.getOrCreateChild(pathOf({"beta"}));
  auto out = renderTree(root);
  EXPECT_THAT(out, ::testing::HasSubstr("alpha"));
  EXPECT_THAT(out, ::testing::HasSubstr("beta"));
}

TEST(TreeView, RenderEmitsOneElementPerNode)
{
  TreeNode root;
  root.getOrCreateChild(pathOf({"a", "b"}));
  root.getOrCreateChild(pathOf({"a", "c"}));
  root.getOrCreateChild(pathOf({"d"}));

  int count = 0;
  root.render([&](ftxui::Element) { ++count; });
  // a, a/b, a/c, d  → 4 nodes.
  EXPECT_EQ(count, 4);
}

TEST(TreeView, RenderUsesBoxDrawingConnectors)
{
  TreeNode root;
  root.getOrCreateChild(pathOf({"a"}));
  root.getOrCreateChild(pathOf({"b"}));
  auto out = renderTree(root);
  // Branch tee ├ for non-last siblings, corner └ for the last.
  EXPECT_THAT(out, ::testing::HasSubstr("\u251c"));
  EXPECT_THAT(out, ::testing::HasSubstr("\u2514"));
}

TEST(TreeView, RenderShowsAnnotations)
{
  TreeNode root;
  auto* child = root.getOrCreateChild(pathOf({"thing"}));
  child->setAnnotation("[Remote]");
  auto out = renderTree(root);
  EXPECT_THAT(out, ::testing::HasSubstr("thing"));
  EXPECT_THAT(out, ::testing::HasSubstr("[Remote]"));
}

TEST(TreeView, RenderNestedChildrenShowDeeperIndent)
{
  TreeNode root;
  root.getOrCreateChild(pathOf({"outer", "inner"}));
  auto out = renderTree(root);
  auto outerPos = out.find("outer");
  auto innerPos = out.find("inner");
  ASSERT_NE(outerPos, std::string::npos);
  ASSERT_NE(innerPos, std::string::npos);
  auto innerNl = out.rfind('\n', innerPos);
  auto innerCol = innerPos - (innerNl == std::string::npos ? 0U : innerNl + 1U);
  auto outerNl = out.rfind('\n', outerPos);
  auto outerCol = outerPos - (outerNl == std::string::npos ? 0U : outerNl + 1U);
  EXPECT_GT(innerCol, outerCol);
}

}  // namespace
}  // namespace sen::components::term
