// === component_api.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_KERNEL_COMPONENT_API_H
#define SEN_KERNEL_COMPONENT_API_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/move_only_function.h"
#include "sen/core/base/mutex_utils.h"
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_list.h"
#include "sen/core/obj/object_source.h"
#include "sen/core/obj/subscription.h"

// kernel
#include "sen/kernel/kernel.h"
#include "sen/kernel/source_info.h"
#include "sen/kernel/tracer.h"
#include "sen/kernel/transport.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/network_footprint.stl.h"

// spdlog
#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>

// std
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace sen::kernel
{

/// The result of operations that are called once.
using FuncResult = Result<void, ExecError>;

/// The result of operations that may be called multiple times.
using PassResult = Result<OpState, ExecError>;

/// Callable that builds a network footprint from bus addresses.
using NetworkFootprintReporter = NetworkFootprint(Span<const BusAddress>) const;

class SessionsDiscoverer;
class RunApi;
class KernelApi;

/// Whether the component registering a log sink owns the terminal.
///
/// `shared` leaves console output alone. `owned` silences every console sink, now and on every logger
/// made while the claim lasts: a console sink writes to the descriptor the component is drawing on, so
/// its output would land on top of the display and every line would appear twice. Silencing rather than
/// detaching is what makes it reversible, and `removeLoggerSink` puts every console sink back to the level
/// it had once no registered sink claims the terminal.
///
/// Re-adding a sink that is already registered, with `owned`, claims the terminal for it. A `shared` add
/// never gives a claim up.
enum class TerminalOwnership
{
  shared,
  owned
};

/// What registering a logger sink did.
///
/// There is no arbitration: a sink is added, never refused, so two components that both render logs will
/// each be handed every line, including the other's. Anything that needs exclusivity has to read
/// `registeredSinks` and decide for itself.
struct LoggerSinkRegistration
{
  bool added = false;                   ///< false when this sink was already registered, so the call only
                                        ///< changed its terminal ownership
  bool ownsTerminal = false;            ///< whether this sink now holds the terminal claim
  bool terminalOwnedElsewhere = false;  ///< whether a *different* registered sink holds the claim
  std::size_t registeredSinks = 0;      ///< sinks registered behind the relay, this one included
};

namespace impl
{

class Runner;
class KernelImpl;

[[nodiscard]] FuncResult execLoop(Runner* runner,
                                  Duration cycleTime,
                                  std::function<void()>&& workFunction,
                                  bool logOverruns);

void installTransportFactory(KernelImpl* kernel, TransportFactory&& factory, uint32_t transportVersion);

void remoteProcessDetected(RunApi& api, const ProcessInfo& processInfo);

void remoteProcessLost(RunApi& api, const ProcessInfo& processInfo);

[[nodiscard]] std::shared_ptr<ObjectSource> getSource(Runner* runner, const BusAddress& address);

[[nodiscard]] SessionsDiscoverer& getSessionsDiscoverer(Runner* runner);

[[nodiscard]] const ProcessInfo* fetchOwnerInfo(const Object* object);

[[nodiscard]] ::sen::impl::WorkQueue* getWorkQueue(Runner* runner);

[[nodiscard]] std::shared_ptr<spdlog::logger> getOrCreateLogger(const std::string& loggerName);

void applyToAllLoggers(std::function<void(std::shared_ptr<spdlog::logger>)>&& func);

[[nodiscard]] Result<LoggerSinkRegistration, ExecError> addLoggerSink(std::shared_ptr<spdlog::sinks::sink> sink,
                                                                      TerminalOwnership terminal);

[[nodiscard]] FuncResult removeLoggerSink(const std::shared_ptr<spdlog::sinks::sink>& sink);

[[nodiscard]] FuncResult setAllLoggersLevel(spdlog::level::level_enum level);

[[nodiscard]] spdlog::level::level_enum getAllLoggersLevel();

void setCrashBannerDescriptor(int descriptor) noexcept;

void prepareCurrentThreadForCrashReports() noexcept;

}  // namespace impl

/// Runtime monitoring information about a single component runner.
struct ComponentMonitoringInfo
{
  std::string name;
  uint32_t group = 0;
  bool requiresRealTime = false;
  std::optional<Duration> cycleTime;
  std::size_t objectCount = 0;
  /// Thread CPU time, user and system, consumed by the last completed execution cycle.
  /// On Windows every CPU time here is quantised to the system clock tick, 15.6 ms by default, so
  /// for a shorter period than that these figures say very little.
  std::optional<Duration> lastCycleExecutionCpuTime;

  /// The part of that spent in the component's own code: its objects' update() and its work
  /// function. Its preDrain() and preCommit(), its commit-time event handlers and its discovery
  /// callbacks run outside this and are counted with Sen's.
  std::optional<Duration> lastCycleComponentCpuTime;

  /// The part of that spent on work queued on the component: the callbacks it registered, and
  /// serving the calls other components make on its objects. Serving a call is partly the
  /// component's method and partly Sen's transport, so it is reported on its own.
  std::optional<Duration> lastCycleQueuedWorkCpuTime;

  /// The most CPU time any one cycle has used since the component started running, leaving out
  /// its first cycle, which carries the startup. A component that runs long once in a thousand
  /// cycles rarely has it in the last one.
  std::optional<Duration> worstCycleExecutionCpuTime;

  /// The component's share of that same worst cycle, so the two can be compared.
  std::optional<Duration> worstCycleComponentCpuTime;

  /// How late the thread woke for the cycle it was waiting for. The scheduler's contribution,
  /// kept apart from the component's. Negative if it woke early. Empty for the first cycle, and
  /// unless the component runs on the kernel's real-time loop.
  std::optional<Duration> lastCycleStartDelay;

  /// The latest the thread has ever woken. A machine that is late once in a thousand cycles
  /// rarely has it in lastCycleStartDelay.
  std::optional<Duration> worstStartDelay;

  /// Cycles whose execution used more CPU time than the period. A cycle that blocked rather than
  /// computed does not appear here, it appears in missedFrameCount. A cycle that overruns also
  /// finishes past its slot, so missedFrameCount rises with it.
  /// Empty unless the component runs on the kernel's real-time loop.
  std::optional<uint64_t> overrunCount;

  /// Cycles lost because the work finished after the cycle it belonged to. Every lost cycle is
  /// counted, not the run of them. Wall time, so blocking counts, and so does a clock correction
  /// that moves the schedule forward. The first cycle is not counted: the schedule starts before
  /// the component has finished its own startup.
  /// Empty unless the component runs on the kernel's real-time loop.
  std::optional<uint64_t> missedFrameCount;

  /// Cycles lost because the sleep returned late, so the component was not running when it should
  /// have been. Every lost cycle is counted, not the run of them. A wake-up late by less than a
  /// period loses no cycle here and shows up in missedFrameCount, so read lastCycleStartDelay
  /// before blaming the component. Empty unless it runs on the kernel's real-time loop.
  std::optional<uint64_t> oversleptCount;
};

/// Kernel runtime monitoring information.
struct KernelMonitoringInfo
{
  RunMode runMode = RunMode::realTime;
  TransportStats transportStats {};
  std::vector<ComponentMonitoringInfo> components;
};

/// User-facing kernel functions. \ingroup kernel
class KernelApi
{
public:
  KernelApi(Kernel& kernel, impl::Runner* runner) noexcept;

public:
  /// The types registered into the kernel.
  [[nodiscard]] CustomTypeRegistry& getTypes() noexcept;

  /// Issues an asynchronous request to stop the kernel.
  /// The request is ignored if a previous stop request was issued.
  void requestKernelStop(int exitCode = 0);

  /// Gets an object source, where objects can be found and published.
  [[nodiscard]] std::shared_ptr<ObjectSource> getSource(const BusAddress& address);

  /// Gets an object source, where objects can be found and published.
  /// The address parameter must be given as <session-name>.<bus-name>.
  [[nodiscard]] std::shared_ptr<ObjectSource> getSource(const std::string& address);

  /// Object that allows discovering sessions and buses
  [[nodiscard]] SessionsDiscoverer& getSessionsDiscoverer() noexcept;

  /// Gets information about the process where an object is.
  /// Returns nullptr if the object resides in the current process.
  [[nodiscard]] const ProcessInfo* fetchOwnerInfo(const Object* object) const noexcept;

  /// Gets the (optional) application name passed to the kernel as a configuration parameter
  [[nodiscard]] const std::string& getAppName() const noexcept;

  /// Gets configured non-local bus addresses
  [[nodiscard]] std::vector<BusAddress> getConfiguredBusAddresses() const;

  /// The work queue of this runner
  [[nodiscard]] ::sen::impl::WorkQueue* getWorkQueue() const noexcept;

  /// Subscribe to every object of type T on `bus`. The returned Subscription owns the
  /// kernel-side wiring; destruct it to stop.
  ///
  /// Callback lifetime (this overload, the next, and selectFrom): onAdded / onRemoved
  /// fire on the kernel's run() thread between subscribe and the Subscription's
  /// destruction. References they capture must outlive the Subscription. Capture state
  /// by shared_ptr or via a component member.
  template <typename T, typename Bus>
  [[nodiscard]] std::shared_ptr<Subscription<T>> selectAllFrom(const Bus& bus);

  /// As above, plus addition/removal callbacks installed before subscribing so they fire
  /// for objects already present. Pass nullptr to skip either.
  template <typename T, typename Bus>
  [[nodiscard]] std::shared_ptr<Subscription<T>> selectAllFrom(
    const Bus& bus,
    typename sen::ObjectList<T>::Callback onAdded,
    typename sen::ObjectList<T>::Callback onRemoved = nullptr);

  /// Subscription against an arbitrary Sen query (with WHERE conditions).
  /// Example: `selectFrom<Shape>(bus, R"(SELECT Shape FROM local.bus WHERE color IN ("red"))")`.
  /// Installs the callbacks before subscribing. Pass nullptr to skip either.
  template <typename T, typename Bus>
  [[nodiscard]] std::shared_ptr<Subscription<T>> selectFrom(const Bus& bus,
                                                            const std::string& query,
                                                            typename sen::ObjectList<T>::Callback onAdded = nullptr,
                                                            typename sen::ObjectList<T>::Callback onRemoved = nullptr);

  /// Gets the path to the configuration file used to construct the kernel.
  /// It might be empty if the kernel is programmatically configured.
  [[nodiscard]] std::filesystem::path getConfigFilePath() const noexcept { return kernel_.getConfigPath(); }

  /// Registers a new logger in the kernel if it does not exist, or returns the existing one by name. Used to propagate
  /// the logger configuration to other packages/components that use it
  [[nodiscard]] static std::shared_ptr<spdlog::logger> getOrCreateLogger(const std::string& loggerName);

  /// Applies the input function to all loggers kept in the logger registry. Used by the logmaster
  /// component.
  ///
  /// `func` runs under spdlog's logger-map mutex, which is not recursive, so it must not do anything
  /// that reaches the registry again. `getOrCreateLogger`, `spdlog::get`, `setAllLoggersLevel` and
  /// logging through a logger looked up by name all take that mutex, and calling one from inside `func`
  /// deadlocks the calling thread. Emitting through a logger `func` was handed is fine.
  static void applyToAllLoggers(std::function<void(std::shared_ptr<spdlog::logger>)>&& func);

  /// The logger state below is per process, not per kernel. The relay sink and the record of what the
  /// console sinks were are statics in libkernel, so they outlive any `Kernel` and are shared by every
  /// kernel in the process. "Every logger" means every logger in libkernel's registry, which no kernel
  /// owns. `configureSpdlog` replaces the sink vector of every logger that already exists, so
  /// `KernelImpl::configure` re-attaches the relay straight afterwards, and a second kernel configured
  /// in the same process keeps working. A logger made by any route other than `getOrCreateLogger`, a
  /// direct spdlog factory for instance, is not reached at all.
  ///
  /// spdlog appears in these signatures deliberately. It makes a component's ABI depend on being built
  /// against the same spdlog as the kernel, which is acceptable because Sen builds the kernel and its
  /// components from one source tree, and a component that renders logs wants spdlog's own formatting.
  /// Revisit it first if components ever ship separately from the kernel.
  using TerminalOwnership = ::sen::kernel::TerminalOwnership;

  /// Sends every logger's output to `sink`, including loggers made afterwards.
  ///
  /// A component cannot do this by walking the registry itself: spdlog iterates a logger's sink vector
  /// without a lock and hands out a bare reference, so appending to a logger another thread is emitting
  /// through is a use-after-free. Appending earlier is no safer, because the kernel starts each group's
  /// threads before it loads the next group and logs between groups itself. The kernel therefore attaches
  /// one sink of its own to every logger, in the only window where attaching is safe, and this call
  /// registers `sink` behind it under a mutex. No logger's sink vector is touched here, so it may be
  /// called from any thread at any time.
  ///
  /// `sink` keeps its own pattern: the relay hands on the unformatted message and each registered sink
  /// formats it.
  ///
  /// The registration has no owner. Sinks are held until removed, so a component that is unloaded must
  /// call `removeLoggerSink`, and it must stop its sink reaching its own state first, because the sink can
  /// be running on another thread. Nothing here enforces that order: this is a static function with no
  /// api object, so the kernel cannot know which component registered what.
  ///
  /// A sink left registered is a state hazard rather than a code one only because nothing dlcloses a
  /// component, so its code stays mapped. If unload is ever made to really unload, every un-removed sink
  /// becomes a jump into unmapped memory on the next log line.
  ///
  /// Returns what the registration did, or an error if `sink` is null. Nothing else can fail.
  [[nodiscard]] static Result<LoggerSinkRegistration, ExecError> addLoggerSink(
    std::shared_ptr<spdlog::sinks::sink> sink,
    TerminalOwnership terminal = TerminalOwnership::shared);

  /// Stops sending output to `sink`. Restores the console sinks if this was the last registered sink
  /// claiming the terminal. For a component being unloaded.
  ///
  /// Removing a sink that was never registered is not an error. A null sink is.
  [[nodiscard]] static FuncResult removeLoggerSink(const std::shared_ptr<spdlog::sinks::sink>& sink);

  /// Sets the level on every logger the kernel knows and on every logger made afterwards.
  ///
  /// Walking the registry with `applyToAllLoggers` reaches only the loggers that exist when it runs, so a
  /// level set that way stops applying as soon as another component makes a logger. This sets the
  /// registry's own level, which is what a new logger is initialised from.
  ///
  /// It replaces the per-logger levels a configuration file asked for, and there is no way back to them:
  /// read one before you change it if you mean to restore it.
  ///
  /// Errors on a level outside the enum. Nothing else can fail.
  [[nodiscard]] static FuncResult setAllLoggersLevel(spdlog::level::level_enum level);

  /// Where the kernel writes the crash banner, the few lines naming what died and where the report went,
  /// for a component that has taken stderr over.
  ///
  /// A component that draws a full-screen terminal captures stderr so a stray write cannot land on its
  /// display. That captures the crash banner too, and a fatal error would print the report's path into a
  /// pipe that dies with the process, leaving the user with a vanished UI and an exit status. Hand over
  /// the descriptor the component saved and the banner goes there. Pass -1 to restore stderr, which a
  /// component must do before the descriptor it gave is closed.
  static void setCrashBannerDescriptor(int descriptor) noexcept;

  /// Give the calling thread the alternate signal stack the crash handler needs.
  ///
  /// Every thread the kernel creates gets this when it starts. A component that creates its own thread
  /// with `std::thread` does not, and a fatal signal on such a thread, a stack overflow in particular,
  /// can fault again inside the handler and produce no dump at all. Call it once, first thing, in any
  /// thread the component starts itself. Harmless if crash reporting is disabled or already armed.
  static void prepareCurrentThreadForCrashReports() noexcept;

  /// The level `setAllLoggersLevel` last set, which is also the level a new logger starts from.
  ///
  /// Read this rather than keeping a copy beside the setter, which drifts as soon as anything else sets
  /// the level.
  [[nodiscard]] static spdlog::level::level_enum getAllLoggersLevel();

private:
  template <typename T>
  [[nodiscard]] std::string buildQuery(const BusAddress& address) const;

  template <typename T>
  [[nodiscard]] std::string buildQuery(std::string_view bus) const;

private:
  Kernel& kernel_;
  impl::Runner* runner_;
};

/// Allows for fetching configuration parameters. \ingroup kernel
class ConfigGetter
{
public:
  explicit ConfigGetter(const VarMap& config) noexcept;

public:
  /// Gets the configuration associated with this component.
  [[nodiscard]] const VarMap& getConfig() const noexcept;

private:
  const VarMap& config_;
};

/// API for objects when registered. \ingroup kernel
class RegistrationApi: public ConfigGetter, public KernelApi
{
public:
  SEN_NOCOPY_NOMOVE(RegistrationApi)

public:
  RegistrationApi(Kernel& kernel, impl::Runner* runner, const VarMap& config) noexcept;
  ~RegistrationApi() noexcept = default;
};

/// What can be done when preloading a component
class PreloadApi: public ConfigGetter, public KernelApi
{
  SEN_NOCOPY_NOMOVE(PreloadApi)

public:
  PreloadApi(Kernel& kernel, impl::Runner* runner, const VarMap& config) noexcept;

  ~PreloadApi() noexcept = default;

public:
  /// Installs a transport factory for the kernel to use for sessions.
  void installTransportFactory(TransportFactory&& factory, uint32_t transportVersion) const;

  /// Installs a tracer factory.
  void installTracerFactory(TracerFactory&& factory) const;

  /// Installs a footprint reporter.
  /// The kernel stores a single reporter, installing another replaces the existing one.
  void installFootprintReporter(sen::std_util::move_only_function<NetworkFootprintReporter>&& reporter);

  /// Installs the offline reporter and runtime snapshot reporter.
  void installFootprintReporter(sen::std_util::move_only_function<NetworkFootprintReporter>&& offlineReporter,
                                sen::std_util::move_only_function<NetworkFootprint() const>&& runtimeReporter);

private:
  Kernel& kernel_;
};

/// What can be done when loading a component
class LoadApi: public ConfigGetter, public KernelApi
{
  SEN_NOCOPY_NOMOVE(LoadApi)

public:
  LoadApi(Kernel& kernel, impl::Runner* runner, const VarMap& config) noexcept;

  ~LoadApi() noexcept = default;
};

/// What can be done when initializing a component. \ingroup kernel
class InitApi: public ConfigGetter, public KernelApi
{
  SEN_NOCOPY_NOMOVE(InitApi)

public:
  InitApi(Kernel& kernel, impl::Runner* runner, const VarMap& config) noexcept;

  ~InitApi() noexcept = default;
};

/// What can be done while a component is running. \ingroup kernel
class RunApi: public ConfigGetter, public KernelApi
{
public:
  SEN_NOCOPY_NOMOVE(RunApi)

public:
  RunApi(Kernel& kernel,
         impl::KernelImpl& kernelImpl,
         impl::Runner* runner,
         std::atomic_bool& stopRequested,
         const VarMap& config,
         Guarded<TimeStamp>& timePoint) noexcept;

  ~RunApi() noexcept = default;

public:
  /// True if stop has been requested by the runtime.
  [[nodiscard]] const std::atomic_bool& stopRequested() const noexcept;

  /// Perform any request coming from the outside and drainInputs all the local
  /// data structures with their most up-to-date value.
  /// This method is thread-safe.
  void drainInputs();

  /// This calls update() on all the objects registered by the component.
  void update();

  /// Send changes, so that they become visible to other participants.
  /// This includes object additions and removals, property changes and
  /// emitted events that others might have interest in.
  /// This method is thread-safe.
  void commit();

  /// The same, tagging what is sent with `time` instead of the time Sen would stamp, for a
  /// component that runs its own cycle and bridges a system keeping its own clock. The time
  /// applies to every object this component owns. getTime() still reports Sen's time, because the
  /// rest of the kernel keeps running on its own clock.
  /// This method is thread-safe.
  void commit(TimeStamp time);

  /// A basic execution loop.
  /// Func is an optional callback that will be invoked on each cycle.
  /// logOverruns keeps the log lines for a missed deadline: an execution time overrun and
  /// the two ways a cycle is lost. The Tracy messages are emitted either way.
  [[nodiscard]] FuncResult execLoop(Duration cycleTime,
                                    std::function<void()>&& func = nullptr,
                                    bool logOverruns = true);

  /// The initial simulation time for the objects in the component
  [[nodiscard]] TimeStamp getStartTime() const noexcept;

  /// The (potentially virtualized) time.
  [[nodiscard]] TimeStamp getTime() const noexcept;

  /// If present, it returns the configured cycle time for iterations.
  [[nodiscard]] std::optional<Duration> getTargetCycleTime() const noexcept;

  /// Monitoring information of the calling component
  [[nodiscard]] ComponentMonitoringInfo fetchComponentMonitoringInfo() const;

  /// Monitoring information of all components loaded by the kernel
  [[nodiscard]] KernelMonitoringInfo fetchMonitoringInfo() const;

  /// Build information for all imported packages (from pipeline components).
  /// The returned span references kernel-owned storage that is stable for the
  /// lifetime of the kernel.
  [[nodiscard]] Span<const ComponentInfo> getImportedPackages() const noexcept;

  /// Build information for every component loaded into the kernel, excluding
  /// pipeline components (which are built from imports and have no individual
  /// build identity) and the internal kernel component. The returned span
  /// references kernel-owned storage that is stable for the lifetime of the
  /// kernel.
  [[nodiscard]] Span<const ComponentInfo> getLoadedComponents() const noexcept;

  /// Version of the currently installed transport protocol. Empty when no
  /// transport is installed. Static for the lifetime of the kernel.
  [[nodiscard]] std::optional<uint32_t> getTransportProtocolVersion() const noexcept;

  /// Create a scoped zone used for tracing runtime performance.
  [[nodiscard]] Tracer& getTracer() const noexcept;

private:
  friend void impl::remoteProcessDetected(RunApi& api, const ProcessInfo& processInfo);

  friend void impl::remoteProcessLost(RunApi& api, const ProcessInfo& processInfo);

private:
  impl::KernelImpl& kernelImpl_;
  impl::Runner* runner_;
  std::atomic_bool& stopRequested_;
  Guarded<TimeStamp>& timePoint_;
};

/// What can be done when unloading a component. \ingroup kernel
class UnloadApi: public ConfigGetter, public KernelApi
{
  SEN_NOCOPY_NOMOVE(UnloadApi)

public:
  UnloadApi(Kernel& kernel, const VarMap& config, impl::Runner* runner) noexcept;
  ~UnloadApi() noexcept = default;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <typename T, typename Bus>
inline std::shared_ptr<Subscription<T>> KernelApi::selectAllFrom(const Bus& bus)
{
  auto sub = std::make_shared<Subscription<T>>();
  sub->attachTo(getSource(bus), Interest::make(buildQuery<T>(bus), getTypes()), true);
  return sub;
}

template <typename T, typename Bus>
inline std::shared_ptr<Subscription<T>> KernelApi::selectAllFrom(const Bus& bus,
                                                                 typename ObjectList<T>::Callback onAdded,
                                                                 typename ObjectList<T>::Callback onRemoved)
{
  auto sub = std::make_shared<Subscription<T>>();
  // Install callbacks before subscribing so they fire for objects already present.
  if (onAdded)
  {
    std::ignore = sub->list.onAdded(std::move(onAdded));
  }
  if (onRemoved)
  {
    std::ignore = sub->list.onRemoved(std::move(onRemoved));
  }
  sub->attachTo(getSource(bus), Interest::make(buildQuery<T>(bus), getTypes()), true);
  return sub;
}

template <typename T, typename Bus>
inline std::shared_ptr<Subscription<T>> KernelApi::selectFrom(const Bus& bus,
                                                              const std::string& query,
                                                              typename sen::ObjectList<T>::Callback onAdded,
                                                              typename sen::ObjectList<T>::Callback onRemoved)
{
  auto sub = std::make_shared<Subscription<T>>();

  // Install callbacks before subscribing so they fire for objects already present.
  if (onAdded)
  {
    std::ignore = sub->list.onAdded(std::move(onAdded));
  }
  if (onRemoved)
  {
    std::ignore = sub->list.onRemoved(std::move(onRemoved));
  }
  sub->attachTo(getSource(bus), Interest::make(query, getTypes()), true);
  return sub;
}

namespace impl
{

/// The type name a query selects: the qualified class name, or "*" for the base Object.
/// The assert is on meta(), not inheritance: a generated interface is not an Object but
/// does carry a ClassType, where an enum or a sequence would give a null one.
template <typename T>
[[nodiscard]] inline std::string_view queryTypeName()
{
  if constexpr (std::is_same_v<T, Object>)
  {
    return "*";
  }
  else
  {
    static_assert(std::is_same_v<decltype(T::meta()), ::sen::ConstTypeHandle<::sen::ClassType>>,
                  "a subscription type must be a Sen object class");
    return T::meta()->getQualifiedName();
  }
}

}  // namespace impl

template <typename T>
inline std::string KernelApi::buildQuery(const BusAddress& address) const
{
  std::string query = "SELECT ";
  query.append(impl::queryTypeName<T>());
  query.append(" FROM ");
  query.append(address.sessionName);
  query.append(".");
  query.append(address.busName);

  return query;
}

template <typename T>
inline std::string KernelApi::buildQuery(std::string_view bus) const
{
  std::string query = "SELECT ";
  query.append(impl::queryTypeName<T>());
  query.append(" FROM ");
  query.append(bus);

  return query;
}

}  // namespace sen::kernel

#endif  // SEN_KERNEL_COMPONENT_API_H
