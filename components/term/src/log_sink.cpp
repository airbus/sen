// === log_sink.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "log_sink.h"

// spdlog
#include <spdlog/details/log_msg.h>
#include <spdlog/pattern_formatter.h>

// std
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

  callback_(msg.level, text);
}

void TermLogSink::flush_() {}

}  // namespace sen::components::term
