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
#include <utility>
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

/// Monitoring information about components.
struct ComponentMonitoringInfo
{
  ComponentInfo info;
  ComponentConfig config;
  bool requiresRealTime = false;
  std::optional<Duration> cycleTime;
  std::size_t objectCount = 0;
};

/// Kernel monitoring information.
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

  /// The work queue of this runner
  [[nodiscard]] ::sen::impl::WorkQueue* getWorkQueue() const noexcept;

  template <typename T, typename Bus>
  [[nodiscard]] std::shared_ptr<Subscription<T>> selectAllFrom(const Bus& bus);

  /// Overload of selectAllFrom that installs addition and removal callbacks before subscribing,
  /// so they fire for objects already present at subscription time.
  /// Pass nullptr for either callback to skip it.
  template <typename T, typename Bus>
  [[nodiscard]] std::shared_ptr<Subscription<T>> selectAllFrom(
    const Bus& bus,
    typename sen::ObjectList<T>::Callback onAdded,
    typename sen::ObjectList<T>::Callback onRemoved = nullptr);

  /// Creates a subscription for objects matching the given Sen query string.
  /// Unlike selectAllFrom, this lets you supply an arbitrary query with WHERE conditions.
  /// Example: selectFrom<Shape>(bus, "SELECT Shape FROM local.bus WHERE color IN (\"red\")").
  /// It installs addition and removal callbacks before subscribing. Pass nullptr for either callback to skip it.
  template <typename T, typename Bus>
  [[nodiscard]] std::shared_ptr<Subscription<T>> selectFrom(const Bus& bus,
                                                            const std::string& query,
                                                            typename sen::ObjectList<T>::Callback onAdded = nullptr,
                                                            typename sen::ObjectList<T>::Callback onRemoved = nullptr);

  /// Gets the path to the configuration file used to construct the kernel.
  /// It might be empty if the kernel is programmatically configured.
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

  /// A basic execution loop.
  /// Func is an optional callback that will be invoked on each cycle.
  [[nodiscard]] FuncResult execLoop(Duration cycleTime,
                                    std::function<void()>&& func = nullptr,
                                    bool logOverruns = true);

  /// The initial simulation time for the objects in the component
  [[nodiscard]] TimeStamp getStartTime() const noexcept;

  /// The (potentially virtualized) time.
  [[nodiscard]] TimeStamp getTime() const noexcept;

  /// If present, it returns the configured cycle time for iterations.
  [[nodiscard]] std::optional<Duration> getTargetCycleTime() const noexcept;

  /// Monitoring information.
  [[nodiscard]] KernelMonitoringInfo fetchMonitoringInfo() const;

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
  sub->source = getSource(bus);
  sub->source->addSubscriber(Interest::make(buildQuery<T>(bus), getTypes()), &sub->list, true);
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
  sub->source = getSource(bus);
  sub->source->addSubscriber(Interest::make(buildQuery<T>(bus), getTypes()), &sub->list, true);
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
  sub->source = getSource(bus);
  sub->source->addSubscriber(Interest::make(query, getTypes()), &sub->list, true);
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
