// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "component_api.h"
#include "interpreter_obj.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/result.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/package_manager.h"

// generated
#include "stl/configuration.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// pybind
#include <pybind11/embed.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

// std
#include <exception>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{

// -------------------------------------------------------------------------------------------------------------
// Constants
// -------------------------------------------------------------------------------------------------------------

constexpr float32_t defaultCycleFreqHz = 1.0f;

}  // namespace

// -------------------------------------------------------------------------------------------------------------
// Python Module
// -------------------------------------------------------------------------------------------------------------

PYBIND11_EMBEDDED_MODULE(sen, m) { sen::components::py::RunApi::definePythonApi(m); }

// -------------------------------------------------------------------------------------------------------------
// Component
// -------------------------------------------------------------------------------------------------------------

namespace sen::components::py
{

struct PyComponent: public kernel::Component
{
  [[nodiscard]] kernel::FuncResult load(kernel::LoadApi&& api) override
  {
    // instantiate the package manager
    packageManager_ = std::make_unique<kernel::PackageManager>(api.getTypes());

    // read the config and handle default values
    config_ = toValue<Configuration>(api.getConfig());
    if (config_.freqHz == 0.0f)
    {
      config_.freqHz = defaultCycleFreqHz;
    }

    if (!config_.imports.empty())
    {
      packageManager_->import(config_.imports.asVector());
    }

    return done();
  }

  [[nodiscard]] kernel::FuncResult run(kernel::RunApi& api) override
  {
    pybind11::scoped_interpreter guard {};

    // load our module to register our functions
    senModule_.pyModule = pybind11::module_::import("sen");

    // make the package types available to Python
    addImportedTypes();
    senModule_.pyModule.reload();

    if (!config_.module.empty())
    {
      try
      {
        userModule_ = pybind11::module::import(config_.module.c_str());
      }
      catch (const pybind11::error_already_set& err)
      {
        return Err(kernel::ExecError {kernel::ErrorCategory::expectationsNotMet, err.what()});
      }

      RunApi apiWrapper(&api, config_.freqHz, packageManager_.get());
      senModule_.pyModule.attr("api") = apiWrapper;

      auto runFunction = getFunctionIfExists("run");
      if (runFunction)
      {
        try
        {
          runFunction();
        }
        catch (const pybind11::error_already_set& err)
        {
          return Err(kernel::ExecError {kernel::ErrorCategory::runtimeError, err.what()});
        }
      }
    }

    std::shared_ptr<InterpreterObj> interpreterObj;
    std::shared_ptr<ObjectSource> bus;
    if (!config_.bus.empty())
    {
      bus = api.getSource(config_.bus);
      interpreterObj = std::make_shared<InterpreterObj>("interpreter");
      bus->add(interpreterObj);
    }

    kernel::FuncResult result = done();

    if (!config_.module.empty())
    {

      if (auto updateFunction = getFunctionIfExists("update"); updateFunction)
      {
        result = api.execLoop(Duration::fromHertz(config_.freqHz),
                              [&updateFunction]()
                              {
                                try
                                {
                                  updateFunction();
                                }
                                catch (pybind11::error_already_set& err)
                                {
                                  getLogger()->error(err.what());
                                  err.discard_as_unraisable("PyComponent::run");
                                }
                                catch (const std::exception& err)
                                {
                                  getLogger()->error("unknown error detected: {}", err.what());
                                }
                              });
      }
      else
      {
        result = api.execLoop(Duration::fromHertz(config_.freqHz));
      }
    }
    else if (!config_.bus.empty())
    {
      result = api.execLoop(Duration::fromHertz(config_.freqHz));
    }
    else
    {
      // no code needed
    }

    if (!config_.module.empty())
    {
      auto stopFunction = getFunctionIfExists("stop");
      if (stopFunction)
      {
        stopFunction();
      }
    }

    // clear the created submodules
    userModule_ = {};
    senModule_.subModules = {};
    definedEnums_.clear();
    senModule_ = {};

    return result;
  }

  [[nodiscard]] bool isRealTimeOnly() const noexcept override { return true; }

  void addImportedTypes()
  {
    const auto& allTypes = packageManager_->getImportedTypes().getAll();
    for (const auto& [typeName, regType]: allTypes)
    {
      if (const auto* type = regType->asEnumType())
      {
        if (definedEnums_.count(typeName) == 0)
        {
          auto tokens = impl::split(std::string(type->getQualifiedName()), '.');
          const auto& ns = getOrCreateNamespace(tokens, senModule_);

          for (const auto& enumerator: type->getEnums())
          {
            ns.attr(enumerator.name.c_str()) = enumerator.name;
          }
          definedEnums_.insert(typeName);
        }
      }
    }
  }

private:
  struct ModuleNode
  {
    pybind11::module_ pyModule;
    std::map<std::string, ModuleNode, std::less<>> subModules;
  };

private:
  [[nodiscard]] pybind11::module_& getOrCreateNamespace(std::vector<std::string> path, ModuleNode& currentNode)
  {
    // the front element is the current namespace
    auto currentPathElem = path.front();

    // remove the front
    path.erase(path.begin());

    // check if we have a submodule with the said name
    if (auto currentItr = currentNode.subModules.find(currentPathElem); currentItr != currentNode.subModules.end())
    {
      // if we reached the final element, return the module
      if (path.empty())
      {
        return currentItr->second.pyModule;
      }

      // otherwise, keep searching inside
      return getOrCreateNamespace(path, currentItr->second);
    }

    ModuleNode node {};
    node.pyModule = currentNode.pyModule.def_submodule(currentPathElem.c_str());

    // we don't have submodule with the said name, so we must create it
    auto [itr, inserted] = currentNode.subModules.try_emplace(currentPathElem, std::move(node));

    // it this was the last element, return the module
    if (path.empty())
    {
      return itr->second.pyModule;
    }

    // this was not the last element, so we must continue creating submodules
    return getOrCreateNamespace(path, itr->second);
  }

  [[nodiscard]] pybind11::function getFunctionIfExists(std::string_view name) const
  {
    try
    {
      return pybind11::reinterpret_borrow<pybind11::function>(userModule_.attr(name.data()));
    }
    catch (const std::exception&)
    {
      return {};
    }
  }

private:
  Configuration config_;
  std::unique_ptr<kernel::PackageManager> packageManager_;
  std::set<std::string, std::less<>> definedEnums_;
  std::map<std::string, pybind11::module_, std::less<>> namespaces_;
  ModuleNode senModule_;
  pybind11::module userModule_;
};

}  // namespace sen::components::py

SEN_COMPONENT(sen::components::py::PyComponent)
