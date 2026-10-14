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

void commit(Runner* runner, TimeStamp time) { runner->commit(time); }

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

}  // namespace

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

void RunApi::commit(TimeStamp time) { return impl::commit(runner_, time); }

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
