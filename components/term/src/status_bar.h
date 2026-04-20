// === status_bar.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_STATUS_BAR_H
#define SEN_COMPONENTS_TERM_SRC_STATUS_BAR_H

// ftxui
#include <ftxui/dom/elements.hpp>

// std
#include <cstddef>
#include <optional>
#include <string>

namespace sen::components::term
{

/// Renders the status bar between the output pane and input pane.
struct StatusBarData
{
  std::string appName;
  std::string hostName;
  std::string scopePath;

  std::size_t objectsInScope = 0;
  std::size_t objectsTotal = 0;
  bool isRootScope = true;
  std::size_t pendingCalls = 0;
  std::size_t sourceCount = 0;
  std::size_t listenerCount = 0;

  std::size_t watchCount = 0;
  bool watchPaneVisible = true;

  std::size_t unreadLogCount = 0;
  std::size_t unreadEventCount = 0;
  bool logPaneVisible = true;

  std::string timeStr;

  // Network throughput (empty = no transport active).
  std::string txRateStr;
  std::string rxRateStr;
  double txRate = 0.0;
  double rxRate = 0.0;

  /// When set, the status bar replaces its usual content with this hint (time is preserved
  /// on the right). Used for the form's key-binding summary.
  std::optional<std::string> modeHint;
};

/// Render the status bar as an FTXUI element.
[[nodiscard]] ftxui::Element renderStatusBar(const StatusBarData& data);

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_STATUS_BAR_H
