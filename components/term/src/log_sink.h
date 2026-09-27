// === log_sink.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_LOG_SINK_H
#define SEN_COMPONENTS_TERM_SRC_LOG_SINK_H

// sen
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
///
/// **The callback runs under this sink's own non-recursive mutex, on whichever thread logged, and must
/// not log.** `base_sink::log` takes the mutex and then calls `sink_it_`, so a log line from inside the
/// callback locks it twice on the same thread. The sink is attached to every logger in the process, so
/// the thread that hangs would be whichever one emitted -- a kernel dispatcher worker, or another
/// component's -- while term carries on drawing, which makes it look as though something else stopped.
/// `sink_it_` drops a re-entrant line rather than deadlocking, but the rule is still the rule: do the
/// least possible here and never call back into anything that logs.
///
/// This is the same class of trap as the registry-walk rule in `log_router.h`, and it was the one of
/// the two that was not written down.
class TermLogSink final: public spdlog::sinks::base_sink<std::mutex>
{
  SEN_NOCOPY_NOMOVE(TermLogSink)

public:
  explicit TermLogSink(LogCallback callback);
  ~TermLogSink() override = default;

  /// Stop calling the callback. A logger holds this sink through a `shared_ptr`, so it can outlive the
  /// object the callback reaches into; detaching first means a logger the teardown sweep misses calls
  /// nothing instead of freed state. Takes the sink's own mutex, so it is safe against a thread
  /// logging through it.
  void detach();

protected:
  void sink_it_(const spdlog::details::log_msg& msg) override;
  void flush_() override;

private:
  LogCallback callback_;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_LOG_SINK_H
