// === classdoc_tree.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_GEN_SRC_MKDOCS_CLASSDOC_TREE_H
#define SEN_LIBS_GEN_SRC_MKDOCS_CLASSDOC_TREE_H

// sen
#include "sen/core/meta/class_type.h"

// std
#include <vector>

namespace sen::gen::detail
{

class TypeStorage;

class Node final
{
public:
  SEN_MOVE_ONLY(Node)

public:
  Node(const sen::ClassType* meta, Node* parent, TypeStorage* storage);
  ~Node() = default;

public:
  [[nodiscard]] Node* getOrCreateChild(std::vector<const sen::ClassType*> path, TypeStorage* storage);
  [[nodiscard]] Node* getParent() noexcept;
  [[nodiscard]] const sen::ClassType* getMeta() const noexcept;
  [[nodiscard]] std::vector<Node>& getChildren() noexcept;
  [[nodiscard]] TypeStorage* getStorage() noexcept;

private:
  std::vector<Node> children_;
  const sen::ClassType* meta_;
  Node* parent_ = nullptr;
  TypeStorage* storage_;
};

}  // namespace sen::gen::detail

#endif  // SEN_LIBS_GEN_SRC_MKDOCS_CLASSDOC_TREE_H
