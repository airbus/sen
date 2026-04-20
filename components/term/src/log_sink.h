// === log_sink.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_LOG_SINK_H
#define SEN_COMPONENTS_TERM_SRC_LOG_SINK_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/move_only_function.h"

// spdlog
#include <spdlog/common.h>
#include <spdlog/sinks/base_sink.h>

// std
#include <mutex>
#include <string>

namespace sen::components::term
{

using LogCallback = sen::std_util::move_only_function<void(spdlog::level::level_enum, const std::string&)>;

/// A custom spdlog sink that forwards log messages to the term output pane.
class TermLogSink final: public spdlog::sinks::base_sink<std::mutex>
{
  SEN_NOCOPY_NOMOVE(TermLogSink)

public:
  explicit TermLogSink(LogCallback callback);

protected:
  void sink_it_(const spdlog::details::log_msg& msg) override;
  void flush_() override;

private:
  LogCallback callback_;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_LOG_SINK_H
