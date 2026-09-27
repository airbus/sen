// === input_pane.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "input_pane.h"

// component
#include "util.h"

// sen
#include "sen/core/base/checked_conversions.h"

// std
#include <algorithm>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

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
  // One decision for both stores. The input enforces this same bound before calling, so it cannot fire
  // for a typed line -- but gating only the file left a line recallable in this session and absent from
  // the next, and which store keeps what should not rest on a caller's discipline.
  if (line.size() > maxLineBytes)
  {
    return;
  }

  if (!history_.empty() && history_.front() == line)
  {
    return;
  }

  history_.emplace_front(line);

  while (history_.size() > maxHistoryLines)
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
    else if (!historyWriteFailureReported_)
    {
      // Once per session, not once per line. A read-only HOME used to mean history quietly never
      // persisted: it worked all session and was empty on the next start, with nothing said.
      historyWriteFailureReported_ = true;
      getLogger()->warn("cannot write the command history to {}; it will not persist beyond this session",
                        historyFile_.string());
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
    // A file that is not there yet is the normal first run and says nothing. One that exists and will
    // not open is worth a line, because the symptom otherwise is history that is silently empty.
    std::error_code exists;
    if (std::filesystem::exists(historyFile_, exists))
    {
      getLogger()->warn("cannot read the command history from {}; starting with an empty history",
                        historyFile_.string());
    }
    return;
  }

  // A ring of the last maxHistoryLines, not the whole file. Within a session `addToHistory` appends
  // without bound -- only the in-memory deque is capped -- so the file carries every command of every
  // previous session. A scripted run of 125,000 commands at the line limit leaves about a gigabyte, and
  // this used to materialise all of it before capping, on the component thread during init.
  std::deque<std::string> fileLines;
  bool trimmedTheFile = false;
  std::string line;
  while (std::getline(in, line))
  {
    // A carriage return left by another writer survives getline, and the renderer hides it while the
    // parser keeps it: the recalled line then fails as an unrecognised command with nothing on screen
    // to explain why. Drop the control characters, and skip a line too long to have been typed here.
    line.erase(std::remove_if(line.begin(), line.end(), [](char c) { return static_cast<unsigned char>(c) < 0x20U; }),
               line.end());
    if (!line.empty() && line.size() <= maxLineBytes)
    {
      fileLines.push_back(std::move(line));
      if (fileLines.size() > maxHistoryLines)
      {
        fileLines.pop_front();
        trimmedTheFile = true;
      }
    }
    line.clear();
  }
  in.close();

  if (trimmedTheFile)
  {
    // Rewrite it at the cap, so the next start does not read past it again.
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
    if (history_.size() > maxHistoryLines)
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

}  // namespace sen::components::term
