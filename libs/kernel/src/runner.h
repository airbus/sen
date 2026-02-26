// === runner.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_RUNNER_H
#define SEN_LIBS_KERNEL_SRC_RUNNER_H

// sen libraries
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/mutex_utils.h"
#include "sen/core/base/span.h"
#include "sen/core/base/strong_type.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/base/uuid.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/detail/native_object_impl.h"
#include "sen/core/obj/detail/proxy_object.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object_provider.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/kernel_config.h"
#include "sen/kernel/source_info.h"

// implementation
#include "bus/local_participant.h"
#include "operating_system.h"
#include "precision_sleeper.h"
#include "thread.h"

// generated
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <list>
#include <memory>
#include <string>
#include <vector>

namespace sen::kernel::impl
{

/// Manages the component execution.
class Runner
{
  SEN_NOCOPY_NOMOVE(Runner)

public:
  Runner(KernelImpl& kernel, OperatingSystem& os, ComponentContext component, uint32_t group, VarMap config) noexcept;

  ~Runner() = default;

public:
  /// Calls preload() to the internal component.
  /// Does nothing if the state is 'preloaded' and fails if the
  /// state is not 'unloaded'.
  [[nodiscard]] FuncResult preload();

  /// Calls load() to the internal component.
  /// Does nothing if the state is 'loaded' and fails if the
  /// state is not 'preloaded'.
  [[nodiscard]] FuncResult load();

  /// Calls init on the internal executor.
  /// Does nothing if the state is 'initialized' and fails if the
  /// state is not 'loaded'
  [[nodiscard]] PassResult init();

  /// Allows the internal thread to execute the component.
  /// Does nothing if the state is 'running' and fails if the
  /// state is not 'initialized'.
  [[nodiscard]] FuncResult run();

  /// Calls unload() on the internal component.
  /// Does nothing if the state is 'unloaded'.
  void unload();

  /// Calls postUnloadCleanup on the internal component.
  void postUnloadCleanup();

  /// Requests the component to stop execution and waits for its completion.
  /// Does nothing if the state is 'stopped' or 'error' and fails if the
  /// state is not 'running'. @see kill_thread().
  void stopThread();

  /// The state of the internal component.
  [[nodiscard]] ComponentState getState() const noexcept { return state_; }

  /// The component group
  [[nodiscard]] uint32_t getGroup() const noexcept { return group_; }

  /// The component name
  [[nodiscard]] const ComponentContext& getComponentContext() const noexcept { return component_; }

  /// Perform any request coming from the outside and update all the local
  /// data structures with their most up-to-date value.
  void drainInputs();

  /// Waits for the next available call in the work queue.
  void drainUntilEventOrTimeout(const Duration& timeout);

  /// Calls update() on all registered objects
  void update();

  /// Send changes, so that they become visible to other participants.
  /// This includes object additions and removals, property changes and
  /// emitted events that others might have interest in.
  void commit();

  /// Structured helper for executing in cycles.
  FuncResult execLoop(Duration cycleTime, std::function<void()>&& workFunction = {}, bool logOverruns = true);

  /// Advances the virtual time a given delta (only meaningful when in virtual time mode).
  std::future<void> advanceVirtualTime(const Duration& delta);

  /// Commits all the objects when in virtual time mode.
  std::future<void> commitVirtualTime();

  /// Gets the remaining time until the next execution cycle
  [[nodiscard]] Duration getNextExecutionDeltaTime() const;

  /// Gets or creates an object source.
  [[nodiscard]] std::shared_ptr<ObjectSource> getOrCreateLocalParticipant(const BusAddress& busAddress);

  /// The object used to discover sessions in this runner
  SessionsDiscoverer& getSessionsDiscoverer() noexcept { return sessionsDiscoverer_; }

  /// Creates an info provider for a given session.
  /// This method triggers the connection to the session (if not already connected)
  [[nodiscard]] std::shared_ptr<SessionInfoProvider> makeSessionInfoProvider(const std::string& sessionName);

  /// Removes a deleted local participant from the runner.
  void localParticipantDeleted(LocalParticipant* participant);

  /// The queue used to push work to this runner
  [[nodiscard]] ::sen::impl::WorkQueue* getWorkQueue() noexcept { return &workQueue_; }

  /// The queue used to store events for serialization
  [[nodiscard]] ::sen::impl::SerializableEventQueue& getSerializableEvents() noexcept { return serializableEvents_; }

  /// The number of registered objects
  [[nodiscard]] std::size_t getObjectCount() const noexcept { return objectCount_; }

  /// Configures the objects to work in this runner
  void registerObjects(Span<std::shared_ptr<NativeObject>> instances);

  /// Configures the objects to not work in this runner
  void unregisterObjects(Span<std::shared_ptr<NativeObject>> instances);

  /// Registers a type into the kernel. Does nothing if null or already present
  void registerType(ConstTypeHandle<> type);

  /// The configured cycle time (if any).
  [[nodiscard]] std::optional<Duration> getCycleTime() const noexcept;

  /// Get the tracer for this runner.
  [[nodiscard]] Tracer& getTracer() const noexcept { return *tracer_; }

  /// Get the starting time of this runner.
  [[nodiscard]] TimeStamp getStartTime() const noexcept { return startTime_; }

private:
  /// Called by 'threadFunction'.
  /// This function simply calls the component's run() method.
  void doRun();

  /// Called by the internal thread.
  static void threadFunction(void* arg);

  /// Sets lastError_ and sets the component state as error;
  [[noreturn]] void toErrorState(const ExecError& err);

  /// Requests the component to stop execution.
  /// Does nothing if the state is 'stopped' or 'error' and fails if the
  /// state is not 'running'.
  void signalThreadToStop();

private:
  void exec(std::function<void()>& workFunction);
  void realTimeExecLoop(std::function<void()>&& workFunction, bool logOverruns);
  void virtualTimeExecLoop(std::function<void()>&& workFunction);

private:
  enum class WorkerCommand
  {
    commandTimeAdvance,
    commandCommit,
  };

private:
  using NativeObjectPtrList = std::list<NativeObject*>;

  KernelImpl& kernel_;
  OperatingSystem& os_;
  ComponentContext component_;
  std::atomic<ComponentState> state_;
  std::atomic_bool stopFlag_;
  Thread thread_ {nullptr};
  ExecError lastError_;
  uint32_t group_;
  VarMap config_;
  ::sen::impl::WorkQueue workQueue_;
  ::sen::impl::SerializableEventQueue serializableEvents_;
  std::mutex serializableEventsMutex_;  // TODO: needed for clear and participants
  std::vector<std::weak_ptr<LocalParticipant>> localParticipants_;
  std::unordered_map<std::shared_ptr<NativeObject>, NativeObjectPtrList::iterator> objectsMap_;
  NativeObjectPtrList objectsList_;
  NativeObjectPtrList objectsThatNeedPreAndPostUpdateCalls_;
  Guarded<TimeStamp> time_;
  TimeStamp startTime_;
  RunApi runApi_;
  RegistrationApi registrationApi_;
  SessionsDiscoverer sessionsDiscoverer_;
  std::promise<void> barrierForWorker_;
  std::promise<void> barrierForCaller_;
  TimeStamp targetVirtualTime_;
  WorkerCommand workerCommand_ = WorkerCommand::commandTimeAdvance;
  Duration nextExecutionDeltaTime_;
  std::optional<Duration> cycleTime_;
  std::atomic<std::size_t> objectCount_ = 0U;
  std::string name_;
  std::string oversleptMessage_;
  std::string oversleepPlotName_;
  std::string overrunMessage_;
  std::string missedFrameEndMessage_;
  std::string workQueueName_;
  std::string eventQueueName_;
  std::unique_ptr<Tracer> tracer_;
  ::sen::UuidRandomGenerator idGenerator_;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

[[nodiscard]] inline Duration roundUpToMilliseconds(Duration duration)
{
  constexpr auto millisecond = Duration(std::chrono::milliseconds {1}).getNanoseconds();
  constexpr auto millisecondMinusOneNano = millisecond - 1;
  const auto nanoseconds = duration.getNanoseconds();
  return (nanoseconds + millisecondMinusOneNano) - ((nanoseconds + millisecondMinusOneNano) % millisecond);
}

inline void Runner::drainInputs()
{
  SEN_TRACE_ZONE(*tracer_);

  for (auto& obj: objectsThatNeedPreAndPostUpdateCalls_)
  {
    obj->preDrain();
  }

  sessionsDiscoverer_.drainInputs();

  // drain the inputs on all our participants
  // explicit copy to prevent modifications
  auto localParticipants = localParticipants_;
  for (auto& participant: localParticipants)
  {
    if (auto p = participant.lock(); p)
    {
      p->drainInputs();
    }
  }

  // plot the number of work elements to execute
  tracer_->plot(workQueueName_, static_cast<int64_t>(workQueue_.getCurrentSize()));

  // do any pending work
  std::ignore = workQueue_.executeAll();
}

inline void Runner::drainUntilEventOrTimeout(const Duration& timeout)
{
  sessionsDiscoverer_.drainInputs();

  // drain the inputs on all our participants
  // explicit copy to prevent modifications
  auto localParticipants = localParticipants_;
  for (auto& participant: localParticipants)
  {
    if (auto p = participant.lock(); p)
    {
      p->drainInputs();
    }
  }

  workQueue_.waitExecuteAll(timeout);
  time_ = TimeStamp(std::chrono::system_clock::now().time_since_epoch());
}

inline void Runner::update()
{
  SEN_TRACE_ZONE(*tracer_);

  for (auto* obj: objectsList_)
  {
    SEN_TRACE_ZONE_NAMED(*tracer_, "object update");
    obj->update(runApi_);
  }
}

inline void Runner::commit()
{
  SEN_TRACE_ZONE(*tracer_);

  {
    SEN_TRACE_ZONE_NAMED(*tracer_, "pre-commit");

    for (auto& obj: objectsThatNeedPreAndPostUpdateCalls_)
    {
      obj->preCommit();
    }
  }

  // commit the state of local objects, to process remote changes. This:
  //  - advances the property buffers, makes the object emit property-related events
  //  - makes the object emit any post-commit events (handled on commit)
  {
    SEN_TRACE_ZONE_NAMED(*tracer_, "commit");

    for (auto* obj: objectsList_)
    {
      obj->commit(time_);
    }
  }

  // plot the number of events produced during commit
  tracer_->plot(eventQueueName_, static_cast<int64_t>(serializableEvents_.getContents().size()));

  // call commit on all local participants
  // explicit copy to prevent modifications
  {
    SEN_TRACE_ZONE_NAMED(*tracer_, "flush");

    auto localParticipants = localParticipants_;
    for (auto& participant: localParticipants)
    {
      if (auto p = participant.lock(); p)
      {
        p->flushOutputs();
      }
    }
  }

  serializableEvents_.clear();
}

inline void Runner::exec(std::function<void()>& workFunction)
{
  // drain the inputs
  drainInputs();

  // let objects update their state
  update();
  {
    SEN_TRACE_ZONE_NAMED(*tracer_, "work");

    // do user work, if any
    workFunction();
  }

  // send changes to listeners
  commit();
}

inline std::future<void> Runner::advanceVirtualTime(const Duration& delta)
{
  barrierForCaller_ = {};
  auto future = barrierForCaller_.get_future();  // the future where the caller will wait

  // command and unblock the worker
  {
    targetVirtualTime_ = time_ + delta;
    workerCommand_ = WorkerCommand::commandTimeAdvance;
    barrierForWorker_.set_value();
  }
  return future;
}

inline std::future<void> Runner::commitVirtualTime()
{
  barrierForCaller_ = {};
  auto future = barrierForCaller_.get_future();  // the future where the caller will wait

  // command and unblock the worker
  {
    workerCommand_ = WorkerCommand::commandCommit;
    barrierForWorker_.set_value();
  }
  return future;
}

inline Duration Runner::getNextExecutionDeltaTime() const
{
  // Round value up to avoid timing precision issues with recurring decimal periods (1/freq)
  return roundUpToMilliseconds(nextExecutionDeltaTime_);
}

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_RUNNER_H
