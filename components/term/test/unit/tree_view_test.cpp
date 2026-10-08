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
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// std
#include <algorithm>
#include <chrono>
#include <initializer_list>
#include <string>
#include <tuple>
#include <utility>
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

/// @test
/// A new root holds no children.
TEST(TreeView, EmptyRootHasNoChildren)
{
  TreeNode root;
  auto path = pathOf({"missing"});
  EXPECT_EQ(root.findChild(path), nullptr);
}

/// @test
/// Asking for a child that is not there creates it.
TEST(TreeView, GetOrCreateAddsNewChild)
{
  TreeNode root;
  auto path = pathOf({"alpha"});
  auto* child = root.getOrCreateChild(path);
  ASSERT_NE(child, nullptr);
  EXPECT_EQ(child, root.findChild(path));
}

/// @test
/// Asking twice for the same child returns the same node rather than a second one.
TEST(TreeView, GetOrCreateIsIdempotent)
{
  TreeNode root;
  auto path = pathOf({"alpha"});
  auto* first = root.getOrCreateChild(path);
  auto* second = root.getOrCreateChild(path);
  EXPECT_EQ(first, second);
}

/// @test
/// Asking for a nested path creates each node along the way.
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

/// @test
/// A child points back at its parent.
TEST(TreeView, ParentPointersAreSet)
{
  TreeNode root;
  auto* a = root.getOrCreateChild(pathOf({"a"}));
  auto* b = root.getOrCreateChild(pathOf({"a", "b"}));
  EXPECT_EQ(a->getParent(), &root);
  EXPECT_EQ(b->getParent(), a);
  EXPECT_EQ(root.getParent(), nullptr);
}

/// @test
/// Clearing a node drops its children and its annotation.
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

/// @test
/// An annotation set on a node is read back unchanged.
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

/// @test
/// Rendering shows the name of each child.
TEST(TreeView, RenderShowsChildNames)
{
  TreeNode root;
  root.getOrCreateChild(pathOf({"alpha"}));
  root.getOrCreateChild(pathOf({"beta"}));
  auto out = renderTree(root);
  EXPECT_THAT(out, ::testing::HasSubstr("alpha"));
  EXPECT_THAT(out, ::testing::HasSubstr("beta"));
}

/// @test
/// Rendering emits one element per node, counting nested nodes once each.
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

/// @test
/// Rendering draws the tree with box drawing connectors.
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

/// @test
/// Rendering shows a node's annotation as well as its name.
TEST(TreeView, RenderShowsAnnotations)
{
  TreeNode root;
  auto* child = root.getOrCreateChild(pathOf({"thing"}));
  child->setAnnotation("[Remote]");
  auto out = renderTree(root);
  EXPECT_THAT(out, ::testing::HasSubstr("thing"));
  EXPECT_THAT(out, ::testing::HasSubstr("[Remote]"));
}

/// @test
/// A deeper node is indented further than its parent.
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

#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
constexpr bool underSanitizer = true;
#elif defined(__has_feature)
#  if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
constexpr bool underSanitizer = true;
#  else
constexpr bool underSanitizer = false;
#  endif
#else
constexpr bool underSanitizer = false;
#endif

/// @test
/// Building a flat bus scales linearly with the number of siblings, not quadratically. The shape
/// is measured by how the time grows when the count doubles, not against a wall-clock bound.
TEST(TreeNode, BuildingAFlatBusIsNotQuadratic)
{
  // A sanitizer replaces the allocator, which is where this build spends its time, so the ratio here
  // would be a measurement of the sanitizer. The lanes without one keep the assertion.
  if (underSanitizer)
  {
    GTEST_SKIP() << "timing shape is not measurable under a sanitizer";
  }

  // `ls` builds one node per object, and a flat bus makes every object a sibling. A linear scan of those
  // siblings per insert makes the build N squared, which is a freeze of about a second at ten thousand
  // objects, on the thread that draws the screen.
  //
  // This measures the *shape*, not a time. Quadratic work quadruples when the count doubles; linear work
  // doubles. An absolute bound cannot do this job: a linear scan of twenty thousand children still
  // finishes inside a second, so a wall-clock limit passes whatever the shape is.
  const auto buildTime = [](int childCount)
  {
    TreeNode root;
    const auto before = std::chrono::steady_clock::now();
    for (int i = 0; i < childCount; ++i)
    {
      std::vector<std::string> path {"object_" + std::to_string(i)};
      root.getOrCreateChild(path);
    }
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - before).count();
  };

  // The fastest of several runs, not one run. A single timing of a few milliseconds is mostly
  // scheduling noise: measured over five runs on a loaded machine, one arm in five came in four times slow,
  // which is exactly the ratio a quadratic build produces, so the noise band covers the whole effect. The
  // minimum throws the hiccups away and keeps the shape.
  const auto fastestBuild = [&buildTime](int childCount)
  {
    constexpr int attempts = 5;
    auto best = buildTime(childCount);
    for (int i = 1; i < attempts; ++i)
    {
      best = std::min(best, buildTime(childCount));
    }
    return best;
  };

  // Warm up, so the first allocation and the first cache miss are not counted against the small half.
  std::ignore = buildTime(2000);

  const auto small = fastestBuild(40000);
  const auto large = fastestBuild(80000);
  ASSERT_GT(small, 0) << "the small build was too fast to time, so the ratio below means nothing";

  // Doubling the count: about 2 for linear, about 4 for quadratic. 3 sits between them with room on both
  // sides, since the scan measures near 4 and the index near 2.
  const double ratio = static_cast<double>(large) / static_cast<double>(small);
  EXPECT_LT(ratio, 3.0) << "doubling the object count multiplied the work by " << ratio
                        << ", which is the shape of a scan per insert (" << small << " then " << large
                        << " microseconds)";

  // And the index has to agree with the list it indexes, or a second `ls` finds the wrong node.
  TreeNode root;
  for (int i = 0; i < 100; ++i)
  {
    std::vector<std::string> path {"object_" + std::to_string(i)};
    ASSERT_NE(root.getOrCreateChild(path), nullptr) << "insert " << i << " returned nothing";
  }
  for (int i = 0; i < 100; i += 17)
  {
    std::vector<std::string> path {"object_" + std::to_string(i)};
    ASSERT_NE(root.findChild(path), nullptr) << "object_" << i << " was inserted and cannot be found";
  }
  std::vector<std::string> absent {"object_not_inserted"};
  EXPECT_EQ(root.findChild(absent), nullptr) << "a name that was never inserted was found";
}

}  // namespace sen::components::term
