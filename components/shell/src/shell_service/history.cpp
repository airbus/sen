// === history.cpp =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "history.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/io/util.h"

// std
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <list>
#include <string>
#include <string_view>
#include <utility>

namespace sen::components::shell
{

namespace
{

void openFile(const std::filesystem::path& path, std::fstream& stream)
{
  stream.open(path, std::ios::in | std::ios::app);
  if (!stream.is_open())
  {
    // no file, try to create it
    stream.clear();
    stream.open(path, std::ios::out);
    stream.close();
    stream.open(path, std::ios::in | std::ios::app);
  }
}

}  // namespace

History::History()
{
#ifdef _WIN32
  char* pValue = nullptr;
  size_t len = 0;
  _dupenv_s(&pValue, &len, "USERPROFILE");
  if (pValue != nullptr)
  {
    currentFileName_ = pValue;
    free(pValue);
  }
#else
  currentFileName_ = getenv("HOME");
#endif

  currentFileName_ = currentFileName_ / ".sen_history.txt";
  currentLine_ = lines_.end();
}

void History::load()
{
  // open or crate any existing file
  std::fstream stream;
  openFile(currentFileName_, stream);
  if (!stream.is_open())
  {
    std::string err;
    err.append("could not open the shell history file '");
    err.append(currentFileName_.string());
    err.append("'");
    throwRuntimeError(err);
  }

  // read contents
  std::string data;
  data.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());

  // populate our memory
  auto tokens = ::sen::impl::split(data, '\n');
  for (auto& elem: tokens)
  {
    if (!elem.empty())
    {
      lines_.push_back(std::move(elem));
    }
  }

  stream.close();
}

void History::addLine(std::string_view line)
{
  if (line.empty())
  {
    return;
  }

  // do not allow two repeated consecutive lines
  if (!lines_.empty() && lines_.back() == line)
  {
    currentLine_ = lines_.end();
    return;
  }

  if (lines_.size() > maxHistorySize)
  {
    lines_.pop_front();
  }

  lines_.emplace_back(line);

  // write to file
  // note: we open it every time (even if it is overkill) to
  //       allow for multiple shells to concurrently write it
  std::fstream stream;
  openFile(currentFileName_, stream);
  if (stream.is_open())
  {
    stream << line << '\n';
    stream.flush();
  }

  currentLine_ = lines_.end();
}

std::string History::nextLine()
{
  // try to advance the line
  if (currentLine_ != lines_.end())
  {
    currentLine_++;
  }

  if (currentLine_ != lines_.end())
  {
    return *currentLine_;
  }
  return {};
}

std::string History::prevLine()
{
  if (currentLine_ == lines_.end())
  {
    if (!lines_.empty())
    {
      currentLine_--;
      return (*currentLine_);
    }
    return {};
  }

  if (!lines_.empty())
  {
    if (currentLine_ != lines_.begin())
    {
      currentLine_--;
    }

    return (*currentLine_);
  }

  return {};
}

const std::list<std::string>& History::getLines() const { return lines_; }

}  // namespace sen::components::shell
