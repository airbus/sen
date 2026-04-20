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

Theme oneDarkTheme()
{
  using C = ftxui::Color;

  Theme t;

  // Semantic colors
  t.accent = C::RGB(229, 192, 123);   // Yellow  #e5c07b
  t.success = C::RGB(152, 195, 121);  // Green   #98c379
  t.error = C::RGB(224, 108, 117);    // Red     #e06c75
  t.info = C::RGB(86, 182, 194);      // Cyan    #56b6c2
  t.mutedText = C::RGB(92, 99, 112);  // Comment #5c6370

  // Completion
  t.completionCommand = C::RGB(97, 175, 239);  // Blue   #61afef
  t.completionObject = C::RGB(152, 195, 121);  // Green  #98c379

  // Value formatting
  t.valueString = C::RGB(209, 154, 102);  // Orange  #d19a66
  t.valueNumber = C::RGB(152, 195, 121);  // Green   #98c379

  // Banner (decorative)
  t.bannerQuote = C::RGB(209, 154, 102);   // Orange   #d19a66
  t.bannerAuthor = C::RGB(152, 195, 121);  // Green    #98c379
  t.bannerTitle = C::RGB(171, 178, 191);   // Fg       #abb2bf
  t.bannerText = C::RGB(92, 99, 112);      // Comment  #5c6370

  // Tree view
  t.treeSession = C::RGB(229, 192, 123);  // Yellow   #e5c07b
  t.treeBus = C::RGB(152, 195, 121);      // Green    #98c379
  t.treeGroup = C::RGB(97, 175, 239);     // Blue     #61afef
  t.treeObject = C::RGB(86, 182, 194);    // Cyan     #56b6c2
  t.treeClosed = C::RGB(92, 99, 112);     // Comment  #5c6370
  t.treePlain = C::RGB(171, 178, 191);    // Fg       #abb2bf
  t.treeConnector = C::RGB(92, 99, 112);  // Comment  #5c6370

  // Status bar
  t.barBackground = C::RGB(44, 49, 58);     // Gutter   #2c313a
  t.barForeground = C::RGB(171, 178, 191);  // Fg       #abb2bf
  t.barAccent = C::RGB(97, 175, 239);       // Blue     #61afef

  // Input pane
  t.inputBackground = C::RGB(40, 44, 52);       // Bg       #282c34
  t.inputForeground = C::RGB(171, 178, 191);    // Fg       #abb2bf
  t.completionBackground = C::RGB(36, 40, 47);  // Between Bg and Gutter  #24282f

  // Secondary panes (watches, logs)
  t.paneBackground = C::RGB(33, 37, 43);       // Darker   #21252b
  t.paneForeground = C::RGB(171, 178, 191);    // Fg       #abb2bf
  t.paneTitleBackground = C::RGB(44, 49, 58);  // Gutter   #2c313a
  t.paneSeparator = C::RGB(55, 62, 73);        // Edge     #373e49

  // Log levels
  t.logTrace = C::RGB(92, 99, 112);         // Comment  #5c6370
  t.logCriticalFg = C::RGB(40, 44, 52);     // Bg       #282c34
  t.logCriticalBg = C::RGB(224, 108, 117);  // Red      #e06c75

  return t;
}

Theme oneLightTheme()
{
  using C = ftxui::Color;

  Theme t;

  // Semantic colors
  t.accent = C::RGB(193, 132, 1);       // Yellow   #c18401
  t.success = C::RGB(80, 161, 79);      // Green    #50a14f
  t.error = C::RGB(228, 86, 73);        // Red      #e45649
  t.info = C::RGB(1, 132, 188);         // Cyan     #0184bc
  t.mutedText = C::RGB(142, 143, 150);  // Comment (darkened)  #8e8f96

  // Completion
  t.completionCommand = C::RGB(64, 120, 242);  // Blue   #4078f2
  t.completionObject = C::RGB(80, 161, 79);    // Green  #50a14f

  // Value formatting
  t.valueString = C::RGB(193, 132, 1);  // Yellow  #c18401
  t.valueNumber = C::RGB(80, 161, 79);  // Green   #50a14f

  // Banner (decorative)
  t.bannerQuote = C::RGB(193, 132, 1);   // Yellow   #c18401
  t.bannerAuthor = C::RGB(80, 161, 79);  // Green    #50a14f
  t.bannerTitle = C::RGB(56, 58, 66);    // Fg       #383a42
  t.bannerText = C::RGB(142, 143, 150);  // Comment (darkened)  #8e8f96

  // Tree view
  t.treeSession = C::RGB(193, 132, 1);      // Yellow   #c18401
  t.treeBus = C::RGB(80, 161, 79);          // Green    #50a14f
  t.treeGroup = C::RGB(64, 120, 242);       // Blue     #4078f2
  t.treeObject = C::RGB(1, 132, 188);       // Cyan     #0184bc
  t.treeClosed = C::RGB(142, 143, 150);     // Comment (darkened)  #8e8f96
  t.treePlain = C::RGB(56, 58, 66);         // Fg       #383a42
  t.treeConnector = C::RGB(142, 143, 150);  // Comment (darkened)  #8e8f96

  // Status bar
  t.barBackground = C::RGB(215, 216, 220);  // Darker UI  #d7d8dc
  t.barForeground = C::RGB(56, 58, 66);     // Fg       #383a42
  t.barAccent = C::RGB(64, 120, 242);       // Blue     #4078f2

  // Input pane
  t.inputBackground = C::RGB(250, 250, 250);       // Bg       #fafafa
  t.inputForeground = C::RGB(56, 58, 66);          // Fg       #383a42
  t.completionBackground = C::RGB(230, 230, 232);  // Step3    #e6e6e8

  // Secondary panes (watches, logs)
  t.paneBackground = C::RGB(240, 240, 241);       // Step2    #f0f0f1
  t.paneForeground = C::RGB(56, 58, 66);          // Fg       #383a42
  t.paneTitleBackground = C::RGB(215, 216, 220);  // Step4    #d7d8dc
  t.paneSeparator = C::RGB(195, 196, 200);        // Step5    #c3c4c8

  // Log levels
  t.logTrace = C::RGB(142, 143, 150);       // Comment (darkened)  #8e8f96
  t.logCriticalFg = C::RGB(250, 250, 250);  // Bg       #fafafa
  t.logCriticalBg = C::RGB(228, 86, 73);    // Red      #e45649

  return t;
}

Theme catppuccinMochaTheme()
{
  using C = ftxui::Color;

  Theme t;

  // Semantic colors
  t.accent = C::RGB(249, 226, 175);     // Yellow    #f9e2af
  t.success = C::RGB(166, 227, 161);    // Green     #a6e3a1
  t.error = C::RGB(243, 139, 168);      // Red       #f38ba8
  t.info = C::RGB(148, 226, 213);       // Teal      #94e2d5
  t.mutedText = C::RGB(108, 112, 134);  // Overlay0  #6c7086

  // Completion
  t.completionCommand = C::RGB(137, 180, 250);  // Blue   #89b4fa
  t.completionObject = C::RGB(166, 227, 161);   // Green  #a6e3a1

  // Value formatting
  t.valueString = C::RGB(250, 179, 135);  // Peach   #fab387
  t.valueNumber = C::RGB(166, 227, 161);  // Green   #a6e3a1

  // Banner (decorative)
  t.bannerQuote = C::RGB(250, 179, 135);   // Peach     #fab387
  t.bannerAuthor = C::RGB(166, 227, 161);  // Green     #a6e3a1
  t.bannerTitle = C::RGB(205, 214, 244);   // Text      #cdd6f4
  t.bannerText = C::RGB(108, 112, 134);    // Overlay0  #6c7086

  // Tree view
  t.treeSession = C::RGB(249, 226, 175);    // Yellow    #f9e2af
  t.treeBus = C::RGB(166, 227, 161);        // Green     #a6e3a1
  t.treeGroup = C::RGB(137, 180, 250);      // Blue      #89b4fa
  t.treeObject = C::RGB(148, 226, 213);     // Teal      #94e2d5
  t.treeClosed = C::RGB(108, 112, 134);     // Overlay0  #6c7086
  t.treePlain = C::RGB(205, 214, 244);      // Text      #cdd6f4
  t.treeConnector = C::RGB(108, 112, 134);  // Overlay0  #6c7086

  // Status bar
  t.barBackground = C::RGB(49, 50, 68);     // Surface0  #313244
  t.barForeground = C::RGB(205, 214, 244);  // Text      #cdd6f4
  t.barAccent = C::RGB(137, 180, 250);      // Blue      #89b4fa

  // Input pane
  t.inputBackground = C::RGB(30, 30, 46);       // Base      #1e1e2e
  t.inputForeground = C::RGB(205, 214, 244);    // Text      #cdd6f4
  t.completionBackground = C::RGB(30, 30, 46);  // Base      #1e1e2e

  // Secondary panes (watches, logs)
  t.paneBackground = C::RGB(24, 24, 37);       // Mantle    #181825
  t.paneForeground = C::RGB(205, 214, 244);    // Text      #cdd6f4
  t.paneTitleBackground = C::RGB(49, 50, 68);  // Surface0  #313244
  t.paneSeparator = C::RGB(69, 71, 90);        // Surface1  #45475a

  // Log levels
  t.logTrace = C::RGB(108, 112, 134);       // Overlay0  #6c7086
  t.logCriticalFg = C::RGB(30, 30, 46);     // Base      #1e1e2e
  t.logCriticalBg = C::RGB(243, 139, 168);  // Red       #f38ba8

  return t;
}

Theme catppuccinLatteTheme()
{
  using C = ftxui::Color;

  Theme t;

  // Semantic colors
  t.accent = C::RGB(223, 142, 29);      // Yellow   #df8e1d
  t.success = C::RGB(64, 160, 43);      // Green    #40a02b
  t.error = C::RGB(210, 15, 57);        // Red      #d20f39
  t.info = C::RGB(23, 146, 153);        // Teal     #179299
  t.mutedText = C::RGB(140, 143, 161);  // Overlay1  #8c8fa1

  // Completion
  t.completionCommand = C::RGB(30, 102, 245);  // Blue   #1e66f5
  t.completionObject = C::RGB(64, 160, 43);    // Green  #40a02b

  // Value formatting
  t.valueString = C::RGB(254, 100, 11);  // Peach   #fe640b
  t.valueNumber = C::RGB(64, 160, 43);   // Green   #40a02b

  // Banner (decorative)
  t.bannerQuote = C::RGB(254, 100, 11);  // Peach     #fe640b
  t.bannerAuthor = C::RGB(64, 160, 43);  // Green     #40a02b
  t.bannerTitle = C::RGB(76, 79, 105);   // Text      #4c4f69
  t.bannerText = C::RGB(140, 143, 161);  // Overlay1  #8c8fa1

  // Tree view
  t.treeSession = C::RGB(223, 142, 29);     // Yellow    #df8e1d
  t.treeBus = C::RGB(64, 160, 43);          // Green     #40a02b
  t.treeGroup = C::RGB(30, 102, 245);       // Blue      #1e66f5
  t.treeObject = C::RGB(23, 146, 153);      // Teal      #179299
  t.treeClosed = C::RGB(140, 143, 161);     // Overlay1  #8c8fa1
  t.treePlain = C::RGB(76, 79, 105);        // Text      #4c4f69
  t.treeConnector = C::RGB(140, 143, 161);  // Overlay1  #8c8fa1

  // Status bar
  t.barBackground = C::RGB(204, 208, 218);  // Surface0  #ccd0da
  t.barForeground = C::RGB(76, 79, 105);    // Text      #4c4f69
  t.barAccent = C::RGB(30, 102, 245);       // Blue      #1e66f5

  // Input pane
  t.inputBackground = C::RGB(239, 241, 245);       // Base      #eff1f5
  t.inputForeground = C::RGB(76, 79, 105);         // Text      #4c4f69
  t.completionBackground = C::RGB(220, 224, 232);  // Between Base and Surface0  #dce0e8

  // Secondary panes (watches, logs)
  t.paneBackground = C::RGB(230, 233, 239);       // Mantle    #e6e9ef
  t.paneForeground = C::RGB(76, 79, 105);         // Text      #4c4f69
  t.paneTitleBackground = C::RGB(204, 208, 218);  // Surface0  #ccd0da
  t.paneSeparator = C::RGB(188, 192, 204);        // Surface1  #bcc0cc

  // Log levels
  t.logTrace = C::RGB(140, 143, 161);       // Overlay1  #8c8fa1
  t.logCriticalFg = C::RGB(239, 241, 245);  // Base      #eff1f5
  t.logCriticalBg = C::RGB(210, 15, 57);    // Red       #d20f39

  return t;
}

Theme draculaTheme()
{
  using C = ftxui::Color;

  Theme t;

  // Semantic colors
  t.accent = C::RGB(241, 250, 140);    // Yellow    #f1fa8c
  t.success = C::RGB(80, 250, 123);    // Green     #50fa7b
  t.error = C::RGB(255, 85, 85);       // Red       #ff5555
  t.info = C::RGB(139, 233, 253);      // Cyan      #8be9fd
  t.mutedText = C::RGB(98, 114, 164);  // Comment   #6272a4

  // Completion
  t.completionCommand = C::RGB(189, 147, 249);  // Purple  #bd93f9
  t.completionObject = C::RGB(80, 250, 123);    // Green   #50fa7b

  // Value formatting
  t.valueString = C::RGB(255, 184, 108);  // Orange  #ffb86c
  t.valueNumber = C::RGB(80, 250, 123);   // Green   #50fa7b

  // Banner (decorative)
  t.bannerQuote = C::RGB(255, 184, 108);  // Orange   #ffb86c
  t.bannerAuthor = C::RGB(80, 250, 123);  // Green    #50fa7b
  t.bannerTitle = C::RGB(248, 248, 242);  // Fg       #f8f8f2
  t.bannerText = C::RGB(98, 114, 164);    // Comment  #6272a4

  // Tree view
  t.treeSession = C::RGB(241, 250, 140);   // Yellow   #f1fa8c
  t.treeBus = C::RGB(80, 250, 123);        // Green    #50fa7b
  t.treeGroup = C::RGB(189, 147, 249);     // Purple   #bd93f9
  t.treeObject = C::RGB(139, 233, 253);    // Cyan     #8be9fd
  t.treeClosed = C::RGB(98, 114, 164);     // Comment  #6272a4
  t.treePlain = C::RGB(248, 248, 242);     // Fg       #f8f8f2
  t.treeConnector = C::RGB(98, 114, 164);  // Comment  #6272a4

  // Status bar
  t.barBackground = C::RGB(68, 71, 90);     // CurrentLine  #44475a
  t.barForeground = C::RGB(248, 248, 242);  // Fg           #f8f8f2
  t.barAccent = C::RGB(255, 121, 198);      // Pink         #ff79c6

  // Input pane
  t.inputBackground = C::RGB(40, 42, 54);       // Bg           #282a36
  t.inputForeground = C::RGB(248, 248, 242);    // Fg           #f8f8f2
  t.completionBackground = C::RGB(40, 42, 54);  // Bg          #282a36

  // Secondary panes (watches, logs)
  t.paneBackground = C::RGB(33, 34, 44);       // Darker       #21222c
  t.paneForeground = C::RGB(248, 248, 242);    // Fg           #f8f8f2
  t.paneTitleBackground = C::RGB(68, 71, 90);  // CurrentLine  #44475a
  t.paneSeparator = C::RGB(98, 114, 164);      // Comment      #6272a4

  // Log levels
  t.logTrace = C::RGB(98, 114, 164);      // Comment  #6272a4
  t.logCriticalFg = C::RGB(40, 42, 54);   // Bg       #282a36
  t.logCriticalBg = C::RGB(255, 85, 85);  // Red      #ff5555

  return t;
}

Theme nordTheme()
{
  using C = ftxui::Color;

  Theme t;

  // Semantic colors
  t.accent = C::RGB(235, 203, 139);   // Yellow    #ebcb8b
  t.success = C::RGB(163, 190, 140);  // Green     #a3be8c
  t.error = C::RGB(191, 97, 106);     // Red       #bf616a
  t.info = C::RGB(136, 192, 208);     // Frost     #88c0d0
  t.mutedText = C::RGB(76, 86, 106);  // Polar3    #4c566a

  // Completion
  t.completionCommand = C::RGB(129, 161, 193);  // Frost   #81a1c1
  t.completionObject = C::RGB(163, 190, 140);   // Green   #a3be8c

  // Value formatting
  t.valueString = C::RGB(208, 135, 112);  // Orange  #d08770
  t.valueNumber = C::RGB(163, 190, 140);  // Green   #a3be8c

  // Banner (decorative)
  t.bannerQuote = C::RGB(208, 135, 112);   // Orange   #d08770
  t.bannerAuthor = C::RGB(163, 190, 140);  // Green    #a3be8c
  t.bannerTitle = C::RGB(216, 222, 233);   // Snow     #d8dee9
  t.bannerText = C::RGB(76, 86, 106);      // Polar    #4c566a

  // Tree view
  t.treeSession = C::RGB(235, 203, 139);  // Yellow   #ebcb8b
  t.treeBus = C::RGB(163, 190, 140);      // Green    #a3be8c
  t.treeGroup = C::RGB(129, 161, 193);    // Frost    #81a1c1
  t.treeObject = C::RGB(136, 192, 208);   // Frost    #88c0d0
  t.treeClosed = C::RGB(76, 86, 106);     // Polar    #4c566a
  t.treePlain = C::RGB(216, 222, 233);    // Snow     #d8dee9
  t.treeConnector = C::RGB(76, 86, 106);  // Polar    #4c566a

  // Status bar
  t.barBackground = C::RGB(67, 76, 94);     // Polar2   #434c5e
  t.barForeground = C::RGB(216, 222, 233);  // Snow     #d8dee9
  t.barAccent = C::RGB(94, 129, 172);       // Frost    #5e81ac

  // Input pane
  t.inputBackground = C::RGB(46, 52, 64);       // Polar0   #2e3440
  t.inputForeground = C::RGB(216, 222, 233);    // Snow     #d8dee9
  t.completionBackground = C::RGB(46, 52, 64);  // Polar0   #2e3440

  // Secondary panes (watches, logs)
  t.paneBackground = C::RGB(59, 66, 82);       // Polar1   #3b4252
  t.paneForeground = C::RGB(216, 222, 233);    // Snow     #d8dee9
  t.paneTitleBackground = C::RGB(67, 76, 94);  // Polar2   #434c5e
  t.paneSeparator = C::RGB(76, 86, 106);       // Polar3   #4c566a

  // Log levels
  t.logTrace = C::RGB(76, 86, 106);        // Polar3   #4c566a
  t.logCriticalFg = C::RGB(46, 52, 64);    // Polar0   #2e3440
  t.logCriticalBg = C::RGB(191, 97, 106);  // Red      #bf616a

  return t;
}

Theme gruvboxDarkTheme()
{
  using C = ftxui::Color;

  Theme t;

  // Semantic colors
  t.accent = C::RGB(250, 189, 47);      // Yellow   #fabd2f
  t.success = C::RGB(184, 187, 38);     // Green    #b8bb26
  t.error = C::RGB(251, 73, 52);        // Red      #fb4934
  t.info = C::RGB(142, 192, 124);       // Aqua     #8ec07c
  t.mutedText = C::RGB(168, 153, 132);  // Gray    #a89984

  // Completion
  t.completionCommand = C::RGB(131, 165, 152);  // Blue    #83a598
  t.completionObject = C::RGB(184, 187, 38);    // Green   #b8bb26

  // Value formatting
  t.valueString = C::RGB(254, 128, 25);  // Orange  #fe8019
  t.valueNumber = C::RGB(184, 187, 38);  // Green   #b8bb26

  // Banner (decorative)
  t.bannerQuote = C::RGB(254, 128, 25);   // Orange   #fe8019
  t.bannerAuthor = C::RGB(184, 187, 38);  // Green    #b8bb26
  t.bannerTitle = C::RGB(235, 219, 178);  // Fg       #ebdbb2
  t.bannerText = C::RGB(168, 153, 132);   // Gray     #a89984

  // Tree view
  t.treeSession = C::RGB(250, 189, 47);     // Yellow   #fabd2f
  t.treeBus = C::RGB(184, 187, 38);         // Green    #b8bb26
  t.treeGroup = C::RGB(131, 165, 152);      // Blue     #83a598
  t.treeObject = C::RGB(142, 192, 124);     // Aqua     #8ec07c
  t.treeClosed = C::RGB(168, 153, 132);     // Gray     #a89984
  t.treePlain = C::RGB(235, 219, 178);      // Fg       #ebdbb2
  t.treeConnector = C::RGB(168, 153, 132);  // Gray     #a89984

  // Status bar
  t.barBackground = C::RGB(80, 73, 69);     // Bg2      #504945
  t.barForeground = C::RGB(235, 219, 178);  // Fg       #ebdbb2
  t.barAccent = C::RGB(131, 165, 152);      // Blue     #83a598

  // Input pane
  t.inputBackground = C::RGB(40, 40, 40);       // Bg       #282828
  t.inputForeground = C::RGB(235, 219, 178);    // Fg       #ebdbb2
  t.completionBackground = C::RGB(50, 48, 47);  // Bg0h     #32302f

  // Secondary panes (watches, logs)
  t.paneBackground = C::RGB(60, 56, 54);       // Bg1      #3c3836
  t.paneForeground = C::RGB(235, 219, 178);    // Fg       #ebdbb2
  t.paneTitleBackground = C::RGB(80, 73, 69);  // Bg2      #504945
  t.paneSeparator = C::RGB(102, 92, 84);       // Bg3      #665c54

  // Log levels
  t.logTrace = C::RGB(168, 153, 132);     // Gray     #a89984
  t.logCriticalFg = C::RGB(40, 40, 40);   // Bg       #282828
  t.logCriticalBg = C::RGB(251, 73, 52);  // Red      #fb4934

  return t;
}

Theme gruvboxLightTheme()
{
  using C = ftxui::Color;

  Theme t;

  // Semantic colors
  t.accent = C::RGB(181, 118, 20);      // Yellow   #b57614
  t.success = C::RGB(121, 116, 14);     // Green    #79740e
  t.error = C::RGB(157, 0, 6);          // Red      #9d0006
  t.info = C::RGB(66, 123, 88);         // Aqua     #427b58
  t.mutedText = C::RGB(124, 111, 100);  // Gray    #7c6f64

  // Completion
  t.completionCommand = C::RGB(7, 102, 120);  // Blue    #076678
  t.completionObject = C::RGB(121, 116, 14);  // Green   #79740e

  // Value formatting
  t.valueString = C::RGB(175, 58, 3);    // Orange  #af3a03
  t.valueNumber = C::RGB(121, 116, 14);  // Green   #79740e

  // Banner (decorative)
  t.bannerQuote = C::RGB(175, 58, 3);     // Orange   #af3a03
  t.bannerAuthor = C::RGB(121, 116, 14);  // Green    #79740e
  t.bannerTitle = C::RGB(60, 56, 54);     // Fg       #3c3836
  t.bannerText = C::RGB(124, 111, 100);   // Gray     #7c6f64

  // Tree view
  t.treeSession = C::RGB(181, 118, 20);     // Yellow   #b57614
  t.treeBus = C::RGB(121, 116, 14);         // Green    #79740e
  t.treeGroup = C::RGB(7, 102, 120);        // Blue     #076678
  t.treeObject = C::RGB(66, 123, 88);       // Aqua     #427b58
  t.treeClosed = C::RGB(124, 111, 100);     // Gray     #7c6f64
  t.treePlain = C::RGB(60, 56, 54);         // Fg       #3c3836
  t.treeConnector = C::RGB(124, 111, 100);  // Gray     #7c6f64

  // Status bar
  t.barBackground = C::RGB(189, 174, 147);  // Bg3      #bdae93
  t.barForeground = C::RGB(60, 56, 54);     // Fg       #3c3836
  t.barAccent = C::RGB(7, 102, 120);        // Blue     #076678

  // Input pane
  t.inputBackground = C::RGB(251, 241, 199);       // Bg       #fbf1c7
  t.inputForeground = C::RGB(60, 56, 54);          // Fg       #3c3836
  t.completionBackground = C::RGB(242, 229, 188);  // Bg0      #f2e5bc

  // Secondary panes (watches, logs)
  t.paneBackground = C::RGB(235, 219, 178);       // Bg1      #ebdbb2
  t.paneForeground = C::RGB(60, 56, 54);          // Fg       #3c3836
  t.paneTitleBackground = C::RGB(189, 174, 147);  // Bg3      #bdae93
  t.paneSeparator = C::RGB(168, 153, 132);        // Bg4      #a89984

  // Log levels
  t.logTrace = C::RGB(124, 111, 100);       // Gray     #7c6f64
  t.logCriticalFg = C::RGB(251, 241, 199);  // Bg       #fbf1c7
  t.logCriticalBg = C::RGB(157, 0, 6);      // Red      #9d0006

  return t;
}

Theme tokyoNightTheme()
{
  using C = ftxui::Color;

  Theme t;

  // Semantic colors
  t.accent = C::RGB(224, 175, 104);   // Yellow    #e0af68
  t.success = C::RGB(158, 206, 106);  // Green     #9ece6a
  t.error = C::RGB(247, 118, 142);    // Red       #f7768e
  t.info = C::RGB(115, 218, 202);     // Teal      #73daca
  t.mutedText = C::RGB(86, 95, 137);  // Comment   #565f89

  // Completion
  t.completionCommand = C::RGB(122, 162, 247);  // Blue     #7aa2f7
  t.completionObject = C::RGB(158, 206, 106);   // Green    #9ece6a

  // Value formatting
  t.valueString = C::RGB(255, 158, 100);  // Orange  #ff9e64
  t.valueNumber = C::RGB(158, 206, 106);  // Green   #9ece6a

  // Banner (decorative)
  t.bannerQuote = C::RGB(255, 158, 100);   // Orange    #ff9e64
  t.bannerAuthor = C::RGB(158, 206, 106);  // Green     #9ece6a
  t.bannerTitle = C::RGB(169, 177, 214);   // Fg        #a9b1d6
  t.bannerText = C::RGB(86, 95, 137);      // Comment   #565f89

  // Tree view
  t.treeSession = C::RGB(224, 175, 104);  // Yellow    #e0af68
  t.treeBus = C::RGB(158, 206, 106);      // Green     #9ece6a
  t.treeGroup = C::RGB(122, 162, 247);    // Blue      #7aa2f7
  t.treeObject = C::RGB(115, 218, 202);   // Teal      #73daca
  t.treeClosed = C::RGB(86, 95, 137);     // Comment   #565f89
  t.treePlain = C::RGB(169, 177, 214);    // Fg        #a9b1d6
  t.treeConnector = C::RGB(86, 95, 137);  // Comment   #565f89

  // Status bar
  t.barBackground = C::RGB(41, 46, 66);     // Surface   #292e42
  t.barForeground = C::RGB(169, 177, 214);  // Fg        #a9b1d6
  t.barAccent = C::RGB(122, 162, 247);      // Blue      #7aa2f7

  // Input pane
  t.inputBackground = C::RGB(26, 27, 38);       // Bg        #1a1b26
  t.inputForeground = C::RGB(169, 177, 214);    // Fg        #a9b1d6
  t.completionBackground = C::RGB(26, 27, 38);  // BgDark    #1a1b26

  // Secondary panes (watches, logs)
  t.paneBackground = C::RGB(22, 22, 30);       // BgDark    #16161e
  t.paneForeground = C::RGB(169, 177, 214);    // Fg        #a9b1d6
  t.paneTitleBackground = C::RGB(41, 46, 66);  // Surface   #292e42
  t.paneSeparator = C::RGB(54, 58, 79);        // Surface1  #363a4f

  // Log levels
  t.logTrace = C::RGB(86, 95, 137);         // Comment   #565f89
  t.logCriticalFg = C::RGB(26, 27, 38);     // Bg        #1a1b26
  t.logCriticalBg = C::RGB(247, 118, 142);  // Red       #f7768e

  return t;
}

Theme solarizedLightTheme()
{
  using C = ftxui::Color;

  Theme t;

  // Semantic colors
  t.accent = C::RGB(181, 137, 0);       // Yellow   #b58900
  t.success = C::RGB(133, 153, 0);      // Green    #859900
  t.error = C::RGB(220, 50, 47);        // Red      #dc322f
  t.info = C::RGB(42, 161, 152);        // Cyan     #2aa198
  t.mutedText = C::RGB(101, 123, 131);  // Base00  #657b83

  // Completion
  t.completionCommand = C::RGB(38, 139, 210);  // Blue     #268bd2
  t.completionObject = C::RGB(133, 153, 0);    // Green    #859900

  // Value formatting
  t.valueString = C::RGB(203, 75, 22);  // Orange  #cb4b16
  t.valueNumber = C::RGB(133, 153, 0);  // Green   #859900

  // Banner (decorative)
  t.bannerQuote = C::RGB(203, 75, 22);   // Orange   #cb4b16
  t.bannerAuthor = C::RGB(133, 153, 0);  // Green    #859900
  t.bannerTitle = C::RGB(88, 110, 117);  // Base01   #586e75
  t.bannerText = C::RGB(101, 123, 131);  // Base00   #657b83

  // Tree view
  t.treeSession = C::RGB(181, 137, 0);      // Yellow   #b58900
  t.treeBus = C::RGB(133, 153, 0);          // Green    #859900
  t.treeGroup = C::RGB(38, 139, 210);       // Blue     #268bd2
  t.treeObject = C::RGB(42, 161, 152);      // Cyan     #2aa198
  t.treeClosed = C::RGB(101, 123, 131);     // Base00   #657b83
  t.treePlain = C::RGB(88, 110, 117);       // Base01   #586e75
  t.treeConnector = C::RGB(101, 123, 131);  // Base00   #657b83

  // Status bar
  t.barBackground = C::RGB(220, 215, 198);  // Between Base2 and Base1  #dcd7c6
  t.barForeground = C::RGB(88, 110, 117);   // Base01   #586e75
  t.barAccent = C::RGB(38, 139, 210);       // Blue     #268bd2

  // Input pane
  t.inputBackground = C::RGB(253, 246, 227);       // Base3    #fdf6e3
  t.inputForeground = C::RGB(88, 110, 117);        // Base01   #586e75
  t.completionBackground = C::RGB(245, 239, 220);  // Between Base3 and Base2  #f5efdc

  // Secondary panes (watches, logs)
  t.paneBackground = C::RGB(238, 232, 213);       // Base2    #eee8d5
  t.paneForeground = C::RGB(88, 110, 117);        // Base01   #586e75
  t.paneTitleBackground = C::RGB(220, 215, 198);  // Between Base2 and Base1  #dcd7c6
  t.paneSeparator = C::RGB(181, 186, 185);        // Between Base1 and Base01  #b5bab9

  // Log levels
  t.logTrace = C::RGB(101, 123, 131);       // Base00   #657b83
  t.logCriticalFg = C::RGB(253, 246, 227);  // Base3    #fdf6e3
  t.logCriticalBg = C::RGB(220, 50, 47);    // Red      #dc322f

  return t;
}

//--------------------------------------------------------------------------------------------------------------
// Active theme accessor
//--------------------------------------------------------------------------------------------------------------

namespace
{

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
