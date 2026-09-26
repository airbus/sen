// === theme.cpp =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "theme.h"

namespace sen::components::term
{

//--------------------------------------------------------------------------------------------------------------
// Built-in themes
//--------------------------------------------------------------------------------------------------------------

namespace
{

/// The colours a theme actually chooses. Every Theme field takes its value from one of these, and
/// the grouping is not a simplification of the palettes -- it was derived from them: each group
/// below held the identical colour in all ten themes, so this reproduces every one of them exactly.
struct Palette
{
  ftxui::Color accent;
  ftxui::Color background;
  ftxui::Color command;
  ftxui::Color completionBackground;
  ftxui::Color error;
  ftxui::Color foreground;
  ftxui::Color info;
  ftxui::Color muted;
  ftxui::Color string;
  ftxui::Color success;
};

/// Spread a palette across the Theme fields. The grouping is the interesting part and it lives here
/// once, rather than being restated in ten near-identical tables.
Theme themeFromPalette(const Palette& p)
{
  Theme t;
  t.accent = p.accent;
  t.treeSession = p.accent;

  t.inputBackground = p.background;
  t.logCriticalFg = p.background;

  t.completionCommand = p.command;
  t.treeGroup = p.command;

  t.completionBackground = p.completionBackground;

  t.error = p.error;
  t.logCriticalBg = p.error;

  t.bannerTitle = p.foreground;
  t.inputForeground = p.foreground;
  t.treePlain = p.foreground;

  t.info = p.info;
  t.treeObject = p.info;

  t.bannerText = p.muted;
  t.logTrace = p.muted;
  t.mutedText = p.muted;
  t.treeConnector = p.muted;

  t.bannerQuote = p.string;
  t.valueString = p.string;

  t.bannerAuthor = p.success;
  t.completionObject = p.success;
  t.success = p.success;
  t.treeBus = p.success;
  t.valueNumber = p.success;
  return t;
}

}  // namespace

/// Atom One Dark.
Theme oneDarkTheme()
{
  using C = ftxui::Color;
  return themeFromPalette({
    /* accent               */ C::RGB(229, 192, 123),  // Yellow  #e5c07b
    /* background           */ C::RGB(40, 44, 52),     // Bg       #282c34
    /* command              */ C::RGB(97, 175, 239),   // Blue   #61afef
    /* completionBackground */ C::RGB(36, 40, 47),     // Between Bg and Gutter  #24282f
    /* error                */ C::RGB(224, 108, 117),  // Red     #e06c75
    /* foreground           */ C::RGB(171, 178, 191),  // Fg       #abb2bf
    /* info                 */ C::RGB(86, 182, 194),   // Cyan    #56b6c2
    /* muted                */ C::RGB(92, 99, 112),    // Comment  #5c6370
    /* string               */ C::RGB(209, 154, 102),  // Orange   #d19a66
    /* success              */ C::RGB(152, 195, 121),  // Green    #98c379
  });
}

/// Atom One Light.
Theme oneLightTheme()
{
  using C = ftxui::Color;
  return themeFromPalette({
    /* accent               */ C::RGB(193, 132, 1),    // Yellow   #c18401
    /* background           */ C::RGB(250, 250, 250),  // Bg       #fafafa
    /* command              */ C::RGB(64, 120, 242),   // Blue   #4078f2
    /* completionBackground */ C::RGB(230, 230, 232),  // Step3    #e6e6e8
    /* error                */ C::RGB(228, 86, 73),    // Red      #e45649
    /* foreground           */ C::RGB(56, 58, 66),     // Fg       #383a42
    /* info                 */ C::RGB(1, 132, 188),    // Cyan     #0184bc
    /* muted                */ C::RGB(142, 143, 150),  // Comment (darkened)  #8e8f96
    /* string               */ C::RGB(193, 132, 1),    // Yellow   #c18401
    /* success              */ C::RGB(80, 161, 79),    // Green    #50a14f
  });
}

/// Catppuccin Mocha.
Theme catppuccinMochaTheme()
{
  using C = ftxui::Color;
  return themeFromPalette({
    /* accent               */ C::RGB(249, 226, 175),  // Yellow    #f9e2af
    /* background           */ C::RGB(30, 30, 46),     // Base      #1e1e2e
    /* command              */ C::RGB(137, 180, 250),  // Blue   #89b4fa
    /* completionBackground */ C::RGB(30, 30, 46),     // Base      #1e1e2e
    /* error                */ C::RGB(243, 139, 168),  // Red       #f38ba8
    /* foreground           */ C::RGB(205, 214, 244),  // Text      #cdd6f4
    /* info                 */ C::RGB(148, 226, 213),  // Teal      #94e2d5
    /* muted                */ C::RGB(108, 112, 134),  // Overlay0  #6c7086
    /* string               */ C::RGB(250, 179, 135),  // Peach     #fab387
    /* success              */ C::RGB(166, 227, 161),  // Green     #a6e3a1
  });
}

/// Catppuccin Latte.
Theme catppuccinLatteTheme()
{
  using C = ftxui::Color;
  return themeFromPalette({
    /* accent               */ C::RGB(223, 142, 29),   // Yellow   #df8e1d
    /* background           */ C::RGB(239, 241, 245),  // Base      #eff1f5
    /* command              */ C::RGB(30, 102, 245),   // Blue   #1e66f5
    /* completionBackground */ C::RGB(220, 224, 232),  // Between Base and Surface0  #dce0e8
    /* error                */ C::RGB(210, 15, 57),    // Red      #d20f39
    /* foreground           */ C::RGB(76, 79, 105),    // Text      #4c4f69
    /* info                 */ C::RGB(23, 146, 153),   // Teal     #179299
    /* muted                */ C::RGB(140, 143, 161),  // Overlay1  #8c8fa1
    /* string               */ C::RGB(254, 100, 11),   // Peach     #fe640b
    /* success              */ C::RGB(64, 160, 43),    // Green     #40a02b
  });
}

/// Dracula.
Theme draculaTheme()
{
  using C = ftxui::Color;
  return themeFromPalette({
    /* accent               */ C::RGB(241, 250, 140),  // Yellow    #f1fa8c
    /* background           */ C::RGB(40, 42, 54),     // Bg           #282a36
    /* command              */ C::RGB(189, 147, 249),  // Purple  #bd93f9
    /* completionBackground */ C::RGB(40, 42, 54),     // Bg          #282a36
    /* error                */ C::RGB(255, 85, 85),    // Red       #ff5555
    /* foreground           */ C::RGB(248, 248, 242),  // Fg       #f8f8f2
    /* info                 */ C::RGB(139, 233, 253),  // Cyan      #8be9fd
    /* muted                */ C::RGB(98, 114, 164),   // Comment  #6272a4
    /* string               */ C::RGB(255, 184, 108),  // Orange   #ffb86c
    /* success              */ C::RGB(80, 250, 123),   // Green    #50fa7b
  });
}

/// Nord.
Theme nordTheme()
{
  using C = ftxui::Color;
  return themeFromPalette({
    /* accent               */ C::RGB(235, 203, 139),  // Yellow    #ebcb8b
    /* background           */ C::RGB(46, 52, 64),     // Polar0   #2e3440
    /* command              */ C::RGB(129, 161, 193),  // Frost   #81a1c1
    /* completionBackground */ C::RGB(46, 52, 64),     // Polar0   #2e3440
    /* error                */ C::RGB(191, 97, 106),   // Red       #bf616a
    /* foreground           */ C::RGB(216, 222, 233),  // Snow     #d8dee9
    /* info                 */ C::RGB(136, 192, 208),  // Frost     #88c0d0
    /* muted                */ C::RGB(76, 86, 106),    // Polar    #4c566a
    /* string               */ C::RGB(208, 135, 112),  // Orange   #d08770
    /* success              */ C::RGB(163, 190, 140),  // Green    #a3be8c
  });
}

/// Gruvbox Dark.
Theme gruvboxDarkTheme()
{
  using C = ftxui::Color;
  return themeFromPalette({
    /* accent               */ C::RGB(250, 189, 47),   // Yellow   #fabd2f
    /* background           */ C::RGB(40, 40, 40),     // Bg       #282828
    /* command              */ C::RGB(131, 165, 152),  // Blue    #83a598
    /* completionBackground */ C::RGB(50, 48, 47),     // Bg0h     #32302f
    /* error                */ C::RGB(251, 73, 52),    // Red      #fb4934
    /* foreground           */ C::RGB(235, 219, 178),  // Fg       #ebdbb2
    /* info                 */ C::RGB(142, 192, 124),  // Aqua     #8ec07c
    /* muted                */ C::RGB(168, 153, 132),  // Gray     #a89984
    /* string               */ C::RGB(254, 128, 25),   // Orange   #fe8019
    /* success              */ C::RGB(184, 187, 38),   // Green    #b8bb26
  });
}

/// Gruvbox Light.
Theme gruvboxLightTheme()
{
  using C = ftxui::Color;
  return themeFromPalette({
    /* accent               */ C::RGB(181, 118, 20),   // Yellow   #b57614
    /* background           */ C::RGB(251, 241, 199),  // Bg       #fbf1c7
    /* command              */ C::RGB(7, 102, 120),    // Blue    #076678
    /* completionBackground */ C::RGB(242, 229, 188),  // Bg0      #f2e5bc
    /* error                */ C::RGB(157, 0, 6),      // Red      #9d0006
    /* foreground           */ C::RGB(60, 56, 54),     // Fg       #3c3836
    /* info                 */ C::RGB(66, 123, 88),    // Aqua     #427b58
    /* muted                */ C::RGB(124, 111, 100),  // Gray     #7c6f64
    /* string               */ C::RGB(175, 58, 3),     // Orange   #af3a03
    /* success              */ C::RGB(121, 116, 14),   // Green    #79740e
  });
}

/// Tokyo Night.
Theme tokyoNightTheme()
{
  using C = ftxui::Color;
  return themeFromPalette({
    /* accent               */ C::RGB(224, 175, 104),  // Yellow    #e0af68
    /* background           */ C::RGB(26, 27, 38),     // Bg        #1a1b26
    /* command              */ C::RGB(122, 162, 247),  // Blue     #7aa2f7
    /* completionBackground */ C::RGB(26, 27, 38),     // BgDark    #1a1b26
    /* error                */ C::RGB(247, 118, 142),  // Red       #f7768e
    /* foreground           */ C::RGB(169, 177, 214),  // Fg        #a9b1d6
    /* info                 */ C::RGB(115, 218, 202),  // Teal      #73daca
    /* muted                */ C::RGB(86, 95, 137),    // Comment   #565f89
    /* string               */ C::RGB(255, 158, 100),  // Orange    #ff9e64
    /* success              */ C::RGB(158, 206, 106),  // Green     #9ece6a
  });
}

/// Solarized Light.
Theme solarizedLightTheme()
{
  using C = ftxui::Color;
  return themeFromPalette({
    /* accent               */ C::RGB(181, 137, 0),    // Yellow   #b58900
    /* background           */ C::RGB(253, 246, 227),  // Base3    #fdf6e3
    /* command              */ C::RGB(38, 139, 210),   // Blue     #268bd2
    /* completionBackground */ C::RGB(245, 239, 220),  // Between Base3 and Base2  #f5efdc
    /* error                */ C::RGB(220, 50, 47),    // Red      #dc322f
    /* foreground           */ C::RGB(88, 110, 117),   // Base01   #586e75
    /* info                 */ C::RGB(42, 161, 152),   // Cyan     #2aa198
    /* muted                */ C::RGB(101, 123, 131),  // Base00   #657b83
    /* string               */ C::RGB(203, 75, 22),    // Orange   #cb4b16
    /* success              */ C::RGB(133, 153, 0),    // Green    #859900
  });
}

namespace
{

/// The one live theme. A function-local static so the order of static initialisation cannot matter.
Theme& mutableActiveTheme()
{
  static Theme instance = oneDarkTheme();
  return instance;
}

}  // namespace

Theme themeForStyle(ThemeStyle style)
{
  switch (style)
  {
    case ThemeStyle::oneDark:
      return oneDarkTheme();
    case ThemeStyle::oneLight:
      return oneLightTheme();
    case ThemeStyle::catppuccinMocha:
      return catppuccinMochaTheme();
    case ThemeStyle::catppuccinLatte:
      return catppuccinLatteTheme();
    case ThemeStyle::dracula:
      return draculaTheme();
    case ThemeStyle::nord:
      return nordTheme();
    case ThemeStyle::gruvboxDark:
      return gruvboxDarkTheme();
    case ThemeStyle::gruvboxLight:
      return gruvboxLightTheme();
    case ThemeStyle::tokyoNight:
      return tokyoNightTheme();
    case ThemeStyle::solarizedLight:
      return solarizedLightTheme();
  }
  return oneDarkTheme();
}

void setActiveTheme(const Theme& theme) { mutableActiveTheme() = theme; }

const Theme& activeTheme() { return mutableActiveTheme(); }

}  // namespace sen::components::term
