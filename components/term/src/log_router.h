// === log_router.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_LOG_ROUTER_H
#define SEN_COMPONENTS_TERM_SRC_LOG_ROUTER_H

// local
#include "log_sink.h"

// sen
#include "sen/core/base/compiler_macros.h"

// spdlog
#include <spdlog/common.h>

// std
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace sen::components::term
{

// forward declarations
class App;

/// Manages the injection of a custom spdlog sink into all registered loggers.
/// Periodically scans for new loggers and injects the sink.
/// Provides commands for controlling log verbosity.
class LogRouter final
{
  SEN_NOCOPY_NOMOVE(LogRouter)

public:
  explicit LogRouter(App& app);
  ~LogRouter();

  /// Scan for new loggers and inject the sink, and render any messages that were
  /// buffered from other threads since the last call. Call periodically from the
  /// term's update loop, never from a background thread.
  void update();

  /// Set the global log level (applies to all loggers).
  void setGlobalLevel(spdlog::level::level_enum level);

  /// Set the log level for a specific logger.
  /// Returns false if the logger is not found.
  bool setLoggerLevel(std::string_view loggerName, spdlog::level::level_enum level);

  /// Get the current global log level.
  [[nodiscard]] spdlog::level::level_enum getGlobalLevel() const noexcept;

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

  /// True if update() injected new loggers since the last call. Resets on read.
  [[nodiscard]] bool hasNewLoggers() noexcept;

private:
  struct PendingMessage
  {
    spdlog::level::level_enum level;
    std::string text;
  };

  void renderMessage(spdlog::level::level_enum level, const std::string& message);

  App& app_;
  std::shared_ptr<TermLogSink> sink_;
  std::set<std::string> injectedLoggers_;
  spdlog::level::level_enum globalLevel_ = spdlog::level::info;
  bool newLoggersInjected_ = true;

  // Messages arrive via the sink from any thread; drained onto the UI from the
  // term thread inside update().
  std::mutex pendingMutex_;
  std::vector<PendingMessage> pendingMessages_;
};

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_LOG_ROUTER_H
