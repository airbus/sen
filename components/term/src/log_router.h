// === log_router.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_LOG_ROUTER_H
#define SEN_COMPONENTS_TERM_SRC_LOG_ROUTER_H

// component
#include "log_sink.h"

// sen
#include "sen/core/base/compiler_macros.h"

// spdlog
#include <spdlog/common.h>

// std
#include <cstddef>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace sen::components::term
{

// forward declarations
class App;

/// Draws the kernel's log lines in term's output area, and carries the commands that change the level.
///
/// It registers one sink with the kernel, which puts it behind the relay sink that sits on every logger. It
/// does not sweep the registry: inside this shared object `spdlog::apply_all` walks term's own registry
/// rather than the kernel's, so a sweep would reach almost nothing.
///
/// Nothing inside a registry walk in this class may log. `KernelApi::applyToAllLoggers` runs under
/// spdlog's logger-map mutex, which is not recursive, and `getLogger()` reaches `registry::get`, which
/// takes the same one. A single log line inside one of those callbacks deadlocks the thread that owns
/// the display. None of them logs today, and `TermLogSink::sink_it_` does not either; the hazard is one
/// edit away, and term has `getLogger()` conveniently in scope.
class LogRouter final
{
  SEN_NOCOPY_NOMOVE(LogRouter)

public:
  explicit LogRouter(App& app);
  ~LogRouter();

  /// Render whatever the sink has queued since the last call. The sink is written from the threads that
  /// log; this drains that queue on the thread that draws. Call it from term's update loop, never from
  /// a background thread.
  void update();

  /// Set the global log level (applies to all loggers).
  /// Returns false if the kernel refused the level, which only an out-of-range value can cause.
  bool setGlobalLevel(spdlog::level::level_enum level);

  /// Set the log level for a specific logger.
  /// Returns false if the logger is not found.
  bool setLoggerLevel(std::string_view loggerName, spdlog::level::level_enum level);

  /// The level every logger is at, read from the kernel rather than kept here. A copy beside the
  /// setter was never assigned, so `log` reported `info` whatever the user had set.
  [[nodiscard]] static spdlog::level::level_enum getGlobalLevel() noexcept;

  /// List all known loggers with their current levels.
  struct LoggerInfo
  {
    std::string name;
    spdlog::level::level_enum level;
  };
  [[nodiscard]] std::vector<LoggerInfo> listLoggers() const;

  /// Parse a level name string (trace, debug, info, warn, error, critical, off).
  /// Returns true on success.
  static bool parseLevel(std::string_view name, spdlog::level::level_enum& level);

private:
  struct PendingMessage
  {
    spdlog::level::level_enum level;
    std::string text;
  };

  struct Batch
  {
    std::vector<PendingMessage> messages;
    std::size_t dropped = 0;
  };

  void renderMessage(spdlog::level::level_enum level, const std::string& message);

  /// Queue one line from the thread that logged it, or count it as dropped if the queue is full.
  void enqueue(spdlog::level::level_enum level, const std::string& message);

  /// Take at most one tick's worth off the queue, with the number dropped since the last call.
  [[nodiscard]] Batch takeBatch();

  App& app_;
  std::shared_ptr<TermLogSink> sink_;

  // Messages arrive via the sink from any thread; drained onto the UI from the term thread inside
  // update(). Both bounds matter, and neither existed: `log level debug` on a kernel with live traffic
  // makes the bus loggers emit on almost every message, so the queue was an unbounded accumulator
  // that the render tick then tried to draw in one frame. The user who turned the level up to diagnose
  // something lost the tool they were diagnosing with.
  std::mutex pendingMutex_;
  std::vector<PendingMessage> pendingMessages_;
  std::size_t droppedMessages_ = 0;

  // Test support
  friend class LogRouterTestAccess;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_LOG_ROUTER_H
