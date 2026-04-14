// === kernel_impl.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "kernel_impl.h"

// implementation
#include "crash_reporter.h"
#include "kernel_component.h"
#include "operating_system.h"
#include "pipeline_component.h"
#include "runner.h"
#include "wall_clock.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/kernel.h"
#include "sen/kernel/kernel_config.h"
#include "sen/kernel/tracer.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// spdlog
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog_config.h>

// std
#include <chrono>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace sen::kernel::impl
{

KernelImpl::KernelImpl(std::shared_ptr<kernel::OperatingSystem> os, Kernel& subject, KernelConfig config)
  : config_(std::move(config))
  , os_(std::move(os))
  , subject_(subject)
  , pluginManager_(os_)
  , executor_(runners_)
  , isRunning_(false)
  , isStopping_(false)
  , requestedExitCode_(0)
  , sessionManager_(
      *this,
      [this](const auto& name) { sessionAvailable(name); },
      [this](const auto& name) { sessionUnavailable(name); })
  , tracerFactory_(getDefaultTracerFactory())
{
}

KernelImpl::~KernelImpl() { CrashReporter::get().uninstall(); }

int KernelImpl::run(KernelBlockMode blockMode)
{
  if (isRunning_)
  {
    return 0;
  }

  isStopping_ = false;
  requestedExitCode_ = EXIT_SUCCESS;
  blockMode_ = blockMode;
  {
    Lock lock(usageMutex_);

    configureCrashReporting();

    if (!configured_)
    {
      configure();
      configured_ = true;
    }

    executor_.startUp(config_.getParams().lockMemoryPages);

    // Can only be started after components are preloaded to ensure a trace component can install the tracer factory.
    sessionManager_.startMessageProcessing();
  }

  getKernelLogger()->debug("running");

  isRunning_ = true;
  return applyRunMode(blockMode);
}

void KernelImpl::requestStop(int exitCode)
{
  {
    std::unique_lock<std::mutex> lock(stopRequestedConditionMutex_);  // NOSONAR
    if (!isRunning_ || isStopping_ || stopRequested_)
    {
      return;
    }

    requestedExitCode_ = exitCode;
    stopRequested_.store(true);
  }
  stopRequestedCondition_.notify_one();

  if (blockMode_ == KernelBlockMode::doNotBlock)
  {
    doStop();
  }
}

int KernelImpl::applyRunMode(KernelBlockMode blockMode)
{
  // do this check before starting any runner
  WallClock::hardwareSupportsInvariantTSC();

  switch (config_.getParams().runMode)
  {
    case RunMode::startAndStop:
    {
      executor_.waitUntilStarted();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    break;

    case RunMode::realTime:
    case RunMode::virtualTime:
    {
      if (blockMode == KernelBlockMode::doBlock)
      {
        idleThread();
      }
      else
      {
        executor_.waitUntilStarted();
        return requestedExitCode_;
      }
    }
    break;

    case RunMode::virtualTimeRunning:
    {
      executor_.waitUntilStarted();
      if (!virtualTimeRunners_.empty())
      {
        while (!stopRequested_.load())
        {
          advanceTimeDrainInputsAndUpdate(computeNextExecutionDeltaTime());
          flushVirtualTime();
        }
      }
    }
    break;
  }

  doStop();
  return requestedExitCode_;
}

void KernelImpl::idleThread()
{
  std::unique_lock<std::mutex> lock(stopRequestedConditionMutex_);  // NOSONAR
  stopRequestedCondition_.wait(lock, [this]() { return stopRequested_.load(); });
}

void KernelImpl::configureCrashReporting()
{
  if (!config_.getParams().crashReportDisabled)
  {
    CrashReporter::get().registerKernel(config_);
  }
}

void KernelImpl::doStop()
{
  isStopping_.store(true);
  {
    Lock lock(usageMutex_);
    executor_.shutDown();
  }

  isRunning_.store(false);
  isStopping_.store(false);
  stoppedCondition_.notify_all();
}

void KernelImpl::configure()
{
  // configure the kernel logging
  configureSpdlog(config_.getParams().logConfig);

  // plug all requested plugins and create runners
  for (const auto& elem: config_.getPluginsToLoad())
  {
    auto pluginInfo = pluginManager_.plug(elem.path);
    pluginInfo.component.config = elem.config;
    runners_.push_back(std::make_unique<Runner>(*this, *os_, pluginInfo.component, elem.config.group, elem.params));
  }

  // create the requested in-memory components
  for (auto elem: config_.getComponentsToLoad())
  {
    elem.component.config = elem.config;
    runners_.push_back(std::make_unique<Runner>(*this, *os_, elem.component, elem.config.group, elem.params));
  }

  // create the requested pipelines
  for (const auto& elem: config_.getPipelinesToLoad())
  {
    pipelines_.push_back(std::make_unique<PipelineComponent>(elem));
    ComponentContext context;
    context.instance = pipelines_.back().get();
    context.config = elem.config;
    context.info.name = elem.name;
    context.info.buildInfo = Kernel::getBuildInfo();

    runners_.push_back(std::make_unique<Runner>(*this, *os_, context, elem.config.group, elem.params));
  }

  // add the kernel component
  kernelComponent_ = std::make_unique<KernelComponent>(this);
  {
    ComponentContext context;
    context.info.buildInfo = Kernel::getBuildInfo();
    context.info.description = "kernel internal component";
    context.info.name = "kernel";
    context.config.group = 1U;
    context.config.priority = Priority::nominalMin;
    context.config.sleepPolicy = config_.getParams().sleepPolicy;
    context.instance = kernelComponent_.get();

    // it is the last one to start
    runners_.push_back(std::make_unique<Runner>(*this, *os_, context, 1U, VarMap {}));
  }

  bool needsVirtualTime =
    config_.getParams().runMode == RunMode::virtualTime || config_.getParams().runMode == RunMode::virtualTimeRunning;

  // compute the list of runners using virtual time
  if (needsVirtualTime)
  {
    virtualTimeRunners_.reserve(runners_.size());
    for (const auto& runner: runners_)
    {
      if (!runner->getComponentContext().instance->isRealTimeOnly())
      {
        virtualTimeRunners_.emplace_back(runner.get());
      }
    }
    virtualTimeRunnersFutures_.reserve(virtualTimeRunners_.size());
  }
}

void KernelImpl::sessionAvailable(const std::string& name) const  // NOSONAR
{
  for (const auto& elem: runners_)
  {
    elem->getSessionsDiscoverer().sessionAvailable(name);
  }
}

void KernelImpl::sessionUnavailable(const std::string& name) const  // NOSONAR
{
  for (const auto& elem: runners_)
  {
    elem->getSessionsDiscoverer().sessionUnavailable(name);
  }
}

std::shared_ptr<spdlog::logger> KernelImpl::getKernelLogger()
{
  static std::shared_ptr<spdlog::logger> logger = spdlog::stderr_color_mt("kernel");
  return logger;
}

std::unique_ptr<Tracer> KernelImpl::makeTracer(std::string_view contextName) { return tracerFactory_(contextName); }

void KernelImpl::installTracerFactory(TracerFactory&& factory) { tracerFactory_ = std::move(factory); }

}  // namespace sen::kernel::impl

extern "C" SEN_EXPORT void get0x2cfc8aa4Types(::sen::ExportedTypesList& types);  // basic_types.stl
extern "C" SEN_EXPORT void get0xf7949d6aTypes(::sen::ExportedTypesList& types);  // log.stl
extern "C" SEN_EXPORT void get0xc817451aTypes(::sen::ExportedTypesList& types);  // type_specs.stl

extern "C" SEN_EXPORT void get0x5b269ebdTypes(::sen::ExportedTypesList& types)
{
  get0x2cfc8aa4Types(types);
  get0xf7949d6aTypes(types);
  get0xc817451aTypes(types);
}
