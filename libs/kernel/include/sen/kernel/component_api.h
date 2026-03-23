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
#include "sen/core/base/mutex_utils.h"
#include "sen/core/base/result.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_source.h"
#include "sen/core/obj/subscription.h"

// kernel
#include "sen/kernel/kernel.h"
#include "sen/kernel/source_info.h"
#include "sen/kernel/tracer.h"
#include "sen/kernel/transport.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <atomic>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sen::kernel
{

/// The result of operations that are called once.
using FuncResult = Result<void, ExecError>;

/// The result of operations that may be called multiple times.
using PassResult = Result<OpState, ExecError>;

class SessionsDiscoverer;
class RunApi;
class KernelApi;

namespace impl
{

class Runner;
class KernelImpl;

[[nodiscard]] FuncResult execLoop(Runner* runner, Duration cycleTime, std::function<void()>&& workFunction);

void installTransportFactory(KernelImpl* kernel, TransportFactory&& factory, uint32_t transportVersion);

void remoteProcessDetected(RunApi& api, const ProcessInfo& processInfo);

void remoteProcessLost(RunApi& api, const ProcessInfo& processInfo);

[[nodiscard]] std::shared_ptr<ObjectSource> getSource(Runner* runner, const BusAddress& address);

[[nodiscard]] SessionsDiscoverer& getSessionsDiscoverer(Runner* runner);

[[nodiscard]] const ProcessInfo* fetchOwnerInfo(const Object* object);

[[nodiscard]] ::sen::impl::WorkQueue* getWorkQueue(Runner* runner);

}  // namespace impl

/// Runtime monitoring snapshot for a single component.
struct ComponentMonitoringInfo
{
  ComponentInfo info;                 ///< Static identity and metadata of the component.
  ComponentConfig config;             ///< Configuration block supplied at load time.
  bool requiresRealTime = false;      ///< `true` if the component was configured to run on a real-time thread.
  std::optional<Duration> cycleTime;  ///< Configured target cycle time, or `nullopt` if not periodic.
  std::size_t objectCount = 0;        ///< Number of objects currently registered by this component.
};

/// Runtime monitoring snapshot for the entire kernel.
struct KernelMonitoringInfo
{
  RunMode runMode = RunMode::realTime;              ///< Current execution mode (real-time or simulation).
  TransportStats transportStats {};                 ///< Aggregate transport-layer statistics (bytes, messages, errors).
  std::vector<ComponentMonitoringInfo> components;  ///< Per-component snapshots, in registration order.
};

/// Facade exposing kernel services to a component during all lifecycle phases.
/// Available through every lifecycle API (`RegistrationApi`, `InitApi`, `RunApi`, etc.).
/// \ingroup kernel
class KernelApi
{
public:
  KernelApi(Kernel& kernel, impl::Runner* runner) noexcept;

public:
  /// Returns the type registry containing all types loaded for this kernel instance.
  /// @return Mutable reference to the `CustomTypeRegistry`.
  [[nodiscard]] CustomTypeRegistry& getTypes() noexcept;

  /// Schedules an asynchronous shutdown of the kernel with the given exit code.
  /// Subsequent calls are ignored if a stop has already been requested.
  /// @param exitCode Process exit code to use when the kernel stops (default 0).
  void requestKernelStop(int exitCode = 0);

  /// Returns (or creates) the `ObjectSource` for the given bus address.
  /// Object sources are the entry points for publishing and subscribing to objects on a bus.
  /// @param address Structured `{sessionName, busName}` bus address.
  /// @return Shared pointer to the `ObjectSource` for that address.
  [[nodiscard]] std::shared_ptr<ObjectSource> getSource(const BusAddress& address);

  /// Returns (or creates) the `ObjectSource` for a bus specified as `"<session>.<bus>"`.
  /// @param address Dot-separated `"session.bus"` string.
  /// @return Shared pointer to the `ObjectSource` for that address.
  [[nodiscard]] std::shared_ptr<ObjectSource> getSource(const std::string& address);

  /// Returns the sessions discoverer, which tracks all known sessions and buses on the network.
  /// @return Reference to the shared `SessionsDiscoverer`; valid for the lifetime of the kernel.
  [[nodiscard]] SessionsDiscoverer& getSessionsDiscoverer() noexcept;

  /// Returns process information for the process that owns `object`.
  /// @param object The object whose owner to look up.
  /// @return Pointer to the `ProcessInfo` of the owning process, or `nullptr` if `object` is local.
  [[nodiscard]] const ProcessInfo* fetchOwnerInfo(const Object* object) const noexcept;

  /// Returns the application name supplied in the kernel configuration, if any.
  /// @return Reference to the application name string; may be empty if not configured.
  [[nodiscard]] const std::string& getAppName() const noexcept;

  /// Returns the internal work queue for this runner (advanced use only).
  /// @return Non-owning pointer to the `WorkQueue`; valid for the lifetime of this runner.
  [[nodiscard]] ::sen::impl::WorkQueue* getWorkQueue() const noexcept;

  /// Subscribes to all objects of type `T` visible on `bus` and returns a managed `Subscription`.
  /// @tparam T  Sen class type to select; use `Object` to select all types.
  /// @tparam B  Bus address type (`BusAddress` or `std::string`).
  /// @param bus The bus to subscribe to.
  /// @return Shared pointer to the new `Subscription<T>`.
  template <typename T, typename B>
  [[nodiscard]] std::shared_ptr<Subscription<T>> selectAllFrom(const B& bus);

  /// Returns the path to the YAML configuration file used to initialise the kernel.
  /// @return Filesystem path, or an empty path if the kernel was configured programmatically.
  [[nodiscard]] std::filesystem::path getConfigFilePath() const noexcept { return kernel_.getConfigPath(); }

private:
  template <typename T>
  [[nodiscard]] std::string buildQuery(const BusAddress& address) const;

  template <typename T>
  [[nodiscard]] std::string buildQuery(std::string_view bus) const;

private:
  Kernel& kernel_;
  impl::Runner* runner_;
};

/// Mixin that provides read access to the component's configuration map.
/// Inherited by all lifecycle API classes so components can read their YAML config in every phase.
/// \ingroup kernel
class ConfigGetter
{
public:
  /// @param config Reference to the component's parsed configuration map; must outlive this object.
  explicit ConfigGetter(const VarMap& config) noexcept;

public:
  /// Returns the component's configuration map as parsed from its YAML block.
  /// @return Const reference to the `VarMap`; valid for the lifetime of this object.
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

/// API available during the `preload` lifecycle phase (before any objects are created).
/// Use this phase to register custom transports or tracers before the kernel starts.
/// \ingroup kernel
class PreloadApi: public ConfigGetter, public KernelApi
{
  SEN_NOCOPY_NOMOVE(PreloadApi)

public:
  PreloadApi(Kernel& kernel, impl::Runner* runner, const VarMap& config) noexcept;

  ~PreloadApi() noexcept = default;

public:
  /// Registers a custom transport factory with the kernel.
  /// The factory is invoked whenever a session of the matching version is opened.
  /// @param factory          Callable that creates transport instances.
  /// @param transportVersion Version tag matched against the session's transport configuration.
  void installTransportFactory(TransportFactory&& factory, uint32_t transportVersion) const;

  /// Registers a custom tracer factory with the kernel.
  /// @param factory Callable that creates `Tracer` instances for performance instrumentation.
  void installTracerFactory(TracerFactory&& factory) const;

private:
  Kernel& kernel_;
};

/// API available during the `load` lifecycle phase.
/// Provides access to the type registry and kernel services for resource allocation before objects are published.
/// \ingroup kernel
class LoadApi: public ConfigGetter, public KernelApi
{
  SEN_NOCOPY_NOMOVE(LoadApi)

public:
  LoadApi(Kernel& kernel, impl::Runner* runner, const VarMap& config) noexcept;

  ~LoadApi() noexcept = default;
};

/// API available during the `init` lifecycle phase.
/// Use this to register objects on buses; `init()` is called repeatedly until `done()` is returned.
/// \ingroup kernel
class InitApi: public ConfigGetter, public KernelApi
{
  SEN_NOCOPY_NOMOVE(InitApi)

public:
  InitApi(Kernel& kernel, impl::Runner* runner, const VarMap& config) noexcept;

  ~InitApi() noexcept = default;
};

/// API available while a component is actively running (inside its `run()` method).
/// Provides the main execution primitives: input draining, object update, output commit, and timing.
/// \ingroup kernel
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
  /// Returns the atomic flag that is set when the kernel requests a shutdown.
  /// Poll this in your run loop to exit cleanly.
  /// @return Const reference to the stop-requested flag.
  [[nodiscard]] const std::atomic_bool& stopRequested() const noexcept;

  /// Processes all pending external requests and refreshes local data structures
  /// with the latest incoming property values and events.
  /// Call this at the start of each cycle before reading object state.
  /// Thread-safe.
  void drainInputs();

  /// Calls `update()` on all objects registered by this component.
  /// Typically called after `drainInputs()` to allow objects to react to new inputs.
  void update();

  /// Publishes all accumulated changes (object additions/removals, property updates, events)
  /// so they become visible to other participants on the bus.
  /// Call this at the end of each cycle after writing object state.
  /// Thread-safe.
  void commit();

  /// Runs a fixed-rate execution loop, calling `func` on each cycle.
  /// The loop exits when `stopRequested()` is set or an error occurs.
  /// @param cycleTime Target period for each iteration.
  /// @param func      Optional callable invoked once per cycle (after `drainInputs`/`update`/`commit`).
  /// @param logOverruns If `true`, warns when a cycle takes longer than `cycleTime`.
  /// @return `Ok(void)` on clean shutdown, or `Err(ExecError)` on failure.
  [[nodiscard]] FuncResult execLoop(Duration cycleTime,
                                    std::function<void()>&& func = nullptr,
                                    bool logOverruns = true);

  /// Returns the simulation timestamp at which this component started running.
  /// @return Start time as a `TimeStamp`.
  [[nodiscard]] TimeStamp getStartTime() const noexcept;

  /// Returns the current (potentially virtualised) simulation time.
  /// In real-time mode this tracks wall-clock time; in simulation mode it advances discretely.
  /// @return Current `TimeStamp`.
  [[nodiscard]] TimeStamp getTime() const noexcept;

  /// Returns the configured target cycle time for this component, if any.
  /// @return The cycle time `Duration`, or `nullopt` if the component has no fixed rate.
  [[nodiscard]] std::optional<Duration> getTargetCycleTime() const noexcept;

  /// Returns a snapshot of kernel-wide monitoring information (transport stats, component states).
  /// @return `KernelMonitoringInfo` populated at the time of the call.
  [[nodiscard]] KernelMonitoringInfo fetchMonitoringInfo() const;

  /// Returns the performance tracer for this component's runner thread.
  /// Use the returned `Tracer` to create named zones for profiling.
  /// @return Reference to the `Tracer`; valid for the lifetime of this `RunApi`.
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

/// API available during the `unload` lifecycle phase.
/// Use this to release kernel-level resources after `run()` has returned.
/// \ingroup kernel
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

template <typename T, typename B>
inline std::shared_ptr<Subscription<T>> KernelApi::selectAllFrom(const B& bus)
{
  auto sub = std::make_shared<Subscription<T>>();
  sub->source = getSource(bus);
  sub->source->addSubscriber(Interest::make(buildQuery<T>(bus), getTypes()), &sub->list, true);
  return sub;
}

template <typename T>
inline std::string KernelApi::buildQuery(const BusAddress& address) const
{
  const ClassType* meta = nullptr;
  if constexpr (!std::is_same_v<T, Object>)
  {
    meta = &T::meta();
  }

  std::string query = "SELECT ";
  query.append(meta ? meta->getQualifiedName() : "*");
  query.append(" FROM ");
  query.append(address.sessionName);
  query.append(".");
  query.append(address.busName);

  return query;
}

template <typename T>
inline std::string KernelApi::buildQuery(std::string_view bus) const
{
  const ClassType* meta = nullptr;
  if constexpr (!std::is_same_v<T, Object>)
  {
    meta = T::meta()->asClassType();
  }

  std::string query = "SELECT ";
  query.append(meta ? meta->getQualifiedName() : "*");
  query.append(" FROM ");
  query.append(bus);

  return query;
}

}  // namespace sen::kernel

#endif  // SEN_KERNEL_COMPONENT_API_H
