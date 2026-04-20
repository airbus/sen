// === log_router.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "log_router.h"

// local
#include "app.h"
#include "styles.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// spdlog
#include <spdlog/spdlog.h>

// std
#include <algorithm>

namespace sen::components::term
{

LogRouter::LogRouter(App& app): app_(app)
{
  sink_ = std::make_shared<TermLogSink>(
    [this](spdlog::level::level_enum level, const std::string& message)
    {
      // The sink fires on whichever thread is logging (spdlog is per-caller). UI mutation
      // must happen on the term thread, so we only buffer here; update() drains the buffer
      // on the term thread.
      std::lock_guard lock(pendingMutex_);
      pendingMessages_.push_back({level, message});
    });

  update();
}

void LogRouter::renderMessage(spdlog::level::level_enum level, const std::string& message)
{
  ftxui::Decorator style;

  switch (level)
  {
    case spdlog::level::trace:
      style = styles::logTrace();
      break;
    case spdlog::level::debug:
      style = styles::logDebug();
      break;
    case spdlog::level::info:
      style = styles::logInfo();
      break;
    case spdlog::level::warn:
      style = styles::logWarn();
      break;
    case spdlog::level::err:
      style = styles::logError();
      break;
    case spdlog::level::critical:
      style = styles::logCritical();
      break;
    default:
      style = ftxui::nothing;
      break;
  }

  // Color only the level tag (e.g. "[info]"), leave the rest in default foreground.
  // The formatted message starts with the spdlog pattern, which typically includes
  // "[timestamp] [level] message". Find the level tag and color it.
  auto sv = spdlog::level::to_string_view(level);
  auto levelStr = std::string(sv.data(), sv.size());
  auto tagStart = message.find('[' + levelStr + ']');
  if (tagStart != std::string::npos)
  {
    auto tagEnd = tagStart + levelStr.size() + 2;  // +2 for []
    ftxui::Elements parts;
    parts.push_back(ftxui::text(message.substr(0, tagStart)));
    parts.push_back(ftxui::text(message.substr(tagStart, tagEnd - tagStart)) | style);
    parts.push_back(ftxui::text(message.substr(tagEnd)));
    app_.appendLogElement(ftxui::hbox(std::move(parts)));
  }
  else
  {
    app_.appendLogElement(ftxui::paragraph(message) | style);
  }
}

LogRouter::~LogRouter()
{
  spdlog::apply_all(
    [this](const std::shared_ptr<spdlog::logger>& logger)
    {
      auto& sinks = logger->sinks();
      sinks.erase(std::remove(sinks.begin(), sinks.end(), sink_), sinks.end());
    });
}

void LogRouter::update()
{
  std::vector<PendingMessage> drained;
  {
    std::lock_guard lock(pendingMutex_);
    drained.swap(pendingMessages_);
  }
  for (const auto& msg: drained)
  {
    renderMessage(msg.level, msg.text);
  }

  spdlog::apply_all(
    [this](const std::shared_ptr<spdlog::logger>& logger)
    {
      auto name = logger->name();

      if (injectedLoggers_.count(name) > 0)
      {
        return;
      }

      // Replace the logger's sinks with our TermLogSink so all output funnels into the
      // log pane. Existing stdout/stderr sinks would otherwise corrupt FTXUI rendering.
      auto level = logger->level();
      logger->sinks().clear();
      logger->sinks().push_back(sink_);
      logger->set_level(level);
      injectedLoggers_.insert(name);
      newLoggersInjected_ = true;
    });
}

bool LogRouter::hasNewLoggers() noexcept
{
  bool result = newLoggersInjected_;
  newLoggersInjected_ = false;
  return result;
}

void LogRouter::setGlobalLevel(spdlog::level::level_enum level)
{
  globalLevel_ = level;
  spdlog::set_level(level);

  spdlog::apply_all([level](const std::shared_ptr<spdlog::logger>& logger) { logger->set_level(level); });
}

bool LogRouter::setLoggerLevel(std::string_view loggerName, spdlog::level::level_enum level)
{
  auto logger = spdlog::get(std::string(loggerName));
  if (!logger)
  {
    return false;
  }
  logger->set_level(level);
  return true;
}

spdlog::level::level_enum LogRouter::getGlobalLevel() const noexcept { return globalLevel_; }

std::vector<LogRouter::LoggerInfo> LogRouter::listLoggers() const
{
  std::vector<LoggerInfo> result;
  spdlog::apply_all([&result](const std::shared_ptr<spdlog::logger>& logger)
                    { result.push_back({logger->name(), logger->level()}); });

  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
  return result;
}

bool LogRouter::parseLevel(std::string_view name, spdlog::level::level_enum& level)
{
  if (name == "trace")
  {
    level = spdlog::level::trace;
  }
  else if (name == "debug")
  {
    level = spdlog::level::debug;
  }
  else if (name == "info")
  {
    level = spdlog::level::info;
  }
  else if (name == "warn" || name == "warning")
  {
    level = spdlog::level::warn;
  }
  else if (name == "error" || name == "err")
  {
    level = spdlog::level::err;
  }
  else if (name == "critical")
  {
    level = spdlog::level::critical;
  }
  else if (name == "off")
  {
    level = spdlog::level::off;
  }
  else
  {
    return false;
  }
  return true;
}

}  // namespace sen::components::term
