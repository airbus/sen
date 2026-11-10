// === kernel_impl.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_KERNEL_IMPL_H
#define SEN_LIBS_KERNEL_SRC_KERNEL_IMPL_H

// kernel
#include "bus/session_manager.h"
#include "executor.h"
#include "operating_system.h"
#include "pipeline_component.h"
#include "plugin_manager.h"
#include "runner.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/move_only_function.h"
#include "sen/core/base/span.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/kernel.h"
#include "sen/kernel/kernel_config.h"

// generated
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/network_footprint.stl.h"

// spdlog
#include <spdlog/fwd.h>

// std
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <vector>

namespace sen::kernel::impl
{

class KernelComponent;

/// Implementation for the Kernel class.
class KernelImpl final
{
  SEN_NOCOPY_NOMOVE(KernelImpl)

public:
  /// Copies the arguments to members
  KernelImpl(std::shared_ptr<kernel::OperatingSystem> os, Kernel& subject, KernelConfig config);

  /// Stops the kernel and unplugs all plugins if not previously done so.
  ~KernelImpl();

public:
  /// Commands the kernel to stop.
  /// The exitCode will be used as the return value for the run() call.
  void requestStop(int exitCode);

  /// The configuration that was passed during construction.
  [[nodiscard]] const KernelConfig& getConfig() const noexcept { return config_; }

  /// The path to the configuration file that was passed during construction.
  [[nodiscard]] std::filesystem::path getConfigFilePath() const noexcept { return config_.getConfigFilePath(); }

  /// Executes the kernel according to the passed configuration.
  [[nodiscard]] int run(KernelBlockMode blockMode);

  /// The global type registry
  [[nodiscard]] CustomTypeRegistry& getTypes() noexcept { return types_; }

  /// The session manager
  [[nodiscard]] SessionManager& getSessionManager() noexcept { return sessionManager_; }

  /// The kernel object that is being implemented
  [[nodiscard]] Kernel& getSubject() noexcept { return subject_; }

  /// The runners of the kernel. It hands out the vector and takes no lock, so it is only for
  /// callers that run once configure() has appended the last runner.
  [[nodiscard]] const std::vector<std::unique_ptr<Runner>>& getRunners() const noexcept { return runners_; }

  /// The internal logger used by all kernel components
  [[nodiscard]] static std::shared_ptr<spdlog::logger> getKernelLogger();

  /// Cycles all the runners with virtualized time.
  /// This is: set time, drain inputs and cycle. Empty once the kernel is stopping.
  std::optional<Duration> advanceTimeDrainInputsAndUpdate(const Duration& delta);

  /// Flushes all the runners with virtualized time. False once the kernel is stopping.
  bool flushVirtualTime();

  /// Cycles all the runners to get the delta time until the next execution.
  [[nodiscard]] Duration computeNextExecutionDeltaTime() const;

  /// Monitoring information
  KernelMonitoringInfo fetchMonitoringInfo() const;

  /// Build information for all imported packages (from pipeline components).
  /// The span references the kernel-owned cache populated during preload,
  /// and is stable for the lifetime of the kernel.
  [[nodiscard]] Span<const ComponentInfo> getImportedPackages() const noexcept;

  /// Registers a batch of imported packages. Called by pipeline components
  /// once they have opened their shared libraries during preload.
  void registerImportedPackages(Span<const ComponentInfo> packages);

  /// Build information for every non-pipeline, non-kernel component loaded
  /// by the kernel (shared-library plugins and programmatic components).
  /// The span references kernel-owned storage populated during configure()
  /// and stable for the lifetime of the kernel.
  [[nodiscard]] Span<const ComponentInfo> getLoadedComponents() const noexcept;

  /// Version of the currently installed transport protocol, if any.
  [[nodiscard]] std::optional<uint32_t> getTransportProtocolVersion() const noexcept;

  /// Tracer creation
  [[nodiscard]] std::unique_ptr<Tracer> makeTracer(std::string_view contextName);

  /// Set the factory function for creating tracers
  void installTracerFactory(TracerFactory&& factory);

  /// Set the reporter used to build process network footprint
  void installFootprintReporter(sen::std_util::move_only_function<NetworkFootprintReporter>&& offlineReporter,
                                sen::std_util::move_only_function<NetworkFootprint() const>&& runtimeReporter);

  /// Configures and preloads the kernel, builds the offline network footprint, and performs final cleanup.
  [[nodiscard]] NetworkFootprint generateOfflineNetworkFootprint(Span<const BusAddress> suppliedBusAddresses);

  /// Builds an offline network footprint for the supplied bus addresses
  [[nodiscard]] NetworkFootprint getOfflineNetworkFootprint(Span<const BusAddress> busAddresses) const;

  /// Returns current network allocations
  [[nodiscard]] NetworkFootprint getRuntimeNetworkFootprint() const;

private:
  /// Implementation of run() depending on the run mode stated in the configuration.
  [[nodiscard]] int applyRunMode(KernelBlockMode blockMode);

  /// Implementation of the actual stop() code.
  void doStop();

  /// From here on no clock call tasks a runner.
  void closeSteps();

  void finishStepInFlight();
  [[nodiscard]] bool stepCallBegins();
  void stepCallEnded(bool leavesAFlush);
  void commitTheStep();

  /// One clock call through the step gate, for as long as it lives.
  class StepCall
  {
    SEN_NOCOPY_NOMOVE(StepCall)

  public:
    StepCall(KernelImpl& kernel, bool leavesAFlush)
      : kernel_(kernel), leavesAFlush_(leavesAFlush), letThrough_(kernel.stepCallBegins())
    {
    }

    ~StepCall()
    {
      if (letThrough_)
      {
        kernel_.stepCallEnded(leavesAFlush_);
      }
    }

    [[nodiscard]] bool letThrough() const noexcept { return letThrough_; }

  private:
    KernelImpl& kernel_;
    bool leavesAFlush_;
    bool letThrough_;
  };

  /// Sets the initial parameters based on the passed configuration.
  void configure();

  /// Code executed by the thread that called run()
  void idleThread();

  /// Configures the crash reporting mechanism
  void configureCrashReporting();

  /// Records what the kernel is doing, for a crash that happens while it is doing it
  void setCrashPhase(const char* phase) const;

  /// Records what was loaded, and the build each was made by
  void setCrashComponents() const;

private:
  void sessionAvailable(const std::string& name) const;
  void sessionUnavailable(const std::string& name) const;

private:
  using Lock = std::scoped_lock<std::recursive_mutex>;

private:
  bool configured_ {false};
  KernelConfig config_;
  KernelBlockMode blockMode_ = KernelBlockMode::doBlock;
  std::shared_ptr<kernel::OperatingSystem> os_;
  Kernel& subject_;
  kernel::PluginManager pluginManager_;
  mutable std::recursive_mutex usageMutex_;

  // runners_ is appended to while the kernel configures itself and never afterwards, but another
  // thread can ask for monitoring at any time. usageMutex_ cannot serve both: doStop holds it
  // across the join of the component threads, so a reader waiting on it never returns. Taken
  // around the appends and by fetchMonitoringInfo; every other reader runs after configure().
  mutable std::shared_mutex runnersMutex_;
  std::vector<std::unique_ptr<Runner>> runners_;
  std::vector<Runner*> virtualTimeRunners_;
  std::vector<std::future<void>> virtualTimeRunnersFutures_;

  // The step gate. A clock call tasks the runners and waits for each to answer, so a runner stopped
  // under one never answers and the call waits forever. The stop closes the gate first.
  std::mutex stepGateMutex_;
  std::condition_variable stepGateCondition_;
  std::size_t stepCallUnderWay_ = 0U;
  bool stepsClosed_ = false;
  bool flushOwed_ = false;
  Executor executor_;
  std::atomic_bool isRunning_ = false;
  std::atomic_bool isStopping_ = false;
  std::atomic_bool stopRequested_ = false;
  std::condition_variable stopRequestedCondition_;
  std::condition_variable stoppedCondition_;
  std::mutex stopRequestedConditionMutex_;
  std::atomic<int> requestedExitCode_;
  std::unique_ptr<KernelComponent> kernelComponent_;
  std::vector<std::unique_ptr<PipelineComponent>> pipelines_;
  CustomTypeRegistry types_;
  SessionManager sessionManager_;
  std::function<std::unique_ptr<Tracer>(std::string_view)> tracerFactory_;
  sen::std_util::move_only_function<NetworkFootprint(Span<const BusAddress>) const> networkOfflineReport_;
  sen::std_util::move_only_function<NetworkFootprint() const> networkRuntimeReport_;
  std::vector<ComponentInfo> importedPackages_;
  std::vector<ComponentInfo> loadedComponents_;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline std::optional<Duration> KernelImpl::advanceTimeDrainInputsAndUpdate(const Duration& delta)
{
  const StepCall call(*this, true);
  if (!call.letThrough())
  {
    return std::nullopt;
  }

  // task the components
  auto beforeExecution = std::chrono::high_resolution_clock::now();

  for (auto* runner: virtualTimeRunners_)
  {
    virtualTimeRunnersFutures_.emplace_back(runner->advanceVirtualTime(delta));
  }

  // wait for all
  for (auto& future: virtualTimeRunnersFutures_)
  {
    future.get();
  }

  virtualTimeRunnersFutures_.clear();
  return Duration {std::chrono::high_resolution_clock::now() - beforeExecution};
}

inline bool KernelImpl::flushVirtualTime()
{
  const StepCall call(*this, false);
  if (!call.letThrough())
  {
    return false;
  }

  commitTheStep();
  return true;
}

inline void KernelImpl::commitTheStep()
{
  // task the components
  for (auto* runner: virtualTimeRunners_)
  {
    virtualTimeRunnersFutures_.emplace_back(runner->commitVirtualTime());
  }

  // wait for all
  for (auto& future: virtualTimeRunnersFutures_)
  {
    if (future.valid())
    {
      future.get();
    }
  }

  virtualTimeRunnersFutures_.clear();
}

inline Duration KernelImpl::computeNextExecutionDeltaTime() const
{
  if (virtualTimeRunners_.empty())
  {
    return {};
  }

  Duration result = std::numeric_limits<Duration::ValueType>::max();
  for (const auto* runner: virtualTimeRunners_)
  {
    const auto runnerNextExecutionDelta = runner->getNextExecutionDeltaTime();
    if (runnerNextExecutionDelta < result)
    {
      result = runnerNextExecutionDelta;
    }
  }
  return result;
}

inline KernelMonitoringInfo KernelImpl::fetchMonitoringInfo() const
{
  KernelMonitoringInfo result;
  result.runMode = config_.getParams().runMode;
  result.transportStats = sessionManager_.fetchTransportStats();

  const std::shared_lock runnersLock(runnersMutex_);

  result.components.reserve(runners_.size());
  for (const auto& runner: runners_)
  {
    result.components.emplace_back(runner->fetchMonitoringInfo());
  }
  return result;
}

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_KERNEL_IMPL_H
