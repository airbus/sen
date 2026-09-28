// === component_api_impl.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// implementation
#include "./crash_reporter.h"
#include "./kernel_impl.h"
#include "bus/session.h"

// sen
#include "bus/remote_participant.h"  // NOLINT(misc-include-cleaner) false positive
#include "sen/core/base/assert.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/move_only_function.h"
#include "sen/core/base/mutex_utils.h"
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/detail/kernel_fwd.h"
#include "sen/kernel/tracer.h"
#include "sen/kernel/transport.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/network_footprint.stl.h"

// spdlog
#include <spdlog/common.h>
#include <spdlog/details/console_globals.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/details/registry.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

// std
#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/sinks/stdout_sinks.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sen::kernel
{

namespace
{

[[nodiscard]] bool isValidNonLocalBusAddress(const BusAddress& address) noexcept
{
  return !address.sessionName.empty() && !address.busName.empty() && address.sessionName != impl::localSessionName;
}

[[nodiscard]] std::optional<BusAddress> tryParseBusAddress(const std::string& address)
{
  const auto tokens = ::sen::impl::split(address, '.');
  if (tokens.size() != 2U)
  {
    return std::nullopt;
  }
  return BusAddress {tokens.at(0U), tokens.at(1U)};
}

void addUniqueBusAddress(std::vector<BusAddress>& busAddresses, const BusAddress& address)
{
  if (!isValidNonLocalBusAddress(address))
  {
    return;
  }

  const auto alreadyAdded = std::find(busAddresses.begin(), busAddresses.end(), address) != busAddresses.end();
  if (!alreadyAdded)
  {
    busAddresses.push_back(address);
  }
}

void addUniqueBusAddress(std::vector<BusAddress>& busAddresses, const std::string& address)
{
  if (const auto parsedAddress = tryParseBusAddress(address))
  {
    addUniqueBusAddress(busAddresses, *parsedAddress);
  }
}

}  // namespace

namespace impl
{

namespace
{
/// Defined further down, beside the relay sink it attaches. Declared here because `getOrCreateLogger`
/// is above that point and the two anonymous namespace blocks in this file are one namespace.
void attachLoggerRelay(const std::shared_ptr<spdlog::logger>& logger);
}  // namespace

void installTransportFactory(KernelImpl* kernel, TransportFactory&& factory, uint32_t transportVersion)
{
  kernel->getSessionManager().setTransportFactory(std::move(factory), transportVersion);
}

CustomTypeRegistry& getTypes(KernelImpl* kernel) noexcept { return kernel->getTypes(); }

std::shared_ptr<ObjectSource> getSource(Runner* runner, const BusAddress& address)
{
  return runner->getOrCreateLocalParticipant(address);
}

void remoteProcessDetected(RunApi& api, const ProcessInfo& processInfo)
{
  api.kernelImpl_.getSessionManager().remoteProcessDetected(processInfo);
}

void remoteProcessLost(RunApi& api, const ProcessInfo& processInfo)
{
  api.kernelImpl_.getSessionManager().remoteProcessLost(processInfo);
}

SessionsDiscoverer& getSessionsDiscoverer(Runner* runner) { return runner->getSessionsDiscoverer(); }

const ProcessInfo* fetchOwnerInfo(const Object* object)
{
  auto* proxy = object->asProxyObject();
  if (proxy == nullptr)
  {
    return nullptr;
  }

  auto* remote = proxy->asRemoteObject();
  if (remote == nullptr)
  {
    return nullptr;
  }

  return &remote->getParticipant().lock()->getProcessInfo();
}

void drainInputs(Runner* runner) { runner->drainInputs(); }

void commit(Runner* runner) { runner->commit(); }

void update(Runner* runner) { runner->update(); }

FuncResult execLoop(Runner* runner, Duration cycleTime, std::function<void()>&& workFunction, bool logOverruns)
{
  return runner->execLoop(cycleTime, std::move(workFunction), logOverruns);
}

::sen::impl::WorkQueue* getWorkQueue(Runner* runner) { return runner->getWorkQueue(); }

std::shared_ptr<spdlog::logger> getOrCreateLogger(const std::string& loggerName)
{
  if (auto existing = spdlog::get(loggerName))
  {
    return existing;
  }

  // The ring goes on before registration: once the logger is in the registry another thread can
  // resolve it by name and be walking its sink vector, which spdlog does without a lock.
  auto logger = std::make_shared<spdlog::logger>(loggerName, std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
  CrashReporter::get().captureLogsFrom(logger);
  attachLoggerRelay(logger);

  try
  {
    // Registers it, and applies the pattern, level and backlog the registry is configured with.
    spdlog::details::registry::instance().initialize_logger(logger);
  }
  catch (const spdlog::spdlog_ex&)
  {
    // Another thread created the same name between the lookup above and here.
    if (auto existing = spdlog::get(loggerName))
    {
      return existing;
    }
    throw;
  }

  // Repeated after registration: captureLogs() may have set its flag and walked the registry between
  // the attempt above and this line, in which case neither reached this logger. The find in
  // captureLogsFrom() makes the repeat a no-op when the first attempt worked.
  //
  // This repeat is the unsafe one, and deliberately so. The logger is registered by now, so another
  // thread can resolve it by name and be walking its sink vector, which is the hazard the first placement
  // avoids. It is kept because the alternative is a logger with no crash ring at all, and the window
  // needs a second kernel's captureLogs() to land between the two lines. The crash reporter already
  // records that a second kernel arming beside the first is not covered. If that is ever fixed, delete
  // this call rather than moving it.
  CrashReporter::get().captureLogsFrom(logger);

  return logger;
}

void applyToAllLoggers(std::function<void(std::shared_ptr<spdlog::logger>)>&& func)
{
  spdlog::details::registry::instance().apply_all(std::move(func));
}

namespace
{

/// One kernel-owned sink, attached to every logger exactly once, in the only window where attaching is
/// safe. Components register their own sinks *behind* it, which never touches a logger's sink vector.
///
/// The vector is why this exists. spdlog walks a logger's sinks with no lock and hands out a bare
/// reference, so appending to a logger another thread is emitting through is a use-after-free. A component
/// cannot attach before the threads start either, because the sink's callback needs state that only
/// exists once the component runs. The relay moves the unsafe operation to a moment the kernel controls
/// and leaves the component with a mutex-guarded list.
///
/// It forwards the unformatted `log_msg`, so each registered sink applies its own pattern. That also makes
/// it immune to `registry::initialize_logger`, which pushes the registry's formatter onto every sink of
/// every new logger and would otherwise replace a component's pattern.
class RelaySink final: public spdlog::sinks::sink
{
public:
  void log(const spdlog::details::log_msg& msg) override
  {
    const std::lock_guard lock(mutex_);
    for (const auto& target: targets_)
    {
      if (target.sink->should_log(msg.level))
      {
        target.sink->log(msg);
      }
    }
  }

  void flush() override
  {
    const std::lock_guard lock(mutex_);
    for (const auto& target: targets_)
    {
      target.sink->flush();
    }
  }

  /// Ignored, both of them. The relay carries no format of its own: it hands the message on and each
  /// registered sink formats it. Accepting the registry's formatter here would overwrite a component's
  /// pattern every time any logger was created.
  void set_pattern(const std::string& /*pattern*/) override {}
  void set_formatter(std::unique_ptr<spdlog::formatter> /*formatter*/) override {}

  /// Register, or change an existing registration's terminal ownership. Reports what happened, so a caller
  /// can learn whether it was already registered, who holds the terminal, and how many sinks share the
  /// relay.
  LoggerSinkRegistration add(std::shared_ptr<spdlog::sinks::sink> sink, bool ownsTerminal)
  {
    const std::lock_guard lock(mutex_);
    auto itr = std::find_if(targets_.begin(), targets_.end(), [&sink](const auto& t) { return t.sink == sink; });

    LoggerSinkRegistration result;
    if (itr != targets_.end())
    {
      // Re-adding to claim the terminal has to work: the ownership rides on this call, so it is the
      // only way a component that registered early can take the screen later.
      itr->ownsTerminal = itr->ownsTerminal || ownsTerminal;
      result.ownsTerminal = itr->ownsTerminal;
    }
    else
    {
      result.added = true;
      result.ownsTerminal = ownsTerminal;
      targets_.push_back({sink, ownsTerminal});
    }

    result.registeredSinks = targets_.size();
    result.terminalOwnedElsewhere = std::any_of(
      targets_.begin(), targets_.end(), [&sink](const auto& t) { return t.ownsTerminal && t.sink != sink; });
    return result;
  }

  /// Unregister. Returns whether any registered sink still owns the terminal.
  bool remove(const std::shared_ptr<spdlog::sinks::sink>& sink)
  {
    const std::lock_guard lock(mutex_);
    targets_.erase(std::remove_if(targets_.begin(), targets_.end(), [&sink](const auto& t) { return t.sink == sink; }),
                   targets_.end());
    return anyOwnsTerminalLocked();
  }

  /// Whether any registered sink owns the terminal. For the add path, which needs it after the fact.
  [[nodiscard]] bool anyOwnsTerminal() const
  {
    const std::lock_guard lock(mutex_);
    return anyOwnsTerminalLocked();
  }

private:
  struct Target
  {
    std::shared_ptr<spdlog::sinks::sink> sink;
    bool ownsTerminal;
  };

  [[nodiscard]] bool anyOwnsTerminalLocked() const
  {
    return std::any_of(targets_.begin(), targets_.end(), [](const auto& t) { return t.ownsTerminal; });
  }

  mutable std::mutex mutex_;
  std::vector<Target> targets_;
};

std::shared_ptr<RelaySink>& relaySink()
{
  static auto relay = std::make_shared<RelaySink>();
  return relay;
}

/// True for a sink that writes to the terminal the kernel was started on. Cast to the base templates
/// rather than the concrete typedefs: `stdout_sink_base` is the base of both the stdout and stderr
/// variants and `ansicolor_sink` of both colour variants, so four casts cover all eight types that
/// spdlog_config can create, single- and multi-threaded alike.
bool isConsoleSink(const std::shared_ptr<spdlog::sinks::sink>& sink)
{
  auto* raw = sink.get();
#ifdef _WIN32
  if (dynamic_cast<spdlog::sinks::wincolor_sink<spdlog::details::console_mutex>*>(raw) != nullptr ||
      dynamic_cast<spdlog::sinks::wincolor_sink<spdlog::details::console_nullmutex>*>(raw) != nullptr)
  {
    return true;
  }
#endif
  return dynamic_cast<spdlog::sinks::stdout_sink_base<spdlog::details::console_mutex>*>(raw) != nullptr ||
         dynamic_cast<spdlog::sinks::stdout_sink_base<spdlog::details::console_nullmutex>*>(raw) != nullptr ||
         dynamic_cast<spdlog::sinks::ansicolor_sink<spdlog::details::console_mutex>*>(raw) != nullptr ||
         dynamic_cast<spdlog::sinks::ansicolor_sink<spdlog::details::console_nullmutex>*>(raw) != nullptr;
}

/// What a console sink's level was before the terminal was claimed, so claiming can be undone.
///
/// Silencing rather than detaching is the whole point: a sink's level is an atomic, so setting it costs
/// no lock and races nothing, while erasing the sink from a logger's vector is the use-after-free the
/// relay exists to avoid. It is also reversible, which erasing was not.
struct ConsoleState
{
  std::mutex mutex;
  std::vector<std::pair<std::shared_ptr<spdlog::sinks::sink>, spdlog::level::level_enum>> silenced;
  bool claimed {false};
};

ConsoleState& consoleState()
{
  static ConsoleState state;
  return state;
}

/// Silence every console sink now reachable, remembering what each was set to.
void silenceConsoleSinks()
{
  auto& state = consoleState();
  const std::lock_guard lock(state.mutex);
  state.claimed = true;
  spdlog::details::registry::instance().apply_all(
    [&state](const auto& logger)
    {
      for (const auto& sink: logger->sinks())
      {
        if (!isConsoleSink(sink))
        {
          continue;
        }
        const bool known = std::any_of(
          state.silenced.begin(), state.silenced.end(), [&sink](const auto& entry) { return entry.first == sink; });
        if (!known)
        {
          state.silenced.emplace_back(sink, sink->level());
          sink->set_level(spdlog::level::off);
        }
      }
    });
}

/// Put every silenced console sink back to the level it had.
void restoreConsoleSinks()
{
  auto& state = consoleState();
  const std::lock_guard lock(state.mutex);
  state.claimed = false;
  for (auto& [sink, level]: state.silenced)
  {
    sink->set_level(level);
  }
  state.silenced.clear();
}

/// Silence the console sinks of one logger, for a logger made while the terminal is claimed.
void silenceConsoleSinksOf(const std::shared_ptr<spdlog::logger>& logger)
{
  auto& state = consoleState();
  const std::lock_guard lock(state.mutex);
  if (!state.claimed)
  {
    return;
  }
  for (const auto& sink: logger->sinks())
  {
    if (isConsoleSink(sink))
    {
      state.silenced.emplace_back(sink, sink->level());
      sink->set_level(spdlog::level::off);
    }
  }
}

/// Put the relay on one logger. Not in the public header: it is a step inside `getOrCreateLogger` and has
/// no use to a component.
void attachLoggerRelay(const std::shared_ptr<spdlog::logger>& logger)
{
  if (!logger)
  {
    return;
  }

  // Called from getOrCreateLogger before the logger is registered, which is the one moment appending
  // to its sink vector is safe: nothing else can resolve it by name yet.
  auto& sinks = logger->sinks();
  if (std::find(sinks.begin(), sinks.end(), relaySink()) == sinks.end())
  {
    sinks.push_back(relaySink());
  }
  silenceConsoleSinksOf(logger);
}

}  // namespace

void installLoggerRelay()
{
  // Every logger that already exists. Called from KernelImpl::configure, before any component thread
  // exists, for the same reason the crash reporter's log ring is armed there.
  spdlog::details::registry::instance().apply_all(
    [](const auto& logger)
    {
      auto& sinks = logger->sinks();
      if (std::find(sinks.begin(), sinks.end(), relaySink()) == sinks.end())
      {
        sinks.push_back(relaySink());
      }
    });
}

Result<LoggerSinkRegistration, ExecError> addLoggerSink(std::shared_ptr<spdlog::sinks::sink> sink,
                                                        TerminalOwnership terminal)
{
  if (!sink)
  {
    return Err(ExecError {ErrorCategory::expectationsNotMet, "a null sink cannot be registered"});
  }

  const auto registration = relaySink()->add(std::move(sink), terminal == TerminalOwnership::owned);
  if (relaySink()->anyOwnsTerminal())
  {
    silenceConsoleSinks();
  }
  return Ok(registration);
}

FuncResult setAllLoggersLevel(spdlog::level::level_enum level)
{
  if (level < spdlog::level::trace || level > spdlog::level::off)
  {
    return Err(ExecError {ErrorCategory::expectationsNotMet, "log level out of range"});
  }

  // Sets every registered logger and the level a new one starts from, which `apply_all` cannot do.
  spdlog::details::registry::instance().set_level(level);
  return Ok();
}

void setCrashBannerDescriptor(int descriptor) noexcept { CrashReporter::setDiagnosticDescriptor(descriptor); }

void prepareCurrentThreadForCrashReports() noexcept { CrashReporter::prepareCurrentThread(); }

spdlog::level::level_enum getAllLoggersLevel()
{
  // The registry has no getter for the level a new logger starts from, so this reads the default
  // logger's, which set_level above assigns along with every other.
  if (auto logger = spdlog::default_logger(); logger)
  {
    return logger->level();
  }
  return spdlog::level::info;
}

FuncResult removeLoggerSink(const std::shared_ptr<spdlog::sinks::sink>& sink)
{
  if (!sink)
  {
    return Err(ExecError {ErrorCategory::expectationsNotMet, "a null sink cannot be unregistered"});
  }

  const bool stillOwned = relaySink()->remove(sink);
  if (!stillOwned)
  {
    restoreConsoleSinks();
  }
  return Ok();
}

}  // namespace impl

//--------------------------------------------------------------------------------------------------------------
// KernelApi
//--------------------------------------------------------------------------------------------------------------

KernelApi::KernelApi(Kernel& kernel, impl::Runner* runner) noexcept: kernel_(kernel), runner_(runner) {}

CustomTypeRegistry& KernelApi::getTypes() noexcept { return impl::getTypes(kernel_.pimpl_.get()); }

void KernelApi::requestKernelStop(int exitCode) { kernel_.requestStop(exitCode); }

std::shared_ptr<ObjectSource> KernelApi::getSource(const BusAddress& address)
{
  return impl::getSource(runner_, address);
}

std::shared_ptr<ObjectSource> KernelApi::getSource(const std::string& address)
{
  const auto parsedAddress = tryParseBusAddress(address);
  if (!parsedAddress)
  {
    std::string err;
    err.append("invalid bus address '");
    err.append(address);
    err.append("'");
    throwRuntimeError(err);
  }

  return getSource(*parsedAddress);
}

SessionsDiscoverer& KernelApi::getSessionsDiscoverer() noexcept { return impl::getSessionsDiscoverer(runner_); }

const ProcessInfo* KernelApi::fetchOwnerInfo(const Object* object) const noexcept
{
  return impl::fetchOwnerInfo(object);
}

const std::string& KernelApi::getAppName() const noexcept { return kernel_.getConfig().getParams().appName; }

std::vector<BusAddress> KernelApi::getConfiguredBusAddresses() const
{
  std::vector<BusAddress> configuredBusAddresses;
  const auto& config = kernel_.getConfig();

  addUniqueBusAddress(configuredBusAddresses, config.getParams().bus);
  const auto runMode = config.getParams().runMode;
  if (runMode == RunMode::virtualTime || runMode == RunMode::virtualTimeRunning)
  {
    addUniqueBusAddress(configuredBusAddresses, config.getParams().clockBus);
  }

  for (const auto& pipeline: config.getPipelinesToLoad())
  {
    for (const auto& object: pipeline.objects)
    {
      addUniqueBusAddress(configuredBusAddresses, object.bus);
    }
  }

  return configuredBusAddresses;
}

::sen::impl::WorkQueue* KernelApi::getWorkQueue() const noexcept { return impl::getWorkQueue(runner_); }

std::shared_ptr<spdlog::logger> KernelApi::getOrCreateLogger(const std::string& loggerName)
{
  return impl::getOrCreateLogger(loggerName);
}

void KernelApi::applyToAllLoggers(std::function<void(std::shared_ptr<spdlog::logger>)>&& func)
{
  impl::applyToAllLoggers(std::move(func));
}

Result<LoggerSinkRegistration, ExecError> KernelApi::addLoggerSink(std::shared_ptr<spdlog::sinks::sink> sink,
                                                                   TerminalOwnership terminal)
{
  return impl::addLoggerSink(std::move(sink), terminal);
}

FuncResult KernelApi::removeLoggerSink(const std::shared_ptr<spdlog::sinks::sink>& sink)
{
  return impl::removeLoggerSink(sink);
}

FuncResult KernelApi::setAllLoggersLevel(spdlog::level::level_enum level) { return impl::setAllLoggersLevel(level); }

spdlog::level::level_enum KernelApi::getAllLoggersLevel() { return impl::getAllLoggersLevel(); }

void KernelApi::setCrashBannerDescriptor(int descriptor) noexcept { impl::setCrashBannerDescriptor(descriptor); }

void KernelApi::prepareCurrentThreadForCrashReports() noexcept { impl::prepareCurrentThreadForCrashReports(); }

//--------------------------------------------------------------------------------------------------------------
// ConfigGetter
//--------------------------------------------------------------------------------------------------------------

ConfigGetter::ConfigGetter(const VarMap& config) noexcept: config_(config) {}

const VarMap& ConfigGetter::getConfig() const noexcept { return config_; }

//--------------------------------------------------------------------------------------------------------------
// RegistrationApi
//--------------------------------------------------------------------------------------------------------------

RegistrationApi::RegistrationApi(Kernel& kernel, impl::Runner* runner, const VarMap& config) noexcept
  : ConfigGetter(config), KernelApi(kernel, runner)
{
}

//--------------------------------------------------------------------------------------------------------------
// PreloadApi
//--------------------------------------------------------------------------------------------------------------

PreloadApi::PreloadApi(Kernel& kernel, impl::Runner* runner, const VarMap& config) noexcept
  : ConfigGetter(config), KernelApi(kernel, runner), kernel_(kernel)
{
}

void PreloadApi::installTransportFactory(TransportFactory&& factory, uint32_t transportVersion) const
{
  impl::installTransportFactory(kernel_.pimpl_.get(), std::move(factory), transportVersion);
}

void PreloadApi::installTracerFactory(TracerFactory&& factory) const
{
  kernel_.pimpl_->installTracerFactory(std::move(factory));
}

void PreloadApi::installFootprintReporter(sen::std_util::move_only_function<NetworkFootprintReporter>&& offlineReporter)
{
  installFootprintReporter(std::move(offlineReporter), sen::std_util::move_only_function<NetworkFootprint() const> {});
}

void PreloadApi::installFootprintReporter(sen::std_util::move_only_function<NetworkFootprintReporter>&& offlineReporter,
                                          sen::std_util::move_only_function<NetworkFootprint() const>&& runtimeReporter)
{
  kernel_.pimpl_->installFootprintReporter(std::move(offlineReporter), std::move(runtimeReporter));
}

//--------------------------------------------------------------------------------------------------------------
// LoadApi
//--------------------------------------------------------------------------------------------------------------

LoadApi::LoadApi(Kernel& kernel, impl::Runner* runner, const VarMap& config) noexcept
  : ConfigGetter(config), KernelApi(kernel, runner)
{
}

//--------------------------------------------------------------------------------------------------------------
// InitApi
//--------------------------------------------------------------------------------------------------------------

InitApi::InitApi(Kernel& kernel, impl::Runner* runner, const VarMap& config) noexcept
  : ConfigGetter(config), KernelApi(kernel, runner)
{
}

//--------------------------------------------------------------------------------------------------------------
// RunApi
//--------------------------------------------------------------------------------------------------------------

RunApi::RunApi(Kernel& kernel,
               impl::KernelImpl& kernelImpl,
               impl::Runner* runner,
               std::atomic_bool& stopRequested,
               const VarMap& config,
               Guarded<TimeStamp>& timePoint) noexcept
  : ConfigGetter(config)
  , KernelApi(kernel, runner)
  , kernelImpl_(kernelImpl)
  , runner_(runner)
  , stopRequested_(stopRequested)
  , timePoint_(timePoint)
{
}

const std::atomic_bool& RunApi::stopRequested() const noexcept { return stopRequested_; }

void RunApi::drainInputs() { return impl::drainInputs(runner_); }

void RunApi::update() { impl::update(runner_); }

void RunApi::commit() { return impl::commit(runner_); }

FuncResult RunApi::execLoop(Duration cycleTime, std::function<void()>&& func, bool logOverruns)
{
  std::function<void()> f = func ? std::move(func) : []()
  {
    // no code needed
  };

  return impl::execLoop(runner_, cycleTime, std::move(f), logOverruns);
}

TimeStamp RunApi::getStartTime() const noexcept { return runner_->getStartTime(); }

TimeStamp RunApi::getTime() const noexcept { return timePoint_; }

std::optional<Duration> RunApi::getTargetCycleTime() const noexcept { return runner_->getCycleTime(); }

ComponentMonitoringInfo RunApi::fetchComponentMonitoringInfo() const { return runner_->fetchMonitoringInfo(); }

KernelMonitoringInfo RunApi::fetchMonitoringInfo() const { return kernelImpl_.fetchMonitoringInfo(); }

Span<const ComponentInfo> RunApi::getImportedPackages() const noexcept { return kernelImpl_.getImportedPackages(); }

Span<const ComponentInfo> RunApi::getLoadedComponents() const noexcept { return kernelImpl_.getLoadedComponents(); }

std::optional<uint32_t> RunApi::getTransportProtocolVersion() const noexcept
{
  return kernelImpl_.getTransportProtocolVersion();
}

Tracer& RunApi::getTracer() const noexcept { return runner_->getTracer(); }

//--------------------------------------------------------------------------------------------------------------
// UnloadApi
//--------------------------------------------------------------------------------------------------------------

UnloadApi::UnloadApi(Kernel& kernel, const VarMap& config, impl::Runner* runner) noexcept
  : ConfigGetter(config), KernelApi(kernel, runner)
{
}

}  // namespace sen::kernel
