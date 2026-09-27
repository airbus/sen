// === signal_stack.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_SIGNAL_STACK_H
#define SEN_COMPONENTS_TERM_SRC_SIGNAL_STACK_H

namespace sen::components::term
{

/// Put `SA_ONSTACK` back on the fatal signal handlers, keeping whatever handler is installed.
///
/// FTXUI installs its own handlers for the fatal signals when the terminal goes up, with a plain `sigaction`
/// and no `SA_ONSTACK`. Its handler is well behaved, restoring the terminal and re-raising into the handler
/// it displaced so that a crash still reaches the crash reporter, but the missing flag quietly undoes the
/// alternate signal stack the kernel gives every thread it starts. That stack is what lets a stack overflow
/// be reported: without it the handler runs on the exhausted stack and faults again, and the process dies
/// with no dump.
///
/// Call this after the terminal is installed. It changes only the flag, so FTXUI's handler and its
/// uninstall still work. Returns the number of handlers it changed. A no-op on Windows, which has no
/// alternate signal stack.
int restoreAltStackOnFatalHandlers();

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_SIGNAL_STACK_H
