// === text_table.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "text_table.h"

// sen
#include "sen/core/base/checked_conversions.h"

// ftxui
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/string.hpp>

// std
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace sen::components::term::textTable
{

using sen::std_util::checkedConversion;

Cell::Cell(std::string text): element(ftxui::text(text)), width(ftxui::string_width(text)) {}

Cell::Cell(std::string text, ftxui::Decorator decorator)
  : element(ftxui::text(text) | std::move(decorator)), width(ftxui::string_width(text))
{
}

Cell::Cell(std::string widthSource, ftxui::Element custom)
  : element(std::move(custom)), width(ftxui::string_width(widthSource))
{
}

ftxui::Element render(std::vector<Row> rows, int columnGap)
{
  if (rows.empty())
  {
    return ftxui::vbox({});
  }

  std::size_t cols = 0;
  for (const auto& r: rows)
  {
    cols = std::max(cols, r.size());
  }

  std::vector<int> colWidths(cols, 0);
  for (const auto& r: rows)
  {
    for (std::size_t c = 0; c < r.size(); ++c)
    {
      colWidths[c] = std::max(colWidths[c], r[c].width);
    }
  }

  const std::string gap(checkedConversion<std::size_t>(columnGap), ' ');

  ftxui::Elements rowElements;
  rowElements.reserve(rows.size());
  for (auto& r: rows)
  {
    if (r.empty())
    {
      rowElements.push_back(ftxui::separator());
      continue;
    }
    ftxui::Elements cells;
    cells.reserve(r.size() * 3U);
    for (std::size_t c = 0; c < r.size(); ++c)
    {
      cells.push_back(std::move(r[c].element));
      const int pad = colWidths[c] - r[c].width;
      if (pad > 0)
      {
        cells.push_back(ftxui::text(std::string(checkedConversion<std::size_t>(pad), ' ')));
      }
      if (c + 1U < r.size())
      {
        cells.push_back(ftxui::text(gap));
      }
    }
    rowElements.push_back(ftxui::hbox(std::move(cells)));
  }
  return ftxui::vbox(std::move(rowElements));
}

}  // namespace sen::components::term::textTable
