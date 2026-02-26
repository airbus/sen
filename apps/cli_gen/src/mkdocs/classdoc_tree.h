// === classdoc_tree.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_MKDOCS_CLASSDOC_TREE_H
#define SEN_APPS_CLI_GEN_SRC_MKDOCS_CLASSDOC_TREE_H

// std
#include <vector>

// sen
#include "sen/core/meta/class_type.h"

// forward declarations
class JsonTypeStorage;

class Node final
{
public:
  SEN_MOVE_ONLY(Node)

public:
  Node(const sen::ClassType* meta, Node* parent, JsonTypeStorage* storage);
  ~Node() = default;

public:
  [[nodiscard]] Node* getOrCreateChild(std::vector<const sen::ClassType*> path, JsonTypeStorage* storage);
  [[nodiscard]] Node* getParent() noexcept;
  [[nodiscard]] const sen::ClassType* getMeta() const noexcept;
  [[nodiscard]] std::vector<Node>& getChildren() noexcept;
  [[nodiscard]] JsonTypeStorage* getStorage() noexcept;

private:
  std::vector<Node> children_;
  const sen::ClassType* meta_;
  Node* parent_ = nullptr;
  JsonTypeStorage* storage_;
};

#endif  // SEN_APPS_CLI_GEN_SRC_MKDOCS_CLASSDOC_TREE_H
