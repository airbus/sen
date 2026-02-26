// === terminal_tree.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_TERMINAL_TREE_H
#define SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_TERMINAL_TREE_H

#include "terminal.h"

// std
#include <list>

namespace sen::components::shell
{

class Node final
{
public:
  SEN_MOVE_ONLY(Node)

public:
  explicit Node(Node* parent);
  ~Node() = default;

public:
  void print(Terminal* term, std::string indent, bool last, bool root) const;
  [[nodiscard]] Node* getOrCreateChild(std::vector<std::string> path);
  [[nodiscard]] Node* getOrCreateChild(std::vector<std::string> path, Style style);
  [[nodiscard]] Node* getParent() noexcept;
  void setPostFix(std::string&& value) noexcept;
  void setStyle(Style style);
  void setHoldsObject();
  [[nodiscard]] bool getHoldsObject() const noexcept;

private:
  std::string name_;
  std::string doc_;
  std::string postFix_;
  std::list<Node> children_;
  Style style_ {};
  Node* parent_ = nullptr;
  bool holdsObject_ = false;
};

}  // namespace sen::components::shell

#endif  // SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_TERMINAL_TREE_H
