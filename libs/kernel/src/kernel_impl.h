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
#include "sen/kernel/kernel.h"
#include "sen/kernel/kernel_config.h"

// spdlog
#include <spdlog/fwd.h>

// std
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
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

  /// The runners of the kernel
  [[nodiscard]] const std::vector<std::unique_ptr<Runner>>& getRunners() const noexcept { return runners_; }

  /// The internal logger used by all kernel components
  [[nodiscard]] static std::shared_ptr<spdlog::logger> getKernelLogger();

  /// Cycles all the runners with virtualized time.
  /// This is: set time, drain inputs and cycle.
  Duration advanceTimeDrainInputsAndUpdate(const Duration& delta);

  /// Flushes all the runners with virtualized time.
  void flushVirtualTime();

  /// Cycles all the runners to get the delta time until the next execution.
  [[nodiscard]] Duration computeNextExecutionDeltaTime() const;

  /// Monitoring information
  KernelMonitoringInfo fetchMonitoringInfo() const;

  /// Tracer creation
  [[nodiscard]] std::unique_ptr<Tracer> makeTracer(std::string_view contextName);

  /// Set the factory function for creating tracers
  void installTracerFactory(TracerFactory&& factory);

private:
  /// Implementation of run() depending on the run mode stated in the configuration.
  [[nodiscard]] int applyRunMode(KernelBlockMode blockMode);

  /// Implementation of the actual stop() code.
  void doStop();

  /// Sets the initial parameters based on the passed configuration.
  void configure();

  /// Code executed by the thread that called run()
  void idleThread();

  /// Configures the crash reporting mechanism
  void configureCrashReporting();

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
  std::vector<std::unique_ptr<Runner>> runners_;
  std::vector<Runner*> virtualTimeRunners_;
  std::vector<std::future<void>> virtualTimeRunnersFutures_;
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
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline Duration KernelImpl::advanceTimeDrainInputsAndUpdate(const Duration& delta)
{
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
  return {std::chrono::high_resolution_clock::now() - beforeExecution};
}

inline void KernelImpl::flushVirtualTime()
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
  Lock lock(usageMutex_);

  KernelMonitoringInfo result;
  result.runMode = config_.getParams().runMode;
  result.components.reserve(runners_.size());
  result.transportStats = sessionManager_.fetchTransportStats();

  for (const auto& runner: runners_)
  {
    ComponentMonitoringInfo monitoringInfo;
    monitoringInfo.config = runner->getComponentContext().config;
    monitoringInfo.info = runner->getComponentContext().info;
    monitoringInfo.requiresRealTime = runner->getComponentContext().instance->isRealTimeOnly();
    monitoringInfo.objectCount = runner->getObjectCount();
    monitoringInfo.cycleTime = runner->getCycleTime();

    result.components.emplace_back(std::move(monitoringInfo));
  }
  return result;
}

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_KERNEL_IMPL_H
