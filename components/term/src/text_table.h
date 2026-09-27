// === text_table.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_TEXT_TABLE_H
#define SEN_COMPONENTS_TERM_SRC_TEXT_TABLE_H

// ftxui
// ftxui
#include <ftxui/dom/elements.hpp>

// std
#include <string>
#include <vector>

namespace sen::components::term::text_table
{

/// One cell in a text table. The width field records the display width of the
/// underlying text so the renderer can align columns without having to peek
/// inside the (potentially composite) element.
class Cell
{
public:
  /// Plain text cell.
  explicit Cell(std::string text);
  /// Text cell with an FTXUI decorator applied (e.g. `ftxui::bold`).
  Cell(std::string text, ftxui::Decorator decorator);
  /// Custom element with an explicit width source (used for composite cells
  /// such as syntax-highlighted content, where `widthSource` is the plain-text
  /// form used only for column alignment).
  Cell(std::string widthSource, ftxui::Element custom);

  /// The element to draw. An Element is a shared pointer, so this is a cheap copy.
  [[nodiscard]] ftxui::Element element() const { return element_; }

  /// Display width of the cell's text, for column alignment.
  [[nodiscard]] int width() const noexcept { return width_; }

private:
  ftxui::Element element_;
  int width_;
};

using Row = std::vector<Cell>;

/// Drop-in replacement for `ftxui::Table` that renders as nested vbox/hbox so
/// FTXUI's selection machinery (which walks the default `Node::children_` and
/// doesn't visit `gridbox` cells) can reach the cell text. Column widths are
/// computed as the max of `Cell::width` in each column.
/// An empty row renders as a horizontal separator spanning the table width.
ftxui::Element render(std::vector<Row> rows, int columnGap = 2);

}  // namespace sen::components::term::text_table

#endif  // SEN_COMPONENTS_TERM_SRC_TEXT_TABLE_H
