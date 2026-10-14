// === ansi_text.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_ANSI_TEXT_H
#define SEN_COMPONENTS_TERM_SRC_ANSI_TEXT_H

// ftxui
#include <ftxui/dom/elements.hpp>

// std
#include <string>

namespace sen::components::term
{

/// Renders a captured line, escapes and all, as a styled element.
///
/// Term captures the descriptors rather than routing loggers, so a line arrives formatted by
/// whoever wrote it: spdlog's console sink colours by level, which is the only thing left that
/// says a warning is a warning once nothing carries the level as a value. FTXUI composes a frame
/// cell by cell and cannot be handed raw escapes: it ignores the control byte and draws the rest,
/// so `\x1b[32m` arrives on screen as the characters `[32m`. Translating into FTXUI's own styling
/// keeps the meaning in the form FTXUI works in.
///
/// Only SGR is interpreted, which is the whole of what a console sink emits. Cursor movement, erase
/// and the rest are dropped: they mean nothing inside one line of a pane, and acting on them would
/// let captured output move the cursor around someone else's interface.
[[nodiscard]] ftxui::Element renderAnsiLine(const std::string& line);

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_ANSI_TEXT_H
