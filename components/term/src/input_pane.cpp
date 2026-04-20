// === input_pane.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "input_pane.h"

// sen
#include "sen/core/base/checked_conversions.h"

// std
#include <cstddef>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace sen::components::term
{

using sen::std_util::checkedConversion;

InputPane::InputPane(SubmitCallback onSubmit): onSubmit_(std::move(onSubmit)) {}

void InputPane::setPrompt(std::string_view prompt) { prompt_ = prompt; }

std::string_view InputPane::getPrompt() const noexcept { return prompt_; }

std::string& InputPane::getBuffer() noexcept { return buffer_; }

void InputPane::submit()
{
  if (onSubmit_)
  {
    onSubmit_(buffer_);
  }

  if (!buffer_.empty())
  {
    addToHistory(buffer_);
  }

  buffer_.clear();
  historyIndex_ = -1;
  savedBuffer_.clear();
}

void InputPane::historyUp()
{
  if (history_.empty())
  {
    return;
  }

  if (historyIndex_ == -1)
  {
    savedBuffer_ = buffer_;
    historyIndex_ = 0;
  }
  else if (historyIndex_ < checkedConversion<int>(history_.size()) - 1)
  {
    ++historyIndex_;
  }
  else
  {
    return;
  }

  buffer_ = history_[checkedConversion<std::size_t>(historyIndex_)];
}

void InputPane::historyDown()
{
  if (historyIndex_ < 0)
  {
    return;
  }

  --historyIndex_;

  if (historyIndex_ < 0)
  {
    buffer_ = savedBuffer_;
    savedBuffer_.clear();
  }
  else
  {
    buffer_ = history_[checkedConversion<std::size_t>(historyIndex_)];
  }
}

void InputPane::addToHistory(std::string_view line)
{
  if (!history_.empty() && history_.front() == line)
  {
    return;
  }

  history_.emplace_front(line);

  while (history_.size() > maxHistoryLines_)
  {
    history_.pop_back();
  }

  if (!historyFile_.empty())
  {
    std::ofstream out(historyFile_, std::ios::app);
    if (out.is_open())
    {
      out << line << '\n';
    }
  }
}

void InputPane::setHistoryFile(std::filesystem::path path) { historyFile_ = std::move(path); }

void InputPane::loadHistory()
{
  if (historyFile_.empty())
  {
    return;
  }

  std::ifstream in(historyFile_);
  if (!in.is_open())
  {
    return;
  }

  std::vector<std::string> fileLines;
  std::string line;
  while (std::getline(in, line))
  {
    if (!line.empty())
    {
      fileLines.push_back(std::move(line));
    }
    line.clear();
  }
  in.close();

  if (fileLines.size() > maxHistoryLines_)
  {
    auto excess = fileLines.size() - maxHistoryLines_;
    fileLines.erase(fileLines.begin(), fileLines.begin() + checkedConversion<ptrdiff_t>(excess));

    std::ofstream out(historyFile_, std::ios::trunc);
    if (out.is_open())
    {
      for (const auto& l: fileLines)
      {
        out << l << '\n';
      }
    }
  }

  for (auto& l: fileLines)
  {
    history_.emplace_front(std::move(l));
    if (history_.size() > maxHistoryLines_)
    {
      history_.pop_back();
    }
  }
}

bool InputPane::searchHistory(std::string_view query)
{
  if (query.empty())
  {
    return false;
  }
  for (const auto& entry: history_)
  {
    if (entry.find(query) != std::string::npos)
    {
      buffer_ = entry;
      historyIndex_ = -1;
      return true;
    }
  }
  return false;
}

const std::deque<std::string>& InputPane::getHistory() const noexcept { return history_; }

}  // namespace sen::components::term
