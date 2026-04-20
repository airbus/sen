// === output_pane.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "output_pane.h"

// local
#include "styles.h"
#include "unicode.h"

// sen
#include "sen/core/base/checked_conversions.h"

// ftxui
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>

// std
#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <sstream>

namespace sen::components::term
{

using sen::std_util::checkedConversion;

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

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
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count() / millisecondsPerSecond;

  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1) << elapsed << "s";

  return ftxui::hbox({ftxui::text(std::string(unicode::spinnerFrames[frameIdx])) | styles::accent(),
                      ftxui::text(" " + description) | ftxui::bold,
                      ftxui::text(" (" + oss.str() + ")") | styles::mutedText()});
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// OutputPane
//--------------------------------------------------------------------------------------------------------------

void OutputPane::setMaxLines(std::size_t maxLines) { maxLines_ = maxLines; }
void OutputPane::setBottomAligned(bool bottomAligned) noexcept { bottomAligned_ = bottomAligned; }

void OutputPane::appendText(std::string_view text)
{
  auto elem = text.empty() ? ftxui::text(" ") : ftxui::hbox({ftxui::text("  "), ftxui::paragraph(std::string(text))});
  lines_.push_back(Entry {std::move(elem)});
  trimLines();
  if (followBottom_)
  {
    scrollPosition_ = scrollBottom;
  }
}

void OutputPane::appendInfo(std::string_view text)
{
  lines_.push_back(Entry {
    ftxui::hbox({ftxui::text("  ") | styles::mutedText(), ftxui::paragraph(std::string(text)) | styles::mutedText()})});
  trimLines();
  if (followBottom_)
  {
    scrollPosition_ = scrollBottom;
  }
}

void OutputPane::appendCommand(std::string_view command)
{
  lines_.push_back(
    Entry {ftxui::hbox({ftxui::text("> ") | ftxui::bold | styles::echoSuccess(), ftxui::text(std::string(command))})});
  trimLines();
  if (followBottom_)
  {
    scrollPosition_ = scrollBottom;
  }
}

void OutputPane::appendElement(ftxui::Element element)
{
  lines_.push_back(Entry {std::move(element)});
  trimLines();
  if (followBottom_)
  {
    scrollPosition_ = scrollBottom;
  }
}

void OutputPane::appendPendingCall(std::size_t id, std::string description)
{
  lines_.push_back(Entry {PendingEntry {id, std::move(description), std::chrono::steady_clock::now()}});
  ++pendingCount_;
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
    lines_.erase(lines_.begin(), lines_.begin() + checkedConversion<ptrdiff_t>(dropCount));
  }
}

void OutputPane::clear()
{
  lines_.clear();
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

  if (bottomAligned_)
  {
    content.insert(content.begin(), ftxui::filler());
  }

  auto inner = ftxui::vbox(std::move(content));
  inner->ComputeRequirement();
  contentHeight_ = std::max(1, inner->requirement().min_y);

  return inner | ftxui::focusPositionRelative(0.0F, scrollPosition_) | ftxui::vscroll_indicator | ftxui::yframe |
         ftxui::flex;
}

}  // namespace sen::components::term
