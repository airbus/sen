// === output_pane.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "output_pane.h"

// component
#include "parse_utils.h"
#include "styles.h"
#include "unicode.h"

// sen
#include "sen/core/base/checked_conversions.h"

// ftxui
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/terminal.hpp>

// std
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace sen::components::term
{

using sen::std_util::checkedConversion;

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

// Longest text a single pane entry will lay out. A captured stderr line or an unparsable log message
// arrives with no length agreed anywhere, and one of them is enough to stop the pane redrawing.
constexpr std::size_t maxTextLength = 4096U;

// Frame dwell time for the pending-call spinner. Tuned for a comfortable rotation speed that
// doesn't feel either laggy or seizure-inducing.
constexpr int64_t spinnerFrameMs = 80;

// Milliseconds per second. Named for readability where we divide an ms count to print seconds.
constexpr double millisecondsPerSecond = 1000.0;

// Endpoints of the scroll position range. scrollPosition_ is a 0-to-1 fraction where 1.0 pins
// the view to the bottom; named for intent rather than appearing as bare literals in the body.
constexpr float scrollBottom = 1.0F;
constexpr float scrollTop = 0.0F;

/// Render a pending call entry with the current spinner frame and elapsed time.
ftxui::Element renderPendingEntry(const std::string& description, std::chrono::steady_clock::time_point startTime)
{
  auto now = std::chrono::steady_clock::now();
  auto frameIdx =
    (std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() / spinnerFrameMs) %
    unicode::spinnerFrames.size();
  auto elapsed = static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count()) /
                 millisecondsPerSecond;

  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1) << elapsed << "s";

  return ftxui::hbox({ftxui::text(std::string(unicode::spinnerFrames.at(frameIdx))) | styles::accent(),
                      ftxui::text(" " + description) | ftxui::bold,
                      ftxui::text(" (" + oss.str() + ")") | styles::mutedText()});
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// OutputPane
//--------------------------------------------------------------------------------------------------------------

void OutputPane::setMaxLines(std::size_t maxLines) { maxLines_ = maxLines; }

void OutputPane::appendText(std::string_view text)
{
  // paragraph() builds a node per word and re-solves the flexbox every frame, and both callers carry
  // text from outside: a captured stderr line and a log message the router could not parse.
  auto elem = text.empty() ? ftxui::text(" ")
                           : ftxui::hbox({ftxui::text("  "), ftxui::paragraph(truncateUtf8(text, maxTextLength))});
  lines_.emplace_back(std::move(elem));
  contentHeightDirty_ = true;
  trimLines();
  if (followBottom_)
  {
    scrollPosition_ = scrollBottom;
  }
}

void OutputPane::appendInfo(std::string_view text)
{
  lines_.emplace_back(
    ftxui::hbox({ftxui::text("  ") | styles::mutedText(), ftxui::paragraph(std::string(text)) | styles::mutedText()}));
  contentHeightDirty_ = true;
  trimLines();
  if (followBottom_)
  {
    scrollPosition_ = scrollBottom;
  }
}

void OutputPane::appendElement(ftxui::Element element)
{
  lines_.emplace_back(std::move(element));
  contentHeightDirty_ = true;
  trimLines();
  if (followBottom_)
  {
    scrollPosition_ = scrollBottom;
  }
}

void OutputPane::appendPendingCall(std::size_t id, std::string description)
{
  lines_.emplace_back(PendingEntry {id, std::move(description), std::chrono::steady_clock::now()});
  ++pendingCount_;
  contentHeightDirty_ = true;
  trimLines();
  if (followBottom_)
  {
    scrollPosition_ = scrollBottom;
  }
}

void OutputPane::replacePendingCall(std::size_t id, ftxui::Element result)
{
  for (auto& entry: lines_)
  {
    if (auto* pending = std::get_if<PendingEntry>(&entry); pending != nullptr && pending->id == id)
    {
      entry = Entry {std::move(result)};
      if (pendingCount_ > 0)
      {
        --pendingCount_;
      }
      return;
    }
  }
}

bool OutputPane::hasPendingCalls() const { return pendingCount_ > 0; }

void OutputPane::trimLines()
{
  if (maxLines_ > 0 && lines_.size() > maxLines_)
  {
    auto dropCount = lines_.size() - maxLines_;
    for (std::size_t i = 0; i < dropCount; ++i)
    {
      if (std::holds_alternative<PendingEntry>(lines_[i]) && pendingCount_ > 0)
      {
        --pendingCount_;
      }
    }
    for (std::size_t i = 0; i < dropCount; ++i)
    {
      lines_.pop_front();
    }
  }
}

void OutputPane::clear()
{
  lines_.clear();
  contentHeightDirty_ = true;
  pendingCount_ = 0;
  scrollPosition_ = scrollBottom;
  followBottom_ = true;
}

void OutputPane::scrollUp(int rows)
{
  float step = checkedConversion<float>(rows) / checkedConversion<float>(contentHeight_);
  scrollPosition_ = std::max(scrollTop, scrollPosition_ - step);
  followBottom_ = false;
}

void OutputPane::scrollDown(int rows)
{
  float step = checkedConversion<float>(rows) / checkedConversion<float>(contentHeight_);
  scrollPosition_ = std::min(scrollBottom, scrollPosition_ + step);
  if (scrollPosition_ >= scrollBottom)
  {
    followBottom_ = true;
  }
}

void OutputPane::scrollToBottom()
{
  scrollPosition_ = scrollBottom;
  followBottom_ = true;
}

ftxui::Element OutputPane::render()
{
  if (lines_.empty())
  {
    return ftxui::emptyElement();
  }

  const int wrapWidth = ftxui::Terminal::Size().dimx;
  const bool layoutChanged = contentHeightDirty_ || wrapWidth != lastWrapWidth_;

  // A pending call animates its spinner, so while one is in flight the vbox has to be rebuilt each
  // frame. Otherwise it is reused: copying 5,000 shared_ptrs into a fresh vector and building a new
  // 5,000-child vbox cost 10,000 atomic refcount operations per frame for content that had not changed,
  // and most frames are idle redraws or single keystrokes.
  if (!contentCache_ || layoutChanged || pendingCount_ > 0)
  {
    ftxui::Elements content;
    content.reserve(lines_.size());

    for (const auto& entry: lines_)
    {
      if (const auto* pending = std::get_if<PendingEntry>(&entry); pending != nullptr)
      {
        content.push_back(renderPendingEntry(pending->description, pending->startTime));
      }
      else
      {
        content.push_back(std::get<ftxui::Element>(entry));
      }
    }
    contentCache_ = ftxui::vbox(std::move(content));
  }

  auto inner = contentCache_;

  // Measure only when it can have changed. A wrap-width change counts: the same lines occupy a
  // different number of rows. This is the pane's own measurement, for the scroll step; FTXUI's Render
  // does its own pass regardless, which the previous comment here overstated as being avoided.
  if (layoutChanged)
  {
    inner->ComputeRequirement();
    contentHeight_ = std::max(1, inner->requirement().min_y);
    contentHeightDirty_ = false;
    lastWrapWidth_ = wrapWidth;
  }

  return inner | ftxui::focusPositionRelative(0.0F, scrollPosition_) | ftxui::vscroll_indicator | ftxui::yframe |
         ftxui::flex;
}

}  // namespace sen::components::term
