// === log_router.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "log_router.h"

// component
#include "app.h"
#include "log_sink.h"
#include "styles.h"
#include "util.h"

// sen
#include "sen/kernel/component_api.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// spdlog
#include <spdlog/common.h>
#include <spdlog/logger.h>

// std
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sen::components::term
{

namespace
{

// The queue the logging threads fill. At 30 Hz and 500 rendered per tick this is four seconds of
// backlog, which is long enough to ride out a burst and short enough that the memory is bounded.
constexpr std::size_t maxPendingMessages = 2000;

// Lines rendered in one tick. Each one builds an ftxui element, so this is the frame-time bound.
constexpr std::size_t maxRenderedPerTick = 500;

}  // namespace

LogRouter::LogRouter(App& app): app_(app)
{
  sink_ = std::make_shared<TermLogSink>(
    [this](spdlog::level::level_enum level, const std::string& message)
    {
      // The sink fires on whichever thread is logging (spdlog is per-caller). UI mutation
      // must happen on the term thread, so we only buffer here; update() drains the buffer
      // on the term thread.
      enqueue(level, message);
    });

  // The pane's own pattern, not the logger's. Every row the output area draws already carries a time on
  // the right, so a full ISO date in the text would waste a third of a narrow terminal.
  sink_->set_pattern("[%n] [%l] %v");

  // Registered once with the kernel, which is the only safe way to reach the loggers. `spdlog::apply_all`
  // inside this shared object walks term's own registry, not the kernel's where every component's logger
  // lives, and appending to a logger another thread is emitting through is a use-after-free. The kernel
  // attaches the sink where neither applies. `owned` also strips console sinks, which write to the
  // descriptors FTXUI is drawing on.
  auto registration = kernel::KernelApi::addLoggerSink(sink_, kernel::KernelApi::TerminalOwnership::owned);
  if (registration.isError())
  {
    // Nothing draws logs if this failed, and the console sinks are still live, so say so where the user
    // will see it: on the console, which is still working precisely because the claim did not happen.
    getLogger()->error("term could not register its log sink: {}. Log lines will not appear in the term.",
                       registration.getError().explanation);
  }
  else if (registration.getValue().terminalOwnedElsewhere)
  {
    // The registration is not arbitrated: two components that both render logs each get every line.
    getLogger()->warn("another component already claimed the terminal for log output; both will draw.");
  }

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
  // Detach before anything else. A logger holds the sink through a shared_ptr, so it outlives this
  // object; until it is inert, a message logged through a logger the removal misses would reach a
  // destroyed mutex and a destroyed vector.
  if (sink_)
  {
    sink_->detach();
  }

  // Hand it back to the kernel, which takes it off every logger that has it and stops it being given
  // to new ones. Nothing here may throw: this is a destructor, and the call allocates a std::function.
  try
  {
    std::ignore = kernel::KernelApi::removeLoggerSink(sink_);
  }
  catch (...)  // NOLINT(bugprone-empty-catch) nowhere to report, and the sink is already inert
  {
  }
}

void LogRouter::enqueue(spdlog::level::level_enum level, const std::string& message)
{
  // Runs on whichever thread logged. Dropping is the only bound available: blocking would stall the
  // component that emitted, and an unbounded queue grows without limit under `log level debug` on a
  // kernel with live traffic, where the bus loggers emit on nearly every message.
  std::lock_guard lock(pendingMutex_);
  if (pendingMessages_.size() >= maxPendingMessages)
  {
    ++droppedMessages_;
    return;
  }
  pendingMessages_.push_back({level, message});
}

LogRouter::Batch LogRouter::takeBatch()
{
  // A budget per tick, not the whole queue: rendering everything that arrived built an element per
  // line, so a burst became a frame longer than the burst and the next tick started further behind.
  Batch batch;
  std::lock_guard lock(pendingMutex_);
  const auto take = static_cast<std::ptrdiff_t>(std::min(pendingMessages_.size(), maxRenderedPerTick));
  batch.messages.assign(std::make_move_iterator(pendingMessages_.begin()),
                        std::make_move_iterator(pendingMessages_.begin() + take));
  pendingMessages_.erase(pendingMessages_.begin(), pendingMessages_.begin() + take);
  batch.dropped = std::exchange(droppedMessages_, std::size_t {0});
  return batch;
}

void LogRouter::update()
{
  auto batch = takeBatch();
  for (const auto& msg: batch.messages)
  {
    renderMessage(msg.level, msg.text);
  }

  if (batch.dropped > 0)
  {
    app_.appendInfo(std::to_string(batch.dropped) +
                    " log lines were dropped: they arrived faster than the "
                    "screen can draw them. Lower the log level to see them all.");
  }
}

bool LogRouter::setGlobalLevel(spdlog::level::level_enum level)
{
  // The kernel's registry level, not a walk over the loggers that happen to exist: a walk stopped
  // applying the moment another component made a logger, and `spdlog::set_level()` here would reach
  // this shared object's own registry while reading as though it set the level everywhere.
  return kernel::KernelApi::setAllLoggersLevel(level).isOk();
}

bool LogRouter::setLoggerLevel(std::string_view loggerName, spdlog::level::level_enum level)
{
  // spdlog::get() reads the component's own registry, which is not the one the kernel registers
  // loggers in, so match by name over the kernel's view instead.
  bool found = false;
  kernel::KernelApi::applyToAllLoggers(
    [&](const std::shared_ptr<spdlog::logger>& logger)
    {
      if (logger->name() == loggerName)
      {
        logger->set_level(level);
        found = true;
      }
    });
  return found;
}

spdlog::level::level_enum LogRouter::getGlobalLevel() noexcept { return kernel::KernelApi::getAllLoggersLevel(); }

std::vector<LogRouter::LoggerInfo> LogRouter::listLoggers() const
{
  std::vector<LoggerInfo> result;
  kernel::KernelApi::applyToAllLoggers([&result](const std::shared_ptr<spdlog::logger>& logger)
                                       { result.push_back({logger->name(), logger->level()}); });

  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
  return result;
}

bool LogRouter::parseLevel(std::string_view name, spdlog::level::level_enum& level)
{
  // spdlog::level::from_str accepts the canonical names plus "warn" and "err", and returns off for
  // anything it does not know, so the only thing left to do is tell "off" apart from "unrecognised". The
  // out-param is left alone on failure, which log_router_test asserts.
  //
  // Case is folded first: from_str only knows the lower-case spellings, and `log level INFO` is what a
  // user types when the levels are printed in upper case beside every line.
  std::string lowered(name);
  std::transform(lowered.begin(),
                 lowered.end(),
                 lowered.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  const auto parsed = spdlog::level::from_str(lowered);
  if (parsed == spdlog::level::off && lowered != "off")
  {
    return false;
  }
  level = parsed;
  return true;
}

}  // namespace sen::components::term
