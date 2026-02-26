// === plugin_manager.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "plugin_manager.h"

// implementation
#include "operating_system.h"
#include "shared_library.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/kernel/component.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sen::kernel
{

class PluginContext
{
  SEN_NOCOPY_NOMOVE(PluginContext)

public:
  PluginContext(std::string_view path, std::shared_ptr<OperatingSystem> os): path_(path), os_(std::move(os))
  {
    SEN_EXPECT(!path.empty());
  }

  ~PluginContext() noexcept { unplug(); }

  void plug()
  {
    if (component_ != nullptr)
    {
      return;  // already plugged
    }

    auto pathToUse = std::filesystem::path(path_);
    lib_ = os_->openSharedLibrary(pathToUse.string());

    SEN_ENSURE(lib_.nativeHandle != nullptr);

    // call the factory function and get the non-owning pointer
    {
      using PluginCreateFunc = Component* (*)();
      void* sym = findSymbol(componentMakerFuncName);
      auto* func = reinterpret_cast<PluginCreateFunc>(sym);  // NOLINT NOSONAR

      // create the component
      component_.reset((*func)());
    }

    // get the component info
    {
      using InfoFunc = const ComponentInfo* (*)();
      void* sym = findSymbol(componentInfoMakerFuncName);
      auto* func = reinterpret_cast<InfoFunc>(sym);  // NOLINT NOSONAR
      componentInfo_ = (*(*func)());
    }
  }

  void unplug() noexcept
  {
    if (!component_)
    {
      return;  // already unplugged
    }

    // delete the component
    component_.reset(nullptr);
  }

  [[nodiscard]] Component* getComponent() noexcept { return component_.get(); }

  [[nodiscard]] const std::string& getPath() const noexcept { return path_; }

  [[nodiscard]] PluginInfo getInfo() const
  {
    PluginInfo info;
    info.path = path_;
    info.component.info = componentInfo_;
    info.component.instance = component_.get();

    return info;
  }

private:
  [[nodiscard]] void* findSymbol(std::string_view name)
  {
    void* symbol = os_->getSymbol(lib_, name);
    if (symbol == nullptr)
    {
      std::string err;
      err.append("cannot find the '");
      err.append(name);
      err.append("' symbol in the library '");
      err.append(path_);
      err.append("'");
      throwRuntimeError(err);
    }

    return symbol;
  }

private:
  std::string path_;
  std::shared_ptr<OperatingSystem> os_;
  std::unique_ptr<Component> component_;
  SharedLibrary lib_ {nullptr};
  ComponentInfo componentInfo_;
};

//------------------------------------------------------------------------------------------------//
// PluginManager
//------------------------------------------------------------------------------------------------//

PluginManager::PluginManager(std::shared_ptr<OperatingSystem> os): os_(std::move(os)) {}

PluginManager::~PluginManager() { unplugAll(); }

PluginInfo PluginManager::plug(std::string_view path)
{
  for (const auto& elem: plugins_)
  {
    if (elem->getPath() == path)
    {
      return elem->getInfo();
    }
  }

  // not present, try to create it
  auto plugin = std::make_unique<PluginContext>(path, os_);
  plugin->plug();

  plugins_.push_back(std::move(plugin));
  return plugins_.back()->getInfo();
}

void PluginManager::unplug(std::string_view path)
{
  for (auto itr = plugins_.begin(); itr != plugins_.end(); ++itr)
  {
    if (const auto& elem = (*itr); elem->getPath() == path)
    {
      elem->unplug();
      plugins_.erase(itr);
      return;
    }
  }
}

void PluginManager::unplugAll()
{
  for (const auto& elem: plugins_)
  {
    elem->unplug();
  }
  plugins_.clear();
}

std::vector<PluginInfo> PluginManager::getOpenPlugins() const
{
  std::vector<PluginInfo> result;
  result.reserve(plugins_.size());

  for (const auto& elem: plugins_)
  {
    result.push_back(elem->getInfo());
  }
  return result;
}

bool PluginManager::isPlugged(std::string_view path) const noexcept
{
  return std::any_of(plugins_.begin(), plugins_.end(), [&path](const auto& elem) { return elem->getPath() == path; });
}

}  // namespace sen::kernel
