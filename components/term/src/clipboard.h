// === clipboard.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_CLIPBOARD_H
#define SEN_COMPONENTS_TERM_SRC_CLIPBOARD_H

#include <string_view>

namespace sen::components::term::clipboard
{

/// Copy `text` to the system clipboard. On platforms that distinguish a primary
/// selection (X11 and some Wayland compositors) the text is written to both the
/// main clipboard (for Ctrl+V) and the primary selection (for middle-click
/// paste). Best-effort: returns true if any path was reached, false if nothing
/// on this platform could accept the text.
///
/// All platform- and protocol-specific handling is confined to this module.
bool copy(std::string_view text);

}  // namespace sen::components::term::clipboard

#endif  // SEN_COMPONENTS_TERM_SRC_CLIPBOARD_H
