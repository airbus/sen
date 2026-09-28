// === clipboard.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_CLIPBOARD_H
#define SEN_COMPONENTS_TERM_SRC_CLIPBOARD_H

// std
#include <optional>
#include <string>
#include <string_view>

namespace sen::components::term::clipboard
{

/// Put `text` on the system clipboard. All platform and protocol handling is confined to this module.
///
/// Two paths are used. The terminal escape (OSC 52) goes out on the calling thread: it is one write to the
/// tty and the only path that survives an ssh session. The local helper, pbcopy or wl-copy or xclip or
/// xsel, forks, and up to four fork+exec pairs inside the component's 33 ms cycle are counted by the
/// runner as missed frames, so that path runs on a worker thread.
///
/// On platforms with a primary selection (X11, some Wayland compositors) the text goes to both the
/// clipboard (Ctrl+V) and the primary selection (middle-click).
void copy(std::string_view text);

/// The reason the last local write failed, once, or nothing.
///
/// The helper runs on the worker, so `copy` cannot say whether it worked. Without this a machine with no
/// local helper would tell the user the text was copied and they would find out by pasting. Poll it from
/// the render tick and show what it returns. Reading it clears it.
[[nodiscard]] std::optional<std::string> takeFailure();

/// RFC 4648 base64, as the OSC 52 payload needs it.
///
/// Exposed because it is the one piece of this module that is identical on every platform. Reaching it
/// through the terminal escape instead needs descriptor redirection, which is written for POSIX only, so
/// the portable half would be verified on one platform out of three.
[[nodiscard]] std::string base64Encode(std::string_view input);

/// Stop the worker, waiting a bounded time for a write in flight. Call before the component returns.
///
/// There is one worker, not a thread per copy: a drag-select loop would otherwise make several, and a
/// wedged helper would leave each one hanging for the life of the process. This is the only place that
/// waits for the worker, for two seconds, after which it is abandoned rather than holding up the
/// shutdown.
void shutdown();

}  // namespace sen::components::term::clipboard

#endif  // SEN_COMPONENTS_TERM_SRC_CLIPBOARD_H
