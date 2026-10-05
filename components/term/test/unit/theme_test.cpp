// === theme_test.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "theme.h"

// sen
#include "sen/core/meta/enum_type.h"
#include "stl/term.stl.h"

// ftxui
#include <ftxui/screen/color.hpp>

// google test
#include <gtest/gtest.h>

// std
#include <string>
#include <utility>
#include <vector>

namespace sen::components::term
{
namespace
{

/// The colours that tell one palette from another. Not every field, which would tie the check to
/// the struct's shape, but enough of them that two themes agreeing on all of these are the same
/// palette for any purpose a reader of the terminal has.
std::vector<ftxui::Color> signature(const Theme& theme)
{
  return {
    theme.accent,
    theme.success,
    theme.error,
    theme.info,
    theme.mutedText,
    theme.valueString,
    theme.valueNumber,
    theme.inputBackground,
    theme.inputForeground,
    theme.treeSession,
  };
}

/// @test
/// Every theme the STL declares is a palette of its own.
///
/// The styles come from the meta enum rather than a list written here, so a theme added to the STL
/// without a case in themeForStyle is caught: the switch falls through to the default, the new
/// style comes back with another theme's colours, and this fails. Written as a comparison between
/// themes rather than against fixed colours, because pinning the palettes would make every change
/// of taste a test failure.
TEST(Theme, EveryDeclaredStyleHasAPaletteOfItsOwn)
{
  const auto& enumType = *MetaTypeTrait<ThemeStyle>::meta();
  std::vector<std::pair<std::string, std::vector<ftxui::Color>>> seen;

  for (const auto& enumerator: enumType.getEnums())
  {
    auto colours = signature(themeForStyle(static_cast<ThemeStyle>(enumerator.key)));
    for (const auto& [name, other]: seen)
    {
      EXPECT_NE(colours, other) << enumerator.name << " and " << name << " are the same palette";
    }
    seen.emplace_back(std::string(enumerator.name), std::move(colours));
  }

  // The loop above passes vacuously over an empty enum, which would mean the meta type was not
  // the one this test thinks it is.
  EXPECT_GT(seen.size(), 1U) << "the ThemeStyle meta enum declares fewer styles than term offers";
}

/// @test
/// Setting the active theme replaces it, which is what a theme command has to do: a switch that
/// left one pane on the previous palette is the visible failure.
TEST(Theme, SettingTheActiveThemeReplacesEveryColour)
{
  const Theme original = activeTheme();

  setActiveTheme(themeForStyle(ThemeStyle::oneDark));
  const auto dark = signature(activeTheme());

  setActiveTheme(themeForStyle(ThemeStyle::solarizedLight));
  EXPECT_NE(signature(activeTheme()), dark);
  EXPECT_EQ(signature(activeTheme()), signature(themeForStyle(ThemeStyle::solarizedLight)));

  // The active theme outlives this test; the suite shares one process.
  setActiveTheme(original);
  EXPECT_EQ(signature(activeTheme()), signature(original));
}

}  // namespace
}  // namespace sen::components::term
