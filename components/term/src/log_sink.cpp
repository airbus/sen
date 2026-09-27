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

  // There was a second write here, straight to the saved stderr for anything at error or above, on
  // the grounds that such a message may be the last thing the process does and nothing drains this
  // buffer afterwards. It is gone, for three reasons, each of which is enough on its own.
  //
  // It did not deliver what it was for. `Runner::toErrorState` logs an explanation and then calls
  // `dump_backtrace()`, and spdlog re-sinks the buffered lines with their original levels between two
  // markers it emits at `info` (logger-inl.h:155-163). So the markers and every buffered line below
  // `err` -- which is the whole reason to keep a backtrace -- failed the test, and the only lines that
  // passed were ones that would have passed anyway.
  //
  // The kernel now covers the case better. `KernelImpl::configure` arms a trace-level log ring on
  // every logger (`crash_reporter.cpp`, via `captureLogs`), and the terminate handler writes those
  // lines into the minidump beside the phase, the version and the loaded components. That survives the
  // process; a line flashed onto an alternate screen about to be torn down does not. The ring also
  // survives this component's sink sweep, which removes console sinks only.
  //
  // And the guard was severity, not "the process is dying", so every recoverable `err` from any
  // component wrote a raw line onto the alternate screen outside FTXUI's model of it. `err` is not
  // fatal. Note the one case not covered: with `crashReportDisabled` the ring is never armed, so a
  // dying process keeps its last lines only in this pane -- and that host has no dump either.
  // base_sink::log takes a non-recursive mutex and then calls this, so a callback that logs would
  // re-enter on the same thread and lock it twice -- undefined behaviour, in practice a permanent hang
  // of whichever thread happened to log, which could be a kernel dispatcher worker rather than term's.
  // The sink is on every logger, so anything term calls from here is one edit away from that. Dropping
  // the re-entrant line is the wrong output; hanging a kernel thread is a wedged process.
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
