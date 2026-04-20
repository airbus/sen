// === test_render_utils.h =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_TEST_UNIT_TEST_RENDER_UTILS_H
#define SEN_COMPONENTS_TERM_TEST_UNIT_TEST_RENDER_UTILS_H

// ftxui
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/screen.hpp>

// google test
#include <gmock/gmock.h>

// std
#include <regex>
#include <string>

namespace sen::components::term::test
{

/// Strip ANSI escape codes from a rendered screen string so tests can assert on plain text.
inline std::string stripAnsi(const std::string& s)
{
  static const std::regex ansiRegex("\x1B\\[[0-9;]*[a-zA-Z]");
  return std::regex_replace(s, ansiRegex, "");
}

/// Render an FTXUI element to a fixed-size screen and return the plain text.
inline std::string renderToText(ftxui::Element element, int width = 100, int height = 20)
{
  auto screen = ftxui::Screen::Create(ftxui::Dimension::Fixed(width), ftxui::Dimension::Fixed(height));
  ftxui::Render(screen, element);
  return stripAnsi(screen.ToString());
}

}  // namespace sen::components::term::test

#endif
