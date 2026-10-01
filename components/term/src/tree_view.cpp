// === tree_view.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "tree_view.h"

// component
#include "styles.h"

// sen
#include "sen/core/base/span.h"

// ftxui
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>

// std
#include <cstddef>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

namespace sen::components::term
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

// Box-drawing prefixes for tree rendering. The same glyphs unicode.h names, written out so they are
// compile-time constants: concatenating them would run before main and could throw where nothing can
// catch it.
constexpr std::string_view branchLeaf = "\u251c\u2500\u2500 ";  // ├──
constexpr std::string_view branchNode = "\u251c\u2500\u252c ";  // ├─┬
constexpr std::string_view cornerLeaf = "\u2514\u2500\u2500 ";  // └──
constexpr std::string_view cornerNode = "\u2514\u2500\u252c ";  // └─┬
constexpr std::string_view pipe = "\u2502 ";                    // │
constexpr std::string_view space = "  ";

ftxui::Color kindColor(TreeNode::Kind kind)
{
  switch (kind)
  {
    case TreeNode::Kind::session:
      return styles::treeSession();
    case TreeNode::Kind::bus:
      return styles::treeBus();
    case TreeNode::Kind::group:
      return styles::treeGroup();
    case TreeNode::Kind::object:
      return styles::treeObject();
    case TreeNode::Kind::plain:
      return styles::treePlain();
  }
  return styles::treePlain();
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// TreeNode
//--------------------------------------------------------------------------------------------------------------

TreeNode::TreeNode(std::string name, std::string annotation): name_(std::move(name)), annotation_(std::move(annotation))
{
}

void TreeNode::clear() noexcept
{
  annotation_.clear();
  children_.clear();
  childIndex_.clear();
  kind_ = Kind::plain;
}

TreeNode* TreeNode::getOrCreateChild(Span<const std::string> path)
{
  if (path.empty())
  {
    return this;
  }

  const auto& childName = path[0];
  auto rest = path.subspan(1);

  if (auto itr = childIndex_.find(childName); itr != childIndex_.end())
  {
    return rest.empty() ? itr->second : itr->second->getOrCreateChild(rest);
  }

  children_.emplace_back(childName);
  auto* newChild = &children_.back();
  newChild->parent_ = this;
  childIndex_.emplace(childName, newChild);

  return rest.empty() ? newChild : newChild->getOrCreateChild(rest);
}

TreeNode* TreeNode::findChild(Span<const std::string> path)
{
  if (path.empty())
  {
    return this;
  }

  const auto& childName = path[0];
  auto rest = path.subspan(1);

  auto itr = childIndex_.find(childName);
  if (itr == childIndex_.end())
  {
    return nullptr;
  }
  return rest.empty() ? itr->second : itr->second->findChild(rest);
}

void TreeNode::setAnnotation(std::string annotation) { annotation_ = std::move(annotation); }

std::string_view TreeNode::getAnnotation() const noexcept { return annotation_; }

void TreeNode::setKind(Kind kind) { kind_ = kind; }

TreeNode* TreeNode::getParent() noexcept { return parent_; }

bool TreeNode::empty() const noexcept { return children_.empty(); }

void TreeNode::render(const std::function<void(ftxui::Element)>& emit) const
{
  for (auto itr = children_.begin(); itr != children_.end(); ++itr)
  {
    itr->renderImpl(emit, "", std::next(itr) == children_.end(), 0);
  }
}

void TreeNode::renderImpl(const std::function<void(ftxui::Element)>& emit,
                          std::string_view prefix,
                          bool isLast,
                          std::size_t depth) const
{
  // The names come from a peer, so the nesting is not ours to trust.
  constexpr std::size_t maxRenderDepth = 64;
  if (depth > maxRenderDepth)
  {
    emit(ftxui::text(std::string(prefix) + "<nested deeper than 64 levels>"));
    return;
  }

  bool hasChildren = !children_.empty();

  const std::string_view connector =
    isLast ? (hasChildren ? cornerNode : cornerLeaf) : (hasChildren ? branchNode : branchLeaf);

  ftxui::Elements parts;
  parts.push_back(ftxui::text(std::string(prefix).append(connector)) | ftxui::color(styles::treeConnector()));

  auto nameColor = kindColor(kind_);
  auto nameStyle = (kind_ == Kind::object) ? ftxui::bold : ftxui::nothing;
  parts.push_back(ftxui::text(name_) | ftxui::color(nameColor) | nameStyle);

  if (!annotation_.empty())
  {
    parts.push_back(ftxui::text(" " + annotation_) | ftxui::color(styles::treeConnector()));
  }

  emit(ftxui::hbox(std::move(parts)));

  std::string childPrefix(prefix);
  childPrefix += isLast ? space : pipe;

  for (auto itr = children_.begin(); itr != children_.end(); ++itr)
  {
    itr->renderImpl(emit, childPrefix, std::next(itr) == children_.end(), depth + 1);
  }
}

}  // namespace sen::components::term
