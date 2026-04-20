// === tree_view.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "tree_view.h"

// local
#include "styles.h"
#include "unicode.h"

// std
#include <iterator>
#include <string>

namespace sen::components::term
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

// Box-drawing prefixes for tree rendering, composed from unicode.h primitives.
const std::string branchLeaf = std::string {unicode::branchTee} + unicode::horizontalBar + unicode::horizontalBar + " ";
const std::string branchNode = std::string {unicode::branchTee} + unicode::horizontalBar + unicode::teeDown + " ";
const std::string cornerLeaf = std::string {unicode::cornerEnd} + unicode::horizontalBar + unicode::horizontalBar + " ";
const std::string cornerNode = std::string {unicode::cornerEnd} + unicode::horizontalBar + unicode::teeDown + " ";
const std::string pipe = std::string {unicode::verticalBar} + " ";
const std::string space = "  ";

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
    case TreeNode::Kind::closed:
      return styles::treeClosed();
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

  for (auto& child: children_)
  {
    if (child.name_ == childName)
    {
      return rest.empty() ? &child : child.getOrCreateChild(rest);
    }
  }

  children_.emplace_back(childName);
  auto* newChild = &children_.back();
  newChild->parent_ = this;

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

  for (auto& child: children_)
  {
    if (child.name_ == childName)
    {
      return rest.empty() ? &child : child.findChild(rest);
    }
  }

  return nullptr;
}

void TreeNode::setAnnotation(std::string annotation) { annotation_ = std::move(annotation); }

std::string_view TreeNode::getAnnotation() const noexcept { return annotation_; }

void TreeNode::setKind(Kind kind) { kind_ = kind; }

TreeNode* TreeNode::getParent() noexcept { return parent_; }

void TreeNode::render(const std::function<void(ftxui::Element)>& emit) const
{
  for (auto itr = children_.begin(); itr != children_.end(); ++itr)
  {
    itr->renderImpl(emit, "", std::next(itr) == children_.end());
  }
}

void TreeNode::renderImpl(const std::function<void(ftxui::Element)>& emit, std::string_view prefix, bool isLast) const
{
  bool hasChildren = !children_.empty();

  const std::string& connector =
    isLast ? (hasChildren ? cornerNode : cornerLeaf) : (hasChildren ? branchNode : branchLeaf);

  ftxui::Elements parts;
  parts.push_back(ftxui::text(std::string(prefix) + connector) | ftxui::color(styles::treeConnector()));

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
    itr->renderImpl(emit, childPrefix, std::next(itr) == children_.end());
  }
}

}  // namespace sen::components::term
