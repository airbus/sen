// === log_router.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "log_router.h"

// component
#include "app.h"
#include "styles.h"

// sen
#include "sen/kernel/component_api.h"

// ftxui
#include <ftxui/dom/elements.hpp>

// spdlog
#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/stdout_sinks.h>
#ifdef _WIN32
#  include <spdlog/sinks/wincolor_sink.h>
#endif
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

namespace
{

/// True for the sinks that write to the process's own stdout or stderr. Those are the only ones the
/// terminal has to take over; file and syslog sinks belong to whoever configured them.
bool isConsoleSink(const std::shared_ptr<spdlog::sinks::sink>& sink)
{
  // Cast to the base templates, not the eight concrete typedefs. stdout_sink_base is the base of
  // both stdout_sink and stderr_sink, and ansicolor_sink of both colour variants, so four casts
  // cover all eight. Naming the _mt typedefs alone missed the _st ones, which spdlog_config creates
  // for any YAML sink carrying singleThreaded -- and a missed stdout sink writes to fd 1, which
  // FTXUI owns. A dist_sink or dup_filter_sink wrapping a console sink is still not seen; nothing in
  // the kernel creates one.
  auto* raw = sink.get();
  return dynamic_cast<spdlog::sinks::stdout_sink_base<spdlog::details::console_mutex>*>(raw) != nullptr ||
         dynamic_cast<spdlog::sinks::stdout_sink_base<spdlog::details::console_nullmutex>*>(raw) != nullptr
#ifdef _WIN32
         || dynamic_cast<spdlog::sinks::wincolor_sink<spdlog::details::console_mutex>*>(raw) != nullptr ||
         dynamic_cast<spdlog::sinks::wincolor_sink<spdlog::details::console_nullmutex>*>(raw) != nullptr;
#else
         || dynamic_cast<spdlog::sinks::ansicolor_sink<spdlog::details::console_mutex>*>(raw) != nullptr ||
         dynamic_cast<spdlog::sinks::ansicolor_sink<spdlog::details::console_nullmutex>*>(raw) != nullptr;
#endif
}

}  // namespace

LogRouter::~LogRouter()
{
  // Detach before removing anything. A logger holds the sink through a shared_ptr, so it outlives this
  // object; until it is detached, a message logged through a logger the sweep below misses would reach
  // a destroyed mutex and a destroyed vector.
  if (sink_)
  {
    sink_->detach();
  }

  // Term's OWN registry, because that is where `update` put the sink. This used to walk the kernel's,
  // which is a different object (F-263), so it removed the sink from loggers that never had it and left
  // it on every logger that did.
  //
  // Nothing here may throw: this is a destructor, and `apply_all` takes a std::function, whose
  // construction allocates. Failing to detach a logger is survivable now that the sink is inert.
  try
  {
    spdlog::apply_all(
      [this](const std::shared_ptr<spdlog::logger>& logger)
      {
        auto& sinks = logger->sinks();
        sinks.erase(std::remove(sinks.begin(), sinks.end(), sink_), sinks.end());
      });
  }
  catch (...)  // NOLINT(bugprone-empty-catch) -- a destructor has nowhere to report, and the sink is inert
  {
  }
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

  // Deliberately term's OWN registry and not the kernel's, which is a different object -- see F-263.
  // spdlog's logger::sink_it_ iterates sinks_ by reference with no lock and logger::sinks() hands out
  // a bare reference, so mutating that vector while another thread logs through it is a
  // use-after-free. Sweeping the kernel's registry made every other component's logger a live
  // participant in that race, thirty times a second; term's own registry has no other writer.
  //
  // The cost is that another component's console sink still writes to fd 1, which FTXUI owns, so its
  // output can disturb the display. That is a pre-existing nuisance and the lesser one. Capturing
  // fd 1 the way OutputCapture captures fd 2 would remove the need to touch sinks at all, but FTXUI
  // renders to stdout -- libftxui-screen references std::cout and nothing references cerr -- so
  // capturing it would swallow the interface. Doing this safely needs the kernel to own the
  // injection at logger-creation time.
  // Nothing in this callback may log -- see the rule on LogRouter in the header.
  spdlog::apply_all(
    [this](const std::shared_ptr<spdlog::logger>& logger)
    {
      auto name = logger->name();

      // Re-check rather than remembering which loggers have been done, which keeps the sweep
      // idempotent and costs one find over a registry with a handful of entries.
      //
      // It used to say this defends against spdlog_config replacing a logger's whole sink vector. It
      // cannot: spdlog_config is compiled into libkernel and called only from KernelImpl::configure,
      // so it works on the kernel's registry, and this sink is only ever added to term's own. The two
      // never meet -- which is also why nothing the kernel does at crash time can drop this sink.
      auto& sinks = logger->sinks();
      if (std::find(sinks.begin(), sinks.end(), sink_) != sinks.end())
      {
        return;
      }

      // stdout sinks write to fd 1, which FTXUI owns, so they have to go. stderr sinks are
      // already coming back through OutputCapture's pipe, so they would only double-print. File
      // and syslog sinks are left alone: clearing them stopped the host's own logging.
      sinks.erase(std::remove_if(sinks.begin(), sinks.end(), isConsoleSink), sinks.end());
      sinks.push_back(sink_);
      if (seenLoggers_.insert(name).second)
      {
        newLoggersInjected_ = true;
      }

      // Only once the user has actually asked for a level. Applying globalLevel_ unconditionally
      // reset every logger the host had configured from YAML to info the moment term loaded, which
      // silenced anything set to trace or debug and made anything set to warn noisier.
      if (levelSetByUser_)
      {
        logger->set_level(globalLevel_);
      }
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
  // Only the kernel's route is used. spdlog::set_level() would reach the component's own registry,
  // which holds nothing the user cares about, while reading as though it set the level everywhere.
  globalLevel_ = level;
  levelSetByUser_ = true;

  kernel::KernelApi::applyToAllLoggers([level](const std::shared_ptr<spdlog::logger>& logger)
                                       { logger->set_level(level); });
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

spdlog::level::level_enum LogRouter::getGlobalLevel() const noexcept { return globalLevel_; }

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
  // spdlog::level::from_str accepts exactly the set this used to spell out by hand -- the canonical
  // names plus "warn" and "err" -- and returns off for anything it does not know, so the only thing
  // left to do is tell "off" apart from "unrecognised". The out-param is left alone on failure:
  // assigning it unconditionally changed the contract, and log_router_test asserts it does not.
  const auto parsed = spdlog::level::from_str(std::string(name));
  if (parsed == spdlog::level::off && name != "off")
  {
    return false;
  }
  level = parsed;
  return true;
}

}  // namespace sen::components::term
