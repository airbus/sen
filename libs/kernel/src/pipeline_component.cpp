// === pipeline_component.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "pipeline_component.h"

// implementation
#include "kernel_impl.h"
#include "operating_system.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/kernel_config.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// spdlog
#include <spdlog/spdlog.h>  // NOLINT(misc-include-cleaner)

// std
#include <functional>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sen::kernel
{

namespace
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

[[nodiscard]] inline std::string getConnectionString(const BusAddress& address)
{
  std::string result = "sen://";
  result.append(address.sessionName);
  result.append(".");
  result.append(address.busName);
  return result;
}

[[nodiscard]] inline bool isValid(const BusAddress& address) noexcept
{
  return !address.sessionName.empty() || !address.busName.empty();
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// PipelineComponent
//--------------------------------------------------------------------------------------------------------------

PipelineComponent::PipelineComponent(KernelConfig::PipelineToLoad config, impl::KernelImpl& kernelImpl)
  : config_(std::move(config)), kernelImpl_(kernelImpl), os_(makeNativeOS())
{
}

PipelineComponent::~PipelineComponent() = default;

//--------------------------------------------------------------------------------------------------------------
// Component entry points
//--------------------------------------------------------------------------------------------------------------

FuncResult PipelineComponent::preload(PreloadApi&& api)
{
  auto& types = api.getTypes();

  openLibs(types);
  fetchTypes(types);
  validateConfig(types);

  // notify the kernel with the package build information we collected
  kernelImpl_.registerImportedPackages(importedPackages_);

  return done();
}

PassResult PipelineComponent::init(InitApi&& api)
{
  createObjects(api.getTypes());
  establishConnections(std::move(api));
  return done();
}

FuncResult PipelineComponent::run(RunApi& api)
{
  publishObjects();
  api.commit();

  if (auto runResult = api.execLoop(config_.period); runResult.isError())
  {
    return runResult;
  }

  removeObjects();
  return done();
}

FuncResult PipelineComponent::unload(UnloadApi&& api)
{
  closeConnections(std::move(api));
  deleteObjects();
  return done();
}

bool PipelineComponent::isRealTimeOnly() const noexcept { return false; }

Span<const ComponentInfo> PipelineComponent::getImportedPackages() const noexcept { return importedPackages_; }

//--------------------------------------------------------------------------------------------------------------
// PipelineComponent implementation
//--------------------------------------------------------------------------------------------------------------

void PipelineComponent::openLibs(CustomTypeRegistry& reg)
{
  // open all the libraries
  for (const auto& importedLibrary: config_.imports)
  {
    auto lib = os_->openSharedLibrary(importedLibrary);
    if (lib.nativeHandle == nullptr)
    {
      std::string err;
      err.append("could not open library ");
      err.append(importedLibrary);
      throw std::runtime_error(err);
    }

    libs_.push_back(lib);

    // try to get all the exported types
    auto* allTypesFunc = os_->getSymbol(lib, "getAllTypes");
    if (allTypesFunc != nullptr)
    {
      ExportedTypesList allTypes;

      // get the function that provides all the types
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      (*reinterpret_cast<AllTypesGetterFunc>(allTypesFunc))(allTypes);  // NOSONAR

      for (auto [type, maker]: allTypes)
      {
        reg.add(type);

        // get the maker
        if (maker != nullptr && type->isClassType())
        {
          reg.addInstanceMaker(*type->asClassType(), maker);
        }

        spdlog::debug("imported type {} from library {}",
                      type->isCustomType() ? type->asCustomType()->getQualifiedName() : type->getName(),
                      importedLibrary);
      }
    }

    // collect package build information
    if (auto* infoFunc = os_->getSymbol(lib, "getPackageInfo"); infoFunc != nullptr)
    {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      if (const auto* info = (*reinterpret_cast<PackageInfoGetterFunc>(infoFunc))(); info != nullptr)
      {
        importedPackages_.push_back(*info);
      }
    }
  }
}

void PipelineComponent::fetchTypes(CustomTypeRegistry& reg)
{
  // compile the list of types to find
  std::set<std::string, std::less<>> typesToFind;
  for (const auto& obj: config_.objects)
  {
    typesToFind.insert(obj.className);
  }

  // find and save the types
  for (const auto& typeName: typesToFind)
  {
    lookupType(typeName, reg);
  }
}

void PipelineComponent::lookupType(const std::string& name, CustomTypeRegistry& reg) const
{
  const auto allTypes = reg.getAll();

  // look up in the registry first
  if (auto regItr = allTypes.find(name); regItr != allTypes.end())
  {
    return;
  }

  auto getter = ClassType::computeTypeGetterFuncName(name);

  // search in the imported libraries
  for (const auto& lib: libs_)
  {
    auto* typeGetter = os_->getSymbol(lib, getter);
    if (typeGetter != nullptr)
    {
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
      const auto* type = (*reinterpret_cast<TypeGetterFunc>(typeGetter))();  // NOSONAR

      // Type lifetime is guaranteed through the generated type function and senExport##classname.
      reg.add(sen::makeNonOwningTypeHandle(type));

      auto makerSymbol = ClassType::computeInstanceMakerFuncName(name);

      // get the maker
      auto* maker = os_->getSymbol(lib, makerSymbol);
      SEN_ENSURE(maker != nullptr);

      SEN_ENSURE(type->isClassType() && "Loaded type should have been a class type.");
      const auto* classType = type->asClassType();

      reg.addInstanceMaker(
        *classType,
        reinterpret_cast<InstanceMakerFunc>(maker));  // NOSONAR NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)

      spdlog::debug("manually imported type {}", type->asCustomType()->getQualifiedName());
      return;
    }
  }

  // not found
  std::string err;
  err.append("could not find type '");
  err.append(name);
  err.append("' in any of the imported libraries (symbol ");
  err.append(getter);
  err.append(" not found)");
  throw std::runtime_error(err);
}

void PipelineComponent::validateConfig(const CustomTypeRegistry& reg) const
{
  // check that we have all the arguments we need to create
  // all the objects we need to create.

  for (const auto& obj: config_.objects)
  {
    auto basicType = reg.get(obj.className);
    if (!basicType)
    {
      std::string err;
      err.append("cannot create object '");
      err.append(obj.name);
      err.append("' because could not find the type '");
      err.append(obj.className);
      err.append("'");
      throw std::runtime_error(err);
    }

    // get the class type
    if (!basicType.value()->isClassType())
    {
      std::string err;
      err.append("cannot create object '");
      err.append(obj.name);
      err.append("' because the type '");
      err.append(obj.className);
      err.append("' is not a class");
      throw std::runtime_error(err);
    }

    const auto* classType = basicType.value()->asClassType();
    // get the constructor arguments and check that we have them
    for (const auto& arg: classType->getConstructor()->getArgs())
    {
      if (obj.params.find(arg.name) == obj.params.end())
      {
        std::string err;
        err.append("missing constructor argument'");
        err.append(arg.name);
        err.append("' of type '");
        err.append(arg.type->getName());
        err.append("' for creating object '");
        err.append(obj.name);
        err.append("' of class '");
        err.append(obj.className);
        err.append("'");
        throw std::runtime_error(err);
      }
    }
  }
}

void PipelineComponent::createObjects(const CustomTypeRegistry& reg)
{
  for (const auto& objConfig: config_.objects)
  {
    const auto type = reg.get(objConfig.className);
    SEN_ASSERT(type.has_value() && "Type was not registered.");

    if (type.value()->isClassType())
    {
      objects_.push_back(reg.makeInstance(type.value()->asClassType(), objConfig.name, objConfig.params));
      objectConfigs_.insert({objects_.back().get(), &objConfig});
    }
    else
    {
      std::string err;
      err.append("not a class type: '");
      err.append(objConfig.className);
      err.append("'");
      throw std::runtime_error(err);
    }
  }
}

void PipelineComponent::establishConnections(InitApi&& api)
{
  for (const auto& obj: config_.objects)
  {
    if (isValid(obj.bus))
    {
      const auto connStr = getConnectionString(obj.bus);
      if (connections_.find(connStr) == connections_.end())
      {
        connections_.try_emplace(connStr, api.getSource(obj.bus));
      }
    }
  }
}

void PipelineComponent::closeConnections(UnloadApi&& api)
{
  std::ignore = api;
  connections_.clear();
}

std::unordered_map<std::string, std::vector<std::shared_ptr<NativeObject>>> PipelineComponent::compileObjectsByBus()
{
  std::unordered_map<std::string, std::vector<std::shared_ptr<NativeObject>>> objsByBus;

  for (const auto& obj: objects_)
  {
    auto* config = objectConfigs_[obj.get()];
    if (isValid(config->bus))
    {
      objsByBus[getConnectionString(config->bus)].push_back(obj);
    }
  }

  return objsByBus;
}

void PipelineComponent::publishObjects()
{
  auto objsByBus = compileObjectsByBus();

  for (const auto& [busName, objectList]: objsByBus)
  {
    fetchConnection(busName).add(objectList);
  }
}

void PipelineComponent::removeObjects()
{
  auto objsByBus = compileObjectsByBus();

  for (const auto& [busName, objectList]: objsByBus)
  {
    fetchConnection(busName).remove(objectList);
  }
}

void PipelineComponent::deleteObjects() { objects_.clear(); }

ObjectSource& PipelineComponent::fetchConnection(const std::string& connectionString)
{
  auto itr = connections_.find(connectionString);
  if (itr == connections_.end())
  {
    std::string err;
    err.append("could not find connection for bus address '");
    err.append(connectionString);
    err.append("'");
    throw std::logic_error(err);
  }

  return *((*itr).second);
}

}  // namespace sen::kernel
