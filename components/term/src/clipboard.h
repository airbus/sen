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
/// Two paths are used. The terminal escape (OSC 52) goes out on the calling thread: it is one write to
/// the tty and it is the only path that survives an ssh session. The local helper -- pbcopy, wl-copy,
/// xclip, xsel -- forks, and up to four fork+exec pairs inside the component's 33 ms cycle were counted
/// by the runner as missed frames, so it runs on a worker thread instead.
///
/// On platforms with a primary selection (X11, some Wayland compositors) the text goes to both the
/// clipboard (Ctrl+V) and the primary selection (middle-click).
void copy(std::string_view text);

/// The reason the last local write failed, once, or nothing.
///
/// The helper runs on the worker, so `copy` cannot say whether it worked -- and it used to be reported
/// as a success unconditionally, which meant a machine with no xclip told the user text had been copied
/// and they found out by pasting. Poll this from the render tick and show what it returns. Reading it
/// clears it.
[[nodiscard]] std::optional<std::string> takeFailure();

/// RFC 4648 base64, as the OSC 52 payload needs it.
///
/// Exposed because it is the one piece of this module that is identical on every platform, and it used
/// to be reachable from a test only through the terminal escape -- which needs descriptor redirection,
/// which was written for POSIX only. So the portable half was verified on one platform out of three.
[[nodiscard]] std::string base64Encode(std::string_view input);

/// Stop the worker, waiting a bounded time for a write in flight. Call before the component returns.
///
/// The worker used to be a detached thread per copy, so a drag-select loop could make several and a
/// wedged helper left each one hanging for the life of the process. There is one worker now, and this
/// is the only place that waits for it -- for two seconds, after which it is abandoned rather than
/// holding up the shutdown, because a helper that has not returned by then is not going to.
void shutdown();

}  // namespace sen::components::term::clipboard

#endif  // SEN_COMPONENTS_TERM_SRC_CLIPBOARD_H
