// === runner.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "runner.h"

// implementation
#include "kernel_impl.h"
#include "operating_system.h"
#include "precision_sleeper.h"
#include "thread.h"
#include "wall_clock.h"

// bus
#include "bus/local_participant.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/kernel_config.h"
#include "sen/kernel/source_info.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// spdlog
#include <spdlog/spdlog.h>

// std
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <future>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

// OS
#ifdef __linux__
#  include <sys/resource.h>
#endif

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

// Max logs to be produced on overruns.
constexpr std::size_t maxTimeOverrunForLog = 100;

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

// NOLINTNEXTLINE
#define EXCEPTION_WRAP_BLOCK(statements)                                                                               \
  do                                                                                                                   \
  {                                                                                                                    \
    statements                                                                                                         \
  } while (0);

//--------------------------------------------------------------------------------------------------------------
// SessionsDiscoverer
//--------------------------------------------------------------------------------------------------------------

namespace sen::kernel
{

std::shared_ptr<SessionInfoProvider> SessionsDiscoverer::makeSessionInfoProvider(const std::string& sessionName)
{
  auto result = owner_->makeSessionInfoProvider(sessionName);
  children_.push_back(result->weak_from_this());
  return result;
}

SessionInfoProvider::~SessionInfoProvider() { session_->infoProviderDeleted(this); }

}  // namespace sen::kernel

namespace sen::kernel::impl
{

namespace
{

template <typename R>
void terminateIfError(const R& result, const char* operation, const ComponentContext& context)
{
  if (result.isError())
  {
    spdlog::error(
      "Error detected while {} component {}: {}", operation, context.info.name, result.getError().explanation);

    spdlog::dump_backtrace();
    std::terminate();
  }
}

[[nodiscard]] NanoSecs getThreadCpuUserTime() noexcept
{
#if defined(__linux__)
  rusage usage;  // NOLINT(misc-include-cleaner)
  getrusage(RUSAGE_THREAD, &usage);
  return std::chrono::seconds(usage.ru_utime.tv_sec) + std::chrono::microseconds(usage.ru_utime.tv_usec);
#elif defined(__APPLE__)
  rusage usage;  // NOLINT(misc-include-cleaner)
  getrusage(RUSAGE_SELF, &usage);
  return std::chrono::seconds(usage.ru_utime.tv_sec) + std::chrono::microseconds(usage.ru_utime.tv_usec);
#elif defined(_WIN32)
  FILETIME creation;
  FILETIME exitTime;
  FILETIME kernelTime;
  FILETIME usrTime;
  GetThreadTimes(GetCurrentThread(), &creation, &exitTime, &kernelTime, &usrTime);

  // convert 100-ns intervals to microseconds and then
  // adjust for the epoch difference (1601-01-01 00:00:00 UTC vs 1970-01-01 00:00:00 UTC)
  const auto high = static_cast<uint64_t>(usrTime.dwHighDateTime);
  const auto low = static_cast<uint64_t>(usrTime.dwLowDateTime);
  return std::chrono::microseconds((((high << 32) | low) / 10) - 11644473600000000ull);  // NOLINT
#else
#  error "OS support not implemented"
#endif
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// Runner
//--------------------------------------------------------------------------------------------------------------

Runner::Runner(KernelImpl& kernel,
               OperatingSystem& os,
               ComponentContext component,
               uint32_t group,
               VarMap config) noexcept
  : kernel_(kernel)
  , os_(os)
  , component_(std::move(component))
  , state_(ComponentState::unloaded)
  , stopFlag_(false)
  , thread_({nullptr})
  , group_(group)
  , config_(std::move(config))
  , workQueue_(component_.config.inQueue.maxSize,
               component_.config.inQueue.evictionPolicy == QueueEvictionPolicy::dropOldest)
  , serializableEvents_(component_.config.outQueue.maxSize,
                        component_.config.inQueue.evictionPolicy == QueueEvictionPolicy::dropOldest)
  , runApi_(kernel.getSubject(), kernel, this, stopFlag_, config_, time_)
  , registrationApi_(kernel.getSubject(), this, config_)
  , sessionsDiscoverer_(this)
  , nextExecutionDeltaTime_(0)
  , name_(component_.info.name)
{
  oversleepPlotName_.append(name_).append(" oversleep (ns)");
  oversleptMessage_.append(name_).append(" missed frame (overslept)");
  missedFrameEndMessage_.append(name_).append(" missed frame (interruption)");
  overrunMessage_.append(name_).append(" execution time overrun");
  workQueueName_.append(name_).append(" work queue size");
  eventQueueName_.append(name_).append(" event queue size");

  auto logger = KernelImpl::getKernelLogger();

  // set the work overflow callback
  {
    std::string dropWorkMessageString(name_);
    dropWorkMessageString.append(" work queue overflow");
    workQueue_.setOnDropped(
      [msg = std::move(dropWorkMessageString), logger, this](auto&)
      {
        tracer_->message(msg);
        logger->warn(msg);
      });
  }

  // set the event overflow callback
  {
    std::string dropEventsMessageString(name_);
    dropEventsMessageString.append(" event queue overflow");
    serializableEvents_.setOnDropped(
      [msg = std::move(dropEventsMessageString), logger, this]()
      {
        tracer_->message(msg);
        logger->warn(msg);
      });
  }
}

FuncResult Runner::preload()
{
  const auto state = state_.load();

  if (state == ComponentState::preloaded)
  {
    return Ok();  // already preloaded
  }

  if (state != ComponentState::unloaded)
  {
    std::string err;
    err.append("invalid state for preload() call on component '");
    err.append(component_.info.name);
    err.append("'");
    throw std::logic_error(err);
  }

  FuncResult preloadResult = ::sen::Ok();

  // NOLINTNEXTLINE
  EXCEPTION_WRAP_BLOCK(preloadResult = component_.instance->preload(PreloadApi(kernel_.getSubject(), this, config_));)
  terminateIfError(preloadResult, "preloading", component_);

  state_.store(ComponentState::preloaded);
  return preloadResult;  // done here
}

FuncResult Runner::load()
{
  const auto state = state_.load();

  if (state == ComponentState::loaded)
  {
    return Ok();  // already loaded
  }

  if (state != ComponentState::preloaded)
  {
    std::string err;
    err.append("invalid state for load() call on component '");
    err.append(component_.info.name);
    err.append("'");
    throw std::logic_error(err);
  }

  state_.store(ComponentState::loading);

  FuncResult loadResult = ::sen::Ok();

  // NOLINTNEXTLINE
  EXCEPTION_WRAP_BLOCK(loadResult = component_.instance->load(LoadApi(kernel_.getSubject(), this, config_));)
  terminateIfError(loadResult, "loading", component_);

  state_.store(ComponentState::loaded);
  return loadResult;  // done here
}

PassResult Runner::init()
{
  const auto state = state_.load();

  if (state == ComponentState::initialized)
  {
    return ::sen::Ok(OpState {OpFinished {}});  // already initialized
  }

  // can only be in loaded (first time) or initializing
  if (state != ComponentState::loaded && state != ComponentState::initializing)
  {
    std::string err;
    err.append("invalid state for init() call on component '");
    err.append(component_.info.name);
    err.append("'");
    throw std::logic_error(err);
  }

  state_.store(ComponentState::initializing);

  PassResult initResult = ::sen::Ok(OpState {OpFinished {}});

  // NOLINTNEXTLINE
  EXCEPTION_WRAP_BLOCK(initResult = component_.instance->init(InitApi(kernel_.getSubject(), this, config_));)
  terminateIfError(initResult, "initializing", component_);

  if (std::holds_alternative<OpFinished>(initResult.getValue()))
  {
    state_.store(ComponentState::initialized);
    return initResult;
  }

  return initResult;
}

FuncResult Runner::run()
{
  const auto state = state_.load();

  if (state == ComponentState::running)
  {
    return Ok();  // already running
  }

  // we need to be initialized
  if (state != ComponentState::initialized)
  {
    std::string err;
    err.append("invalid state for run() call on component '");
    err.append(component_.info.name);
    err.append("'");
    throw std::logic_error(err);
  }

  ThreadConfig threadConfig {};
  threadConfig.arg = this;
  threadConfig.function = &threadFunction;
  threadConfig.affinity = component_.config.cpuAffinity;
  threadConfig.priority = component_.config.priority;
  threadConfig.stackSize = component_.config.stackSize;
  threadConfig.name = std::string(component_.info.name) + "-component";

  auto threadCreationResult = os_.createThread(threadConfig);
  if (threadCreationResult.isError())
  {
    std::string msg;
    msg.append("error creating thread for component '");
    msg.append("': ");
    msg.append(StringConversionTraits<ThreadCreateErr>::toString(threadCreationResult.getError()));

    ExecError err {ErrorCategory::runtimeError, msg};
    toErrorState(err);
    SEN_UNREACHABLE();
    return Err(err);
  }

  thread_ = threadCreationResult.getValue();
  state_.store(ComponentState::running);
  return Ok();
}

void Runner::unload()
{
  const auto state = state_.load();

  if (state == ComponentState::unloaded)
  {
    return;  // already unloaded
  }

  if (state != ComponentState::stopped)
  {
    std::string err;
    err.append("invalid state for unload() call on component '");
    err.append(component_.info.name);
    err.append("'");
    throw std::logic_error(err);
  }

  state_.store(ComponentState::unloading);

  FuncResult unloadResult = ::sen::Ok();

  // NOLINTNEXTLINE
  EXCEPTION_WRAP_BLOCK(unloadResult = component_.instance->unload(UnloadApi(kernel_.getSubject(), config_, this));)
  terminateIfError(unloadResult, "unloading", component_);

  state_.store(ComponentState::unloaded);
}

void Runner::postUnloadCleanup() { component_.instance->postUnloadCleanup(); }

void Runner::signalThreadToStop()
{
  const auto state = state_.load();

  if (state == ComponentState::stopped || state == ComponentState::stopping)
  {
    return;  // nothing to stop
  }

  if (state != ComponentState::running)
  {
    std::string err;
    err.append("invalid state for signalThreadToStop() call on component '");
    err.append(component_.info.name);
    err.append("'");
    throw std::logic_error(err);
  }

  workQueue_.disable();

  state_.store(ComponentState::stopping);
  stopFlag_.store(true);

  bool needsVirtualTime = kernel_.getConfig().getParams().runMode == RunMode::virtualTime ||
                          kernel_.getConfig().getParams().runMode == RunMode::virtualTimeRunning;

  if (needsVirtualTime && !component_.instance->isRealTimeOnly())
  {
    try
    {
      barrierForWorker_.set_value();  // unblock the worker
    }
    catch (const std::future_error& err)
    {
      std::ignore = err;
    }
  }
}

void Runner::stopThread()
{
  if (const auto state = state_.load(); state == ComponentState::stopped)
  {
    return;  // nothing to stop
  }

  signalThreadToStop();

  if (!os_.joinThread(thread_))
  {
    toErrorState(ExecError {ErrorCategory::runtimeError, "error joining thread"});
    SEN_UNREACHABLE();
    return;
  }

  state_.store(ComponentState::stopped);

  // clear local participants
  localParticipants_.clear();

  // clear objects
  objectsMap_.clear();
  objectsList_.clear();

  serializableEvents_.clear();
  workQueue_.clear();
}

void Runner::doRun()
{
  tracer_ = kernel_.makeTracer(name_);

  workQueue_.enable();

  FuncResult runResult = ::sen::Ok();

  // NOLINTNEXTLINE
  EXCEPTION_WRAP_BLOCK(runResult = component_.instance->run(runApi_);)
  serializableEvents_.clear();
  terminateIfError(runResult, "running", component_);
}

void Runner::threadFunction(void* arg) { static_cast<Runner*>(arg)->doRun(); }

void Runner::toErrorState(const ExecError& err)
{
  spdlog::error(err.explanation);
  lastError_ = err;
  state_.store(ComponentState::error);
  spdlog::dump_backtrace();
  std::terminate();
}

void Runner::registerObjects(Span<std::shared_ptr<NativeObject>> instances)
{
  for (const auto& object: instances)
  {
    objectsList_.emplace_back(object.get());
    objectsMap_.emplace(object, std::prev(objectsList_.end()));

    object->setQueues(&workQueue_, &serializableEvents_);

    // we use the wallclock time here to avoid collisions when running in
    // virtualized time mode. Registration time is not accessible to users.
    object->setRegistrationTime(TimeStamp {Duration {std::chrono::system_clock::now().time_since_epoch()}});

    // notify the user (this can trigger property changes, events, additions, removals, etc.)
    object->registered(registrationApi_);

    // ensure all properties are current
    object->commit(time_);

    // let the kernel know about the type
    kernel_.getTypes().add(object->getClass());

    if (object->needsPreDrainOrPreCommit())
    {
      objectsThatNeedPreAndPostUpdateCalls_.push_back(object.get());
    }

    ++objectCount_;
  }
}

void Runner::unregisterObjects(Span<std::shared_ptr<NativeObject>> instances)
{
  for (const auto& object: instances)
  {
    // remove object from the map and the list
    if (auto it = objectsMap_.find(object); it != objectsMap_.end())
    {
      object->setQueues(nullptr, nullptr);
      object->unregistered(registrationApi_);
      object->setRegistrationTime({});
      objectsList_.erase(it->second);
      objectsMap_.erase(it);

      if (object->needsPreDrainOrPreCommit())
      {
        auto& list = objectsThatNeedPreAndPostUpdateCalls_;
        if (auto elem = std::find(list.begin(), list.end(), object.get()); elem != list.end())
        {
          list.erase(elem);
        }
      }
      --objectCount_;
    }
  }
}

void Runner::registerType(ConstTypeHandle<> type) { kernel_.getTypes().add(type); }

std::optional<Duration> Runner::getCycleTime() const noexcept { return cycleTime_; }

FuncResult Runner::execLoop(Duration cycleTime, std::function<void()>&& workFunction, bool logOverruns)
{
  cycleTime_ = cycleTime;

  bool needsVirtualTime = kernel_.getConfig().getParams().runMode == RunMode::virtualTime ||
                          kernel_.getConfig().getParams().runMode == RunMode::virtualTimeRunning;

  if (needsVirtualTime && !component_.instance->isRealTimeOnly())
  {
    virtualTimeExecLoop(std::move(workFunction));
  }
  else
  {
    realTimeExecLoop(std::move(workFunction), logOverruns);
  }

  return Ok();
}

void Runner::virtualTimeExecLoop(std::function<void()>&& workFunction)
{
  // set the time values
  time_ = TimeStamp {};
  startTime_ = {};
  const auto cycleTime = cycleTime_.value();

  bool updated = false;

  while (true)
  {
    // wait until we are told we need to continue

    barrierForWorker_.get_future().wait();
    barrierForWorker_ = {};

    // stop if signaled
    if (stopFlag_)
    {
      break;
    }

    switch (workerCommand_)
    {
      case WorkerCommand::commandTimeAdvance:
      {
        nextExecutionDeltaTime_ -= targetVirtualTime_ - time_;
        time_ = targetVirtualTime_;

        if (nextExecutionDeltaTime_ <= 0)
        {
          drainInputs();
          update();
          updated = true;
          workFunction();
        }
      }
      break;

      case WorkerCommand::commandCommit:
      {
        // commit only if the state has been previously updated when advancing time
        if (updated)
        {
          updated = false;

          // Delta time until the next execution calculated as the cycleTime (1/freq)
          // minus the time advanced from the execution of the last cycle
          nextExecutionDeltaTime_ = cycleTime.get() - (time_->sinceEpoch().get() % cycleTime.get());
          commit();
        }
      }
      break;
    }

    // notify the caller we are done
    try
    {
      barrierForCaller_.set_value();
    }
    catch (const std::future_error& err)
    {
      std::ignore = err;
    }
  }
}

void Runner::realTimeExecLoop(std::function<void()>&& workFunction, bool logOverruns)
{
  auto logger = KernelImpl::getKernelLogger();

  WallClock wallClock {getComponentContext().config.sleepPolicy};

  PrecisionSleeper sleeper {wallClock, component_.info.name};

  const auto startOfSchedule = wallClock.highResNow();
  const auto period = NanoSecs(cycleTime_.value().getNanoseconds());
  const auto halfPeriod = period / 2;

  auto nextCalibrationTime = startOfSchedule + wallClock.getCalibrateIntervalNs();

  // set the time values
  auto time64 = startOfSchedule;
  time_ = TimeStamp(time64);
  startTime_ = TimeStamp(startOfSchedule);

  const bool isKernel = (name_ == "kernel");
  std::string nameToUse;
  if (!isKernel)
  {
    nameToUse = name_;
  }

  while (!stopFlag_)
  {
    tracer_->frameStart(nameToUse);

    NanoSecs execDuration;
    {
      auto execStartThreadCpuTime = getThreadCpuUserTime();
      exec(workFunction);
      execDuration = getThreadCpuUserTime() - execStartThreadCpuTime;
    }
    const auto workEnd = wallClock.highResNow();

    if (execDuration > period)
    {
      tracer_->message(overrunMessage_);

      if (SEN_LIKELY(logOverruns))
      {
        SPDLOG_LOGGER_WARN(logger, overrunMessage_);
      }
    }

    // time64 now marks the next cycle
    time64 += period;

    bool missedFrameEnd = false;

    // check if we need to skip cycles
    while (workEnd > time64)
    {
      missedFrameEnd = true;
      time64 += period;
    }

    if (missedFrameEnd)
    {
      tracer_->message(missedFrameEndMessage_);
    }

    // calibrate the clock from time to time
    if (workEnd >= nextCalibrationTime)
    {
      wallClock.calibrate();
      nextCalibrationTime = workEnd + wallClock.getCalibrateIntervalNs();
    }

  doSleep:
    // sleep until nextCycleStart
    sleeper.sleep(static_cast<std::chrono::nanoseconds>(time64 - wallClock.highResNow()));
    const auto wakeUpTime = wallClock.highResNow();

    tracer_->plot(oversleepPlotName_, (wakeUpTime - time64).count());

    // check if we missed one or more cycles while sleeping
    bool skippedFrames = false;

    // jump over the cycles we missed
    while (wakeUpTime - time64 > period)
    {
      skippedFrames = true;
      time64 += period;
    }

    if (skippedFrames)
    {
      tracer_->message(oversleptMessage_);

      // check if we have enough time to run. If not, sleep until the next cycle
      if (wakeUpTime - time64 > halfPeriod)
      {
        time64 += period;
        goto doSleep;  // NOLINT(hicpp-avoid-goto)
      }
    }

    time_ = TimeStamp(time64);
    tracer_->frameEnd(nameToUse);
  }
}

std::shared_ptr<SessionInfoProvider> Runner::makeSessionInfoProvider(const std::string& sessionName)
{
  return kernel_.getSessionManager().getOrOpenSession(sessionName)->makeInfoProvider();
}

std::shared_ptr<ObjectSource> Runner::getOrCreateLocalParticipant(const BusAddress& busAddress)
{
  for (auto& participant: localParticipants_)
  {
    if (auto p = participant.lock(); p && p->getBusAddress() == busAddress)
    {
      return p;
    }
  }

  auto session = kernel_.getSessionManager().getOrOpenSession(busAddress.sessionName);
  auto ptr = std::make_shared<LocalParticipant>(idGenerator_().getHash32(), busAddress, session, this, workQueue_);
  localParticipants_.push_back(ptr->weak_from_this());
  ptr->connect();

  return ptr;
}

void Runner::localParticipantDeleted(LocalParticipant* participant)
{
  localParticipants_.erase(std::remove_if(localParticipants_.begin(),
                                          localParticipants_.end(),
                                          [participant](auto& elem)
                                          {
                                            auto p = elem.lock();
                                            return !p or p.get() == participant;
                                          }),
                           localParticipants_.end());
}

}  // namespace sen::kernel::impl
