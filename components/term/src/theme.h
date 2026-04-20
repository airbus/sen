// === theme.h =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_THEME_H
#define SEN_COMPONENTS_TERM_SRC_THEME_H

// generated code
#include "stl/term.stl.h"

// ftxui
#include <ftxui/dom/elements.hpp>

namespace sen::components::term
{

struct Theme
{
  // Semantic colors
  ftxui::Color accent;     // focus indicators, event badge, spinner, prompts
  ftxui::Color success;    // echo success, log info
  ftxui::Color error;      // echo error, validation errors, log error
  ftxui::Color info;       // type names, completion paths, log debug
  ftxui::Color mutedText;  // de-emphasized labels, timestamps (theme-aware alt to ftxui::dim)

  // Completion
  ftxui::Color completionCommand;
  ftxui::Color completionObject;

  // Value formatting
  ftxui::Color valueString;
  ftxui::Color valueNumber;

  // Banner (decorative)
  ftxui::Color bannerQuote;
  ftxui::Color bannerAuthor;
  ftxui::Color bannerTitle;
  ftxui::Color bannerText;

  // Tree view
  ftxui::Color treeSession;
  ftxui::Color treeBus;
  ftxui::Color treeGroup;
  ftxui::Color treeObject;
  ftxui::Color treeClosed;
  ftxui::Color treePlain;
  ftxui::Color treeConnector;

  // Status bar
  ftxui::Color barBackground;
  ftxui::Color barForeground;
  ftxui::Color barAccent;

  // Input pane
  ftxui::Color inputBackground;
  ftxui::Color inputForeground;

  // Completion area
  ftxui::Color completionBackground;

  // Secondary pane background (watches, logs)
  ftxui::Color paneBackground;
  ftxui::Color paneForeground;
  ftxui::Color paneTitleBackground;
  ftxui::Color paneSeparator;

  // Log levels
  ftxui::Color logTrace;
  ftxui::Color logCriticalFg;
  ftxui::Color logCriticalBg;
};

/// Atom One Dark theme.
[[nodiscard]] Theme oneDarkTheme();

/// Atom One Light theme.
[[nodiscard]] Theme oneLightTheme();

/// Catppuccin Mocha theme (dark, warm).
[[nodiscard]] Theme catppuccinMochaTheme();

/// Catppuccin Latte theme (light, warm).
[[nodiscard]] Theme catppuccinLatteTheme();

/// Dracula theme (dark, purple).
[[nodiscard]] Theme draculaTheme();

/// Nord theme (dark, arctic).
[[nodiscard]] Theme nordTheme();

/// Gruvbox Dark theme (warm retro).
[[nodiscard]] Theme gruvboxDarkTheme();

/// Gruvbox Light theme (warm retro).
[[nodiscard]] Theme gruvboxLightTheme();

/// Tokyo Night theme (dark, blue-purple).
[[nodiscard]] Theme tokyoNightTheme();

/// Solarized Light theme (classic).
[[nodiscard]] Theme solarizedLightTheme();

/// Get the theme for a ThemeStyle enum value.
[[nodiscard]] Theme themeForStyle(ThemeStyle style);

/// Set the active theme. Call once during component init.
void setActiveTheme(const Theme& theme);

/// Get the active theme.
[[nodiscard]] const Theme& activeTheme();

}  // namespace sen::components::term

#endif
