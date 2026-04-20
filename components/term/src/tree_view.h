// === tree_view.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_TREE_VIEW_H
#define SEN_COMPONENTS_TERM_SRC_TREE_VIEW_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/span.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// std
#include <functional>
#include <list>
#include <string>
#include <string_view>
#include <vector>

namespace sen::components::term
{

/// A tree node for building hierarchical text output (like the `ls` command).
class TreeNode final
{
  SEN_NOCOPY_NOMOVE(TreeNode)

public:
  enum class Kind
  {
    plain,    /// No special meaning
    session,  /// Session node
    bus,      /// Bus node
    group,    /// Object group
    object,   /// Leaf object
    closed    /// Closed/unavailable source
  };

  explicit TreeNode(std::string name = {}, std::string annotation = {});

  /// Reset this node back to an empty state (drop children, annotation, and kind).
  void clear() noexcept;

  /// Get or create a child node along a path of names (e.g., {"session", "bus", "object"}).
  TreeNode* getOrCreateChild(Span<const std::string> path);

  /// Find a child node by path, returning nullptr if any segment is missing.
  [[nodiscard]] TreeNode* findChild(Span<const std::string> path);

  /// Set the annotation text shown after the name (e.g., "[Type, Remote]").
  void setAnnotation(std::string annotation);

  /// Get the current annotation.
  [[nodiscard]] std::string_view getAnnotation() const noexcept;

  /// Set the visual kind of this node (determines color).
  void setKind(Kind kind);

  /// Get the parent node (nullptr for root).
  [[nodiscard]] TreeNode* getParent() noexcept;

  /// Render the tree as a list of FTXUI elements.
  /// Each element is passed to the callback.
  void render(const std::function<void(ftxui::Element)>& emit) const;

private:
  void renderImpl(const std::function<void(ftxui::Element)>& emit, std::string_view prefix, bool isLast) const;

private:
  std::string name_;
  std::string annotation_;
  std::list<TreeNode> children_;
  TreeNode* parent_ = nullptr;
  Kind kind_ = Kind::plain;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_TREE_VIEW_H
