// === terminal_tree.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "terminal_tree.h"

// component
#include "box_chars.h"
#include "styles.h"
#include "terminal.h"

// std
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sen::components::shell
{

Node::Node(Node* parent): parent_(parent) {}

Node* Node::getParent() noexcept { return parent_; }

void Node::setStyle(Style style) { style_ = style; }

void Node::setHoldsObject() { holdsObject_ = true; }

bool Node::getHoldsObject() const noexcept { return holdsObject_; }

void Node::print(Terminal* term, std::string indent, bool last, bool root) const
{
  term->cprint(treeBranchesStyle, indent);

  if (root)
  {
    indent += "  ";
    term->cprintf(treeBranchesStyle, "  %s", boxSdlr);  // NOLINT
    term->newLine();
  }
  else
  {
    size_t lineSize = indent.size();

    if (last)
    {
      term->cprint(treeBranchesStyle, boxSur);  // NOLINT
      lineSize++;

      indent += "  ";
    }
    else
    {
      term->cprint(treeBranchesStyle, boxSudr);  // NOLINT
      lineSize++;

      indent += std::string(boxSud) + " ";  // NOLINT
    }

    term->cprint(treeBranchesStyle, boxSlr);  // NOLINT
    lineSize++;

    if (!children_.empty())
    {
      term->cprint(treeBranchesStyle, boxSdlr);  // NOLINT
      lineSize++;
    }
    else
    {
      term->cprint(treeBranchesStyle, boxSlr);  // NOLINT
      lineSize++;
    }

    term->cprint(style_, name_ + " ");
    lineSize += name_.size() + 1;

    if (!postFix_.empty())
    {
      term->cprint(descriptionStyle, postFix_ + " ");
      lineSize += postFix_.size() + 1;
    }

    uint32_t termW = 0U;
    uint32_t termH = 0U;
    term->getSize(termH, termW);

    if (!doc_.empty())
    {
      term->print(" ");
      lineSize++;

      size_t remainingSpace = termW - lineSize - 3;  // margin

      if (doc_.size() > remainingSpace)
      {
        term->cprint(inlineDocStyle, doc_.substr(0, remainingSpace - 3) + "...");
      }
      else
      {
        term->cprint(inlineDocStyle, std::string_view {doc_}.substr(0, remainingSpace));
      }
    }

    term->newLine();
  }

  for (auto itr = children_.begin(); itr != children_.end(); ++itr)
  {
    itr->print(term, indent, std::next(itr) == children_.end(), false);
  }
}

Node* Node::getOrCreateChild(std::vector<std::string> path)  // NOSONAR
{
  return getOrCreateChild(path, leafNodeStyle);  // NOLINT NOSONAR
}

Node* Node::getOrCreateChild(std::vector<std::string> path, Style style)  // NOLINT NOSONAR
{
  std::string curChildName = (*path.begin());
  path.erase(path.begin());

  for (auto& child: children_)
  {
    if (child.name_ == curChildName)
    {
      if (path.empty())
      {
        return &child;
      }
      return child.getOrCreateChild(path, style);  // NOLINT NOSONAR
    }
  }

  children_.emplace_back(this);
  children_.back().name_ = curChildName;

  if (path.empty())
  {
    children_.back().style_ = style;
    return &children_.back();
  }

  children_.back().postFix_ = "";
  return children_.back().getOrCreateChild(path, style);  // NOLINT NOSONAR
}

void Node::setPostFix(std::string&& value) noexcept { postFix_ = std::move(value); }

}  // namespace sen::components::shell
