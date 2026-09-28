// === log_sink.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "log_sink.h"

// spdlog
#include <spdlog/common.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>

// std
#include <mutex>
#include <string>
#include <utility>

namespace sen::components::term
{

TermLogSink::TermLogSink(LogCallback callback): callback_(std::move(callback)) {}

void TermLogSink::sink_it_(const spdlog::details::log_msg& msg)
{
  spdlog::memory_buf_t formatted;
  formatter_->format(msg, formatted);

  std::string text(formatted.data(), formatted.size());
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
  {
    text.pop_back();
  }

  // One write, into the pane. A message at error or above is not sent anywhere else from here: the kernel
  // arms a trace-level log ring on every logger (`crash_reporter.cpp`), and the terminate handler writes
  // those lines into the minidump, which survives the process where a line on a torn-down alternate screen
  // does not. With `crashReportDisabled` there is no ring, and then this pane holds the only copy.
  //
  // base_sink::log takes a non-recursive mutex and then calls this, so a callback that logs would lock it
  // twice on the same thread and hang whichever thread emitted, which may be a kernel worker rather than
  // term's. Dropping the re-entrant line is the lesser failure.
  static thread_local bool inCallback = false;
  if (inCallback || !callback_)
  {
    return;
  }

  inCallback = true;
  try
  {
    callback_(msg.level, text);
  }
  catch (...)
  {
    inCallback = false;
    throw;
  }
  inCallback = false;
}

void TermLogSink::detach()
{
  const std::lock_guard lock(base_sink<std::mutex>::mutex_);
  callback_ = nullptr;
}

void TermLogSink::flush_() {}

}  // namespace sen::components::term
