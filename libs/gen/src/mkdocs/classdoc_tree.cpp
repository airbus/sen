// === classdoc_tree.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "mkdocs/classdoc_tree.h"

// sen
#include "sen/core/meta/class_type.h"

// std
#include <vector>

namespace sen::gen::detail
{

Node::Node(const sen::ClassType* meta, Node* parent, TypeStorage* storage)
  : meta_(meta), parent_(parent), storage_(storage)
{
}

Node* Node::getParent() noexcept { return parent_; }

const sen::ClassType* Node::getMeta() const noexcept { return meta_; }

std::vector<Node>& Node::getChildren() noexcept { return children_; }

TypeStorage* Node::getStorage() noexcept { return storage_; }

Node* Node::getOrCreateChild(std::vector<const sen::ClassType*> path, TypeStorage* storage)
{
  auto curChild = (*path.begin());
  path.erase(path.begin());

  for (auto& child: children_)
  {
    if (child.meta_ == curChild)
    {
      if (path.empty())
      {
        return &child;
      }
      return child.getOrCreateChild(path, storage);
    }
  }

  children_.emplace_back(curChild, this, storage);

  if (path.empty())
  {
    return &children_.back();
  }

  return children_.back().getOrCreateChild(path, storage);
}

}  // namespace sen::gen::detail
