// === component_api_impl.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// implementation
#include "./kernel_impl.h"

// sen
#include "bus/remote_participant.h"  // NOLINT(misc-include-cleaner) false positive
#include "sen/core/base/assert.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/mutex_utils.h"
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

// std
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace sen::kernel
{

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

void update(Runner* runner) { runner->update(); }

FuncResult execLoop(Runner* runner, Duration cycleTime, std::function<void()>&& workFunction, bool logOverruns)
{
  return runner->execLoop(cycleTime, std::move(workFunction), logOverruns);
}

::sen::impl::WorkQueue* getWorkQueue(Runner* runner) { return runner->getWorkQueue(); }

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
  auto tokens = ::sen::impl::split(address, '.');
  if (tokens.size() != 2U)
  {
    std::string err;
    err.append("invalid bus address '");
    err.append(address);
    err.append("'");
    throwRuntimeError(err);
  }

  return getSource(BusAddress {tokens.at(0U), tokens.at(1U)});
}

SessionsDiscoverer& KernelApi::getSessionsDiscoverer() noexcept { return impl::getSessionsDiscoverer(runner_); }

const ProcessInfo* KernelApi::fetchOwnerInfo(const Object* object) const noexcept
{
  return impl::fetchOwnerInfo(object);
}

const std::string& KernelApi::getAppName() const noexcept { return kernel_.getConfig().getParams().appName; }

::sen::impl::WorkQueue* KernelApi::getWorkQueue() const noexcept { return impl::getWorkQueue(runner_); }

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

KernelMonitoringInfo RunApi::fetchMonitoringInfo() const { return kernelImpl_.fetchMonitoringInfo(); }

Tracer& RunApi::getTracer() const noexcept { return runner_->getTracer(); }

//--------------------------------------------------------------------------------------------------------------
// UnloadApi
//--------------------------------------------------------------------------------------------------------------

UnloadApi::UnloadApi(Kernel& kernel, const VarMap& config, impl::Runner* runner) noexcept
  : ConfigGetter(config), KernelApi(kernel, runner)
{
}

}  // namespace sen::kernel
