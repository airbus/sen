// === runner.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "runner.h"

#include "schedule_cycles.h"

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
#include "sen/core/obj/object_provider.h"
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
#include <atomic>
#include <chrono>
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
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

// OS
#if defined(__unix__) || defined(__APPLE__)
#  include <ctime>
#endif

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

}  // namespace

/// The calling thread's CPU time, user and system together. System time counts because it comes
/// out of the same period: writing to the transport uses the cycle up like computing does.
[[nodiscard]] NanoSecs getThreadCpuTime() noexcept
{
#if defined(__unix__) || defined(__APPLE__)
  // Per thread on every platform. getrusage has no thread scope on macOS, where it can only
  // answer for the whole process.
  timespec threadTime {};
  // NOLINTNEXTLINE(misc-include-cleaner) POSIX declares it, and <time.h> is deprecated in C++
  clock_gettime(CLOCK_THREAD_CPUTIME_ID, &threadTime);
  return std::chrono::seconds(threadTime.tv_sec) + std::chrono::nanoseconds(threadTime.tv_nsec);
#elif defined(_WIN32)
  FILETIME creation {};
  FILETIME exitTime {};
  FILETIME kernelTime {};
  FILETIME usrTime {};
  if (GetThreadTimes(GetCurrentThread(), &creation, &exitTime, &kernelTime, &usrTime) == 0)
  {
    return NanoSecs {0};
  }

  // These two are elapsed durations in 100-nanosecond units, not absolute times, so no epoch
  // adjustment applies to them.
  const auto toNanoSecs = [](const FILETIME& value)
  {
    const auto high = static_cast<uint64_t>(value.dwHighDateTime);
    const auto low = static_cast<uint64_t>(value.dwLowDateTime);
    return ((high << 32U) | low) * 100U;
  };
  return std::chrono::nanoseconds(toNanoSecs(usrTime) + toNanoSecs(kernelTime));
#else
#  error "OS support not implemented"
#endif
}

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
  if (threadCreationResult.isOk() && !threadCreationResult.getValue().priorityApplied)
  {
    // Running without the configured priority beats refusing to start, but the component is not
    // scheduled the way it was configured.
    SPDLOG_LOGGER_WARN(KernelImpl::getKernelLogger(),
                       "component '{}' runs without its configured priority: the operating system "
                       "refused it",
                       component_.info.name);
  }

  if (threadCreationResult.isOk() && !threadCreationResult.getValue().affinityApplied)
  {
    // Same reasoning: a mask the machine cannot honour used to be ignored in silence.
    SPDLOG_LOGGER_WARN(KernelImpl::getKernelLogger(),
                       "component '{}' runs unpinned: the configured cpu affinity could not be applied",
                       component_.info.name);
  }

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

  if (runsVirtualTimeLoop())
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

bool Runner::isCurrentThread() const noexcept { return os_.isCurrentThread(thread_); }

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

  for (auto& participant: localParticipants_)
  {
    if (auto p = participant.lock())
    {
      p->markTornDown();
    }
  }

  // clear local participants
  for (auto it = localParticipants_.begin(); it != localParticipants_.end();)
  {
    localParticipants_.erase(it);
    it = localParticipants_.begin();
  }

  // The pre-drain list holds raw pointers to objects the map's erase destroys, and a count left at
  // its pre-stop value is read by fetchMonitoringInfo() for as long as the runner exists.
  // The raw-pointer lists go before the owning map: clearing the map destroys the objects, and
  // those lists would hold dangling pointers in between.
  objectsList_.clear();
  objectsThatNeedPreAndPostUpdateCalls_.clear();
  objectsMap_.clear();
  objectCount_ = 0U;

  serializableEvents_.clear();
  workQueue_.clear();
}

ComponentMonitoringInfo Runner::fetchMonitoringInfo() const
{
  ComponentMonitoringInfo result;

  result.name = component_.info.name;
  result.group = group_;
  result.requiresRealTime = component_.instance->isRealTimeOnly();
  result.objectCount = objectCount_;
  result.cycleTime = getCycleTime();
  result.lastCycleExecutionCpuTime = getLastCycleExecutionCpuTime();
  result.lastCycleComponentCpuTime = getLastCycleComponentCpuTime();
  const auto worst = getWorstCycle();
  result.worstCycleExecutionCpuTime = worst.execution;
  result.worstCycleComponentCpuTime = worst.component;
  result.lastCycleQueuedWorkCpuTime = getLastCycleQueuedWorkCpuTime();

  // Only the real-time loop has a schedule to miss, and a cycle time means execLoop ran. A
  // component driving its own loop would otherwise report zeros for cycles nobody counted.
  if (!runsVirtualTimeLoop() && result.cycleTime.has_value())
  {
    result.overrunCount = getOverrunCount();
    result.missedFrameCount = getMissedFrameCount();
    result.oversleptCount = getOversleptCount();
    result.lastCycleStartDelay = getLastCycleStartDelay();
    result.worstStartDelay = getWorstStartDelay();
  }

  return result;
}

void Runner::doRun()
{
  tracer_ = kernel_.makeTracer(name_);

  workQueue_.enable();

  FuncResult runResult = ::sen::Ok();

  // We need to initialize time before starting to execute, to ensure everything, even initialize published objects see
  // the correct starting time.
  initializeTime();
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

void Runner::registerObjects(ObjectOwnerId from, Span<std::shared_ptr<NativeObject>> instances)
{
  // Copied so the loop does not depend on the caller's storage outliving the hooks it runs:
  // registered() and the pre-drain query below are user code and can re-enter the participant.
  const std::vector<std::shared_ptr<NativeObject>> batch(instances.begin(), instances.end());

  // A hook belonging to one member can remove a later member, which reaches unregisterObjects
  // before this loop has registered it. That removal finds nothing, so it records the object here
  // and the loop honours it by never registering it: added and removed before the add took effect
  // means nothing happened, and no hook runs for it.
  std::unordered_set<NativeObject*> cancelled;
  const ScopedCancelBatch scopedBatch(cancelBatchesInFlight_, from, batch, cancelled);

  for (const auto& object: batch)
  {
    // A lookup, not an erase: a batch holding one object twice would otherwise cancel only its
    // first occurrence. LocalParticipant::add cannot produce such a batch, and this does not rely
    // on that.
    if (cancelled.count(object.get()) != 0U)
    {
      continue;
    }

    objectsList_.emplace_back(object.get());
    const auto node = std::prev(objectsList_.end());
    const auto serial = ++registrationSerial_;

    // An already registered object would leave this node reachable from no map entry, and the
    // cycle walks the list, so it would be updated twice and then dangle.
    if (!objectsMap_.emplace(object, ObjectRegistration {node, serial}).second)
    {
      // Uniform with the other erase from this list. Nothing between the emplace_back above and
      // here can move a cursor, so this steps nothing today; relying on that would make the erase
      // correct only while these lines stay free of anything that calls user code.
      stepCursorsPast(objectsList_, node);
      objectsList_.erase(node);
      spdlog::debug("Object '{}' is already registered with runner '{}'; ignoring the second registration.",
                    object->getName(),
                    name_);
      continue;
    }
    ++objectCount_;

    object->setQueues(&workQueue_, &serializableEvents_);

    // we use the wallclock time here to avoid collisions when running in
    // virtualized time mode. Registration time is not accessible to users.
    object->setRegistrationTime(TimeStamp {Duration {std::chrono::system_clock::now().time_since_epoch()}});

    // let the kernel know about the type before any user code runs: add() is documented as
    // registering the object including its type, and a hook that removes the object must not
    // leave that half undone.
    kernel_.getTypes().add(object->getClass());

    // notify the user (this can trigger property changes, events, additions, removals, etc.)
    object->registered(registrationApi_);

    // registered() can remove this object, or remove and add it again. A second add registers it
    // in full, so carrying on would commit it twice. The serial identifies our own registration:
    // a removal erases the list node, so an iterator to it could match a reused address. The
    // count is raised at the insert above so both paths balance.
    if (const auto itr = objectsMap_.find(object); itr == objectsMap_.end() || itr->second.serial != serial)
    {
      continue;
    }

    // ensure all properties are current
    object->commit(time_);

    // Virtual, so an override can remove the object, and asked before the check below so the check
    // covers it. Asked after registered() rather than before, because an override may be deciding
    // its answer in that hook.
    const bool wantsPreDrainOrPreCommit = object->needsPreDrainOrPreCommit();

    // Asked again so it covers either statement above removing the object: commit() dispatches
    // properties and needsPreDrainOrPreCommit() is virtual.
    if (const auto itr = objectsMap_.find(object); itr == objectsMap_.end() || itr->second.serial != serial)
    {
      continue;
    }

    if (wantsPreDrainOrPreCommit)
    {
      objectsThatNeedPreAndPostUpdateCalls_.push_back(object.get());
    }
  }
}

void Runner::unregisterObjects(ObjectOwnerId from, Span<std::shared_ptr<NativeObject>> instances)
{
  // Copied so the loop does not depend on the caller's storage outliving the hooks it runs, and so
  // each object stays alive across its own unregistered(), which the map's key stops doing once its
  // entry is erased.
  const std::vector<std::shared_ptr<NativeObject>> batch(instances.begin(), instances.end());

  for (const auto& object: batch)
  {
    const auto it = objectsMap_.find(object);
    if (it == objectsMap_.end())
    {
      // Not registered yet, so this is a removal of a member of a batch still being registered.
      // Recording it cancels that member, rather than letting the registration loop carry on and
      // register an object the participant has already unstaged and whose name it has freed.
      // Only a batch opened by the participant this removal came through. The miss above has two
      // meanings, and membership does not separate them -- a removal sent through another bus of
      // this runner names a genuine member of the batch. Cancelling on that would stop a
      // registration whose own participant never asked for it to stop, leaving the object published
      // by that participant and registered by nobody.
      for (const auto& inFlight: cancelBatchesInFlight_)
      {
        if (inFlight.owner != from)
        {
          continue;
        }

        if (std::find(inFlight.members->begin(), inFlight.members->end(), object) != inFlight.members->end())
        {
          inFlight.cancelled->insert(object.get());
        }
      }
      continue;
    }

    // The map iterator must not outlive unregistered(), because the hook can add objects and a
    // rehash would invalidate it, and taking the entry out first is what stops a hook removing this
    // object again from re-entering. A fresh keyed lookup after the hook is still fine.
    object->setQueues(nullptr, nullptr);

    // A cycle walk may be standing on this node: its own hook is what called us, and a walk holds
    // its cursor on the element whose hook is running, so the node being freed can be that object
    // or any other the hook asked to remove.
    stepCursorsPast(objectsList_, it->second.node);
    objectsList_.erase(it->second.node);
    objectsMap_.erase(it);

    // Unconditional, because needsPreDrainOrPreCommit() can answer differently now than it did at
    // registration and the entry to erase is the one that earlier answer added. Every match rather
    // than the first, because the list holds raw pointers and a stale entry would otherwise match a
    // later object allocated at the same address. Written out rather than std::list::remove so the
    // pre-drain walks get their cursors stepped off each node.
    for (auto itr = objectsThatNeedPreAndPostUpdateCalls_.begin(); itr != objectsThatNeedPreAndPostUpdateCalls_.end();)
    {
      if (*itr != object.get())
      {
        ++itr;
        continue;
      }

      stepCursorsPast(objectsThatNeedPreAndPostUpdateCalls_, itr);
      itr = objectsThatNeedPreAndPostUpdateCalls_.erase(itr);
    }

    --objectCount_;

    object->unregistered(registrationApi_);

    // Clearing the stamp before the hook would leave it zero for as long as user code runs, and a
    // zero stamp wins the duplicate-name arbitration rather than losing it, so this side claims
    // seniority it does not have and the repeated name is settled on neither side. Our own entry
    // went before the hook and the serial only rises, so an entry here now is a newer registration
    // whose stamp is not ours to clear.
    if (objectsMap_.find(object) == objectsMap_.end())
    {
      object->setRegistrationTime({});
    }
  }
}

void Runner::registerType(ConstTypeHandle<> type) { kernel_.getTypes().add(type); }

std::optional<Duration> Runner::getCycleTime() const noexcept
{
  const auto cycleTime = cycleTime_.load(std::memory_order_relaxed);
  return (cycleTime == noDuration) ? std::nullopt : std::make_optional(Duration {cycleTime});
}

std::optional<Duration> Runner::getLastCycleExecutionCpuTime() const noexcept
{
  const auto lastTime = lastCycleExecutionCpuTime_.load(std::memory_order_relaxed);
  return (lastTime == noDuration) ? std::nullopt : std::make_optional(Duration {lastTime});
}

std::optional<Duration> Runner::getLastCycleComponentCpuTime() const noexcept
{
  const auto lastTime = lastCycleComponentCpuTime_.load(std::memory_order_relaxed);
  return (lastTime == noDuration) ? std::nullopt : std::make_optional(Duration {lastTime});
}

std::optional<Duration> Runner::getLastCycleQueuedWorkCpuTime() const noexcept
{
  const auto lastTime = lastCycleQueuedWorkCpuTime_.load(std::memory_order_relaxed);
  return (lastTime == noDuration) ? std::nullopt : std::make_optional(Duration {lastTime});
}

std::optional<Duration> Runner::getWorstStartDelay() const noexcept
{
  const auto delay = worstStartDelay_.load(std::memory_order_relaxed);
  return (delay == noStartDelay) ? std::nullopt : std::make_optional(Duration {delay});
}

std::optional<Duration> Runner::getLastCycleStartDelay() const noexcept
{
  const auto delay = lastCycleStartDelay_.load(std::memory_order_relaxed);
  return (delay == noStartDelay) ? std::nullopt : std::make_optional(Duration {delay});
}

/// Reads the pair and checks the version did not move, so both halves come from one cycle. Read
/// without that check, a share could come from a later cycle than the total it sits inside.
WorstCycle Runner::getWorstCycle() const noexcept
{
  // Bounded, not spun. The writer is a component thread on a real-time policy with an affinity,
  // so a reader on the same core can leave it unable to finish. Empty beats hanging the caller.
  constexpr int maxAttempts = 100;

  for (int attempt = 0; attempt < maxAttempts; ++attempt)
  {
    const auto version = worstCycleVersion_.load(std::memory_order_acquire);
    if ((version % 2U) != 0U)
    {
      // a write is in progress, so what is there now is half of one cycle and half of another
      continue;
    }

    const auto worstExecution = worstCycleExecutionCpuTime_.load(std::memory_order_relaxed);
    const auto worstComponent = worstCycleComponentCpuTime_.load(std::memory_order_relaxed);

    std::atomic_thread_fence(std::memory_order_acquire);
    if (worstCycleVersion_.load(std::memory_order_relaxed) != version)
    {
      continue;
    }

    WorstCycle worst;
    worst.execution = (worstExecution == noDuration) ? std::nullopt : std::make_optional(Duration {worstExecution});
    worst.component = (worstComponent == noDuration) ? std::nullopt : std::make_optional(Duration {worstComponent});
    return worst;
  }

  return {};
}

/// Keeps the worst cycle seen so far, as a pair taken from one cycle. The runner's own thread is
/// the only writer, so the odd version is only ever seen by a reader.
void Runner::recordWorstCycle(NanoSecs executionCpuTime, NanoSecs componentCpuTime) noexcept
{
  if (executionCpuTime.count() <= worstCycleExecutionCpuTime_.load(std::memory_order_relaxed))
  {
    return;
  }

  const auto version = worstCycleVersion_.load(std::memory_order_relaxed);
  worstCycleVersion_.store(version + 1U, std::memory_order_relaxed);
  std::atomic_thread_fence(std::memory_order_release);
  worstCycleComponentCpuTime_.store(componentCpuTime.count(), std::memory_order_relaxed);
  worstCycleExecutionCpuTime_.store(executionCpuTime.count(), std::memory_order_relaxed);
  worstCycleVersion_.store(version + 2U, std::memory_order_release);
}

uint64_t Runner::getOverrunCount() const noexcept { return overrunCount_.load(std::memory_order_relaxed); }

uint64_t Runner::getMissedFrameCount() const noexcept { return missedFrameCount_.load(std::memory_order_relaxed); }

uint64_t Runner::getOversleptCount() const noexcept { return oversleptCount_.load(std::memory_order_relaxed); }

FuncResult Runner::execLoop(Duration cycleTime, std::function<void()>&& workFunction, bool logOverruns)
{
  cycleTime_.store(cycleTime.getNanoseconds(), std::memory_order_relaxed);

  if (runsVirtualTimeLoop())
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
  const auto cycleTime = getCycleTime().value();

  bool updated = false;
  bool firstCycle = true;
  NanoSecs executionCpuTime {};
  CycleCpuTime cycleCpuTime {};

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
          const auto executionStartCpuTime = getThreadCpuTime();
          cycleCpuTime.queuedWork = drainInputs();
          const auto componentStartCpuTime = getThreadCpuTime();
          update();
          updated = true;
          workFunction();
          const auto cycleEndCpuTime = getThreadCpuTime();
          cycleCpuTime.component = cycleEndCpuTime - componentStartCpuTime;
          executionCpuTime = cycleEndCpuTime - executionStartCpuTime;
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
          const auto commitStartCpuTime = getThreadCpuTime();
          commit();
          executionCpuTime += getThreadCpuTime() - commitStartCpuTime;
          lastCycleExecutionCpuTime_.store(executionCpuTime.count(), std::memory_order_relaxed);
          lastCycleComponentCpuTime_.store(cycleCpuTime.component.count(), std::memory_order_relaxed);
          lastCycleQueuedWorkCpuTime_.store(cycleCpuTime.queuedWork.count(), std::memory_order_relaxed);

          // not the first cycle: see the real-time loop
          if (!firstCycle)
          {
            recordWorstCycle(executionCpuTime, cycleCpuTime.component);
          }
          firstCycle = false;
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

void Runner::initializeTime()
{
  if (runsVirtualTimeLoop())
  {
    time_ = TimeStamp {};
    startTime_ = {};
  }
  else
  {
    WallClock wallClock {getComponentContext().config.sleepPolicy};
    const auto startOfSchedule = wallClock.highResNow();

    time_ = TimeStamp(startOfSchedule);
    startTime_ = TimeStamp(startOfSchedule);
  }
}

void Runner::realTimeExecLoop(std::function<void()>&& workFunction, bool logOverruns)
{
  auto logger = KernelImpl::getKernelLogger();

  WallClock wallClock {getComponentContext().config.sleepPolicy};

  PrecisionSleeper sleeper {wallClock, component_.info.name};

  const auto period = NanoSecs(getCycleTime().value().getNanoseconds());
  const auto halfPeriod = period / 2;

  const auto startOfSchedule = time_->sinceEpoch().toChrono();
  auto nextCalibrationTime = startOfSchedule + wallClock.getCalibrateIntervalNs();

  auto time64 = startOfSchedule;

  bool firstCycle = true;

  const bool isKernel = (name_ == "kernel");
  std::string nameToUse;
  if (!isKernel)
  {
    nameToUse = name_;
  }

  while (!stopFlag_)
  {
    tracer_->frameStart(nameToUse);

    NanoSecs executionCpuTime;
    CycleCpuTime cycleCpuTime;
    {
      auto execStartThreadCpuTime = getThreadCpuTime();
      exec(workFunction, cycleCpuTime);
      executionCpuTime = getThreadCpuTime() - execStartThreadCpuTime;
    }
    const auto workEnd = wallClock.highResNow();
    lastCycleExecutionCpuTime_.store(executionCpuTime.count(), std::memory_order_relaxed);
    lastCycleComponentCpuTime_.store(cycleCpuTime.component.count(), std::memory_order_relaxed);
    lastCycleQueuedWorkCpuTime_.store(cycleCpuTime.queuedWork.count(), std::memory_order_relaxed);

    // The first cycle carries the component's own startup and everything that queued during it,
    // so it says nothing about the component running.
    if (!firstCycle)
    {
      recordWorstCycle(executionCpuTime, cycleCpuTime.component);
    }

    // CPU time against the period, which is what an overrun means here. A cycle that blocked
    // rather than computed is not one of these, and is reported as a missed frame below.
    if (executionCpuTime > period)
    {
      tracer_->message(overrunMessage_);
      overrunCount_.fetch_add(1U, std::memory_order_relaxed);

      if (SEN_LIKELY(logOverruns))
      {
        SPDLOG_LOGGER_WARN(logger, overrunMessage_);
      }
    }

    // time64 now marks the next cycle
    time64 += period;

    int64_t missedCycles = 0;

    // check if we need to skip cycles. A clock that jumps forward puts the next cycle an arbitrary
    // distance away, so walk to it in one step: period by period is unbounded and stopFlag_ is not
    // read along the way. Only ever forward, as the bus discards updates stamped before the last.
    if (workEnd > time64)
    {
      missedCycles = cyclesToCover(workEnd - time64, period);
      time64 += period * missedCycles;
    }

    if (missedCycles > 0)
    {
      // Every cycle that will not run is counted, not the one event that lost them. The first is
      // skipped: its lateness is the startup before the loop, not cycles the component lost. The
      // warning is logged once either way.
      if (!firstCycle)
      {
        missedFrameCount_.fetch_add(static_cast<uint64_t>(missedCycles), std::memory_order_relaxed);
      }
      tracer_->message(missedFrameEndMessage_);

      if (SEN_LIKELY(logOverruns))
      {
        SPDLOG_LOGGER_WARN(logger, missedFrameEndMessage_);
      }
    }

    // calibrate the clock from time to time. This instant is read from the same clock it
    // schedules, so a backward jump leaves it out of reach and stalls drift correction until wall
    // time catches up.
    const auto calibrateInterval = wallClock.getCalibrateIntervalNs();
    if (nextCalibrationTime > workEnd + calibrateInterval)
    {
      nextCalibrationTime = workEnd + calibrateInterval;
    }

    if (workEnd >= nextCalibrationTime)
    {
      wallClock.calibrate();
      nextCalibrationTime = workEnd + calibrateInterval;
    }

  doSleep:
    // sleep until nextCycleStart, never asking for longer than a cycle. A backward jump makes this
    // difference arbitrarily large, and the sleeper issues it as one call that does not read
    // stopFlag_. This bounds the request, not the sleep: where highResNow is the system clock, a
    // jump landing inside a sleep still holds it for the size of the jump.
    sleeper.sleep(static_cast<std::chrono::nanoseconds>(std::min(time64 - wallClock.highResNow(), period)));
    const auto wakeUpTime = wallClock.highResNow();

    // How late the thread woke for the cycle it was waiting for. The sleeper promises a minimum
    // and no maximum, so this is the scheduler's contribution, kept apart from the component's.
    const auto startDelay = wakeUpTime - time64;
    tracer_->plot(oversleepPlotName_, startDelay.count());
    lastCycleStartDelay_.store(startDelay.count(), std::memory_order_relaxed);

    if (startDelay.count() > worstStartDelay_.load(std::memory_order_relaxed))
    {
      worstStartDelay_.store(startDelay.count(), std::memory_order_relaxed);
    }

    // check if we missed one or more cycles while sleeping
    int64_t oversleptCycles = 0;

    // jump over the cycles we missed, in one step for the same reason as above
    if (wakeUpTime - time64 > period)
    {
      oversleptCycles = cyclesToCover(wakeUpTime - time64 - period, period);
      time64 += period * oversleptCycles;
    }

    if (oversleptCycles > 0)
    {
      oversleptCount_.fetch_add(static_cast<uint64_t>(oversleptCycles), std::memory_order_relaxed);
      tracer_->message(oversleptMessage_);

      if (SEN_LIKELY(logOverruns))
      {
        SPDLOG_LOGGER_WARN(logger, oversleptMessage_);
      }

      // check if we have enough time to run. If not, sleep until the next cycle
      if (wakeUpTime - time64 > halfPeriod)
      {
        // that cycle is dropped too, so it is counted with the ones slept through
        oversleptCount_.fetch_add(1U, std::memory_order_relaxed);
        time64 += period;
        goto doSleep;  // NOLINT(cppcoreguidelines-avoid-goto, hicpp-avoid-goto)
      }
    }

    firstCycle = false;
    time_ = TimeStamp(time64);
    tracer_->frameEnd(nameToUse);
  }
}

bool Runner::runsVirtualTimeLoop() const { return needsVirtualTime() && !component_.instance->isRealTimeOnly(); }

bool Runner::needsVirtualTime() const
{
  return kernel_.getConfig().getParams().runMode == RunMode::virtualTime ||
         kernel_.getConfig().getParams().runMode == RunMode::virtualTimeRunning;
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
  localParticipants_.emplace(ptr);
  ptr->connect();

  return ptr;
}

void Runner::localParticipantDeleted(LocalParticipant* participant)
{
  for (auto it = localParticipants_.begin(); it != localParticipants_.end();)
  {
    bool shouldRemove = false;

    if (auto p = it->lock())
    {
      // remove if the participant introduced as argument matches
      if (p.get() == participant)
      {
        shouldRemove = true;
      }
    }
    else
    {
      // remove expired local participants
      shouldRemove = true;
    }

    if (shouldRemove)
    {
      const auto toRemove = *it;
      ++it;
      localParticipants_.erase(toRemove);
      continue;
    }

    ++it;
  }
}

}  // namespace sen::kernel::impl
