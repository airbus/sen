// === component_api.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "component_api.h"

// component
#include "object_list_wrapper.h"
#include "object_source_wrapper.h"
#include "object_wrapper.h"
#include "variant_conversion.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/hash32.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/interest.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/package_manager.h"

// pybind11
#include <pybind11/detail/common.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

// spdlog
#include <spdlog/logger.h>

// std
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>

namespace sen::components::py
{

// -------------------------------------------------------------------------------------------------------------
// Logging
// -------------------------------------------------------------------------------------------------------------

std::shared_ptr<spdlog::logger> getLogger()
{
  static auto logger = std::make_shared<spdlog::logger>("py");
  return logger;
}

// -------------------------------------------------------------------------------------------------------------
// RunApi
// -------------------------------------------------------------------------------------------------------------

RunApi::RunApi(kernel::RunApi* runApi, Duration updatePeriod, kernel::PackageManager* packageManager)
  : runApi_(runApi), updatePeriod_(updatePeriod), packageManager_(packageManager)
{
}

void RunApi::requestKernelStop(int exitCode) { runApi_->requestKernelStop(exitCode); }

std::unique_ptr<ObjectSourceWrapper> RunApi::getSource(const std::string& address)
{
  return std::make_unique<ObjectSourceWrapper>(runApi_->getSource(address), this);
}

std::unique_ptr<ObjectListWrapper> RunApi::open(const std::string& selection)
{
  // build the interest
  auto interest = Interest::make(selection, runApi_->getTypes());

  // ensure the interest defines the bus
  if (!interest->getBusCondition().has_value())
  {
    throwRuntimeError("missing bus in selection");
  }

  // get the bus
  const auto& busCondition = interest->getBusCondition().value();
  const std::string busAddr = busCondition.sessionName + "." + busCondition.busName;
  auto source = runApi_->getSource(busAddr);

  // build the name of the provider
  std::string providerName = busAddr;
  providerName.append(".");
  providerName.append(std::to_string(crc32(selection)));

  // create the provider
  auto provider = source->getOrCreateNamedProvider(providerName, interest);

  // create the list
  return std::make_unique<ObjectListWrapper>(source, provider, std::move(providerName), this);
}

PythonDuration RunApi::getTime() const noexcept { return runApi_->getTime().sinceEpoch().toChrono(); }

std::string RunApi::getAppName() const noexcept { return runApi_->getAppName(); }

pybind11::dict RunApi::getConfig() const { return toPython(runApi_->getConfig()); }

void RunApi::definePythonApi(pybind11::module_& pythonModule)
{
  // ensure the dependent classes are defined
  ObjectSourceWrapper::definePythonApi(pythonModule);
  ObjectListWrapper::definePythonApi(pythonModule);

  // our class
  pybind11::class_<RunApi> cl(pythonModule, "KernelApi");

  // properties
  cl.def_property_readonly("time", &RunApi::getTime, "The time elapsed since epoch");
  cl.def_property_readonly("config", &RunApi::getConfig);
  cl.def_property_readonly("appName", &RunApi::getAppName);
  cl.def_readwrite("syncCalls", &RunApi::syncCalls_);
  cl.def_property("defaultTimeout", &RunApi::getDefaultTimeout, &RunApi::setDefaultTimeout);

  // methods
  cl.def("requestKernelStop",
         &RunApi::requestKernelStop,
         "Issues an asynchronous request to stop the kernel.",
         pybind11::arg("exitCode") = 0);
  cl.def("open", &RunApi::open, "Open a named interest on a set of objects");
  cl.def("getBus", &RunApi::getSource, "Get a reference to a bus");

  cl.def("make",
         &RunApi::make,
         "instantiates a native object",
         pybind11::arg("className"),
         pybind11::arg("instanceName"),
         pybind11::kw_only());

  cl.def("waitUntil",
         &RunApi::waitUntil,
         "Drains inputs until the condition is met or the time expires. Returns true if the condition is met",
         pybind11::arg("condition"),
         pybind11::arg("timeout") = std::chrono::nanoseconds {});

  cl.def("__repr__", [](const RunApi& /*a*/) { return "<sen.RunApi>"; });
}

bool RunApi::getSyncCalls() const noexcept { return syncCalls_; }

::sen::impl::WorkQueue* RunApi::getWorkQueue() noexcept { return runApi_->getWorkQueue(); }

bool RunApi::waitUntil(pybind11::object condition, std::chrono::nanoseconds timeout)
{
  if (!pybind11::isinstance<pybind11::function>(condition))
  {
    std::string err;
    err.append("expecting function argument to waitUntil()");
    throw pybind11::attribute_error(err);
  }

  return waitUntilCpp([&condition]() { return condition().cast<bool>(); }, timeout);
}

std::chrono::nanoseconds RunApi::getDefaultTimeout() const noexcept { return defaultTimeout_; }

void RunApi::setDefaultTimeout(std::chrono::nanoseconds timeout) noexcept { defaultTimeout_ = timeout; }

bool RunApi::waitUntilCpp(std::function<bool()>&& condition, std::chrono::nanoseconds timeout)
{
  if (timeout.count() == 0)
  {
    timeout = defaultTimeout_;
  }

  if (timeout.count() == 0)
  {
    while (!condition())
    {
      runApi_->drainInputs();
      runApi_->update();
      std::this_thread::sleep_for(updatePeriod_.toChrono());
      runApi_->commit();
    }
    return true;
  }

  using Clock = std::chrono::high_resolution_clock;

  auto start = Clock::now();
  while (!condition())
  {
    if (Clock::now() - start > timeout)
    {
      return false;
    }

    runApi_->drainInputs();
    runApi_->update();
    std::this_thread::sleep_for(updatePeriod_.toChrono());
    runApi_->commit();
  }

  return true;
}

void RunApi::commit() { runApi_->commit(); }

ObjectWrapper RunApi::make(const std::string& className, const std::string& instanceName, pybind11::kwargs args)
{
  const auto* type = packageManager_->lookupType(className);
  if (type == nullptr)
  {
    std::string err;
    err.append("did not find any registered type named ");
    err.append(className);
    throw pybind11::attribute_error(err);
  }

  if (!type->isClassType())
  {
    std::string err;
    err.append("found a registered type named ");
    err.append(className);
    err.append(" but it is not a class");
    throw pybind11::attribute_error(err);
  }

  const auto* meta = type->asClassType();
  VarMap map;
  for (const auto& [keyObject, valueObject]: args)
  {
    const auto key = keyObject.cast<std::string>();

    const auto* prop = meta->searchPropertyByName(key);
    if (prop == nullptr)
    {
      std::string err;
      err.append("did not find any property named ");
      err.append(key);
      err.append(" in class ");
      err.append(className);
      throw pybind11::attribute_error(err);
    }

    map.try_emplace(key, fromPython(valueObject.cast<pybind11::object>(), *prop->getType()));
  }

  return ObjectWrapper::makeNative(runApi_->getTypes().makeInstance(meta, instanceName, map), this);
}

}  // namespace sen::components::py
