// === status_bar.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "status_bar.h"

// local
#include "byte_format.h"
#include "styles.h"
#include "unicode.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// std
#include <chrono>

namespace sen::components::term
{

using styles::barAccent;
using styles::barBg;
using styles::barFg;
using styles::barWarn;
using styles::mutedText;

namespace
{

// Rate thresholds for the status-bar throughput indicator. Values feel right for kernel-level
// telemetry traffic; tune here if the heuristic stops matching real-world load.
constexpr double rateGreenLimit = 10.0 * byte_format::bytesPerKilobyte;  // < 10 KB/s
constexpr double rateYellowLimit = byte_format::bytesPerMegabyte;        // < 1 MB/s

/// Color-code a rate value: barFg for zero, green for low, yellow for moderate, red for high.
ftxui::Decorator rateColor(double bytesPerSec)
{
  if (bytesPerSec <= 0.0)
  {
    return barFg();
  }
  if (bytesPerSec < rateGreenLimit)
  {
    return ftxui::color(activeTheme().success);
  }
  if (bytesPerSec < rateYellowLimit)
  {
    return ftxui::color(activeTheme().accent);
  }
  return ftxui::color(activeTheme().error);
}

}  // namespace

ftxui::Element renderStatusBar(const StatusBarData& data)
{
  using namespace ftxui;

  if (data.modeHint.has_value())
  {
    Elements bar;
    bar.push_back(text(" " + *data.modeHint) | barFg());
    bar.push_back(filler());
    if (!data.timeStr.empty())
    {
      bar.push_back(text(data.timeStr + " ") | barFg());
    }
    return hbox(std::move(bar)) | barBg();
  }

  // Left side
  Elements left;

  {
    auto nowSec =
      std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const auto* beat = (nowSec % 2 == 0) ? unicode::beatOn : unicode::beatOff;
    left.push_back(text(" "));
    left.push_back(text(beat) | barAccent());
  }

  {
    std::string label;
    if (!data.appName.empty())
    {
      label += data.appName;
    }
    if (!data.hostName.empty())
    {
      label += "@";
      label += data.hostName;
    }
    if (!label.empty())
    {
      left.push_back(text(" " + label) | barAccent());
    }
  }

  if (!data.txRateStr.empty())
  {
    constexpr std::size_t rateWidth = 10;
    auto padRate = [](const std::string& s)
    { return s.size() < rateWidth ? s + std::string(rateWidth - s.size(), ' ') : s; };
    left.push_back(text(std::string("  ") + unicode::arrowUp) | barFg());
    left.push_back(text(padRate(data.txRateStr)) | rateColor(data.txRate));
    left.push_back(text(unicode::arrowDown) | barFg());
    left.push_back(text(padRate(data.rxRateStr)) | rateColor(data.rxRate));
  }

  if (!data.scopePath.empty())
  {
    left.push_back(text("  " + data.scopePath) | barFg());
  }

  // Right side
  Elements right;

  {
    Elements objs;
    objs.push_back(text("objects ") | barFg());
    if (!data.isRootScope)
    {
      objs.push_back(text(std::to_string(data.objectsInScope)) | barAccent());
      objs.push_back(text("/") | barFg());
    }
    objs.push_back(text(std::to_string(data.objectsTotal)) | barAccent());
    right.push_back(hbox(std::move(objs)));
  }

  {
    right.push_back(text("  listeners ") | barFg());
    right.push_back(text(std::to_string(data.listenerCount)) | barAccent());
  }

  {
    right.push_back(text("  sources ") | barFg());
    right.push_back(text(std::to_string(data.sourceCount)) | barAccent());
  }

  if (data.pendingCalls > 0)
  {
    right.push_back(text("  pending ") | barFg());
    right.push_back(text(std::to_string(data.pendingCalls)) | barWarn());
  }

  if (!data.logPaneVisible)
  {
    auto unread = data.unreadLogCount + data.unreadEventCount;
    right.push_back(text("  logs ") | barFg());
    if (unread > 0)
    {
      right.push_back(text(std::to_string(unread)) | barWarn());
      right.push_back(text(" F4") | mutedText());
    }
    else
    {
      right.push_back(text("off ") | barFg());
      right.push_back(text("F4") | mutedText());
    }
  }

  {
    right.push_back(text("  watches ") | barFg());
    if (!data.watchPaneVisible && data.watchCount > 0)
    {
      right.push_back(text(std::to_string(data.watchCount)) | barWarn());
      right.push_back(text(" F5") | mutedText());
    }
    else if (!data.watchPaneVisible)
    {
      right.push_back(text("off ") | barFg());
      right.push_back(text("F5") | mutedText());
    }
    else
    {
      right.push_back(text(std::to_string(data.watchCount)) | barAccent());
    }
  }

  if (!data.timeStr.empty())
  {
    right.push_back(text("  " + data.timeStr) | barFg());
  }

  right.push_back(text(" "));

  return hbox({hbox(std::move(left)), filler(), hbox(std::move(right))}) | barBg();
}

}  // namespace sen::components::term
