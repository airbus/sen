// === browser_open.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CLI_RUN_BROWSER_OPEN_H
#define SEN_CLI_RUN_BROWSER_OPEN_H

#include <chrono>
#include <string>

namespace sen::cli_run
{

/// Block until a TCP connect to host:port succeeds, or `timeout` elapses. Used to delay
/// the browser launch until the explorer is reachable; opening the URL while the kernel
/// is still bringing the jsonrpc listener up would land on a connection-refused page.
[[nodiscard]] bool waitForTcpListening(const std::string& host, int port, std::chrono::milliseconds timeout);

/// Hand the URL to the user's default browser via the OS launcher (`open`, `xdg-open`,
/// `start`). Returns false if the launcher could not be started; the caller is responsible
/// for printing the URL so a headless user still sees it.
bool openInBrowser(const std::string& url);

}  // namespace sen::cli_run

#endif  // SEN_CLI_RUN_BROWSER_OPEN_H
