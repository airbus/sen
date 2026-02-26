// === kernel_config.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/kernel_config.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <utility>
#include <vector>

namespace sen::kernel
{

void KernelConfig::addToLoad(ComponentPluginToLoad plugin)
{
  for (auto& element: pluginsToLoad_)
  {
    if (element.path == plugin.path)
    {
      element.config = std::move(plugin.config);
      element.params = plugin.params;
      return;
    }
  }

  pluginsToLoad_.push_back(std::move(plugin));
}

void KernelConfig::addToLoad(KernelConfig::ComponentToLoad component)
{
  for (auto& element: componentsToLoad_)
  {
    if (element.component.info.name == component.component.info.name)
    {
      element.config = std::move(component.config);
      element.params = component.params;
      element.component.info = component.component.info;
      return;
    }
  }

  componentsToLoad_.push_back(std::move(component));
}

void KernelConfig::addToLoad(KernelConfig::PipelineToLoad pipeline)
{
  for (auto& element: pipelinesToLoad_)
  {
    if (element.name == pipeline.name)
    {
      element.config = std::move(pipeline.config);
      element.imports = std::move(pipeline.imports);
      element.objects = std::move(pipeline.objects);
      return;
    }
  }

  pipelinesToLoad_.push_back(std::move(pipeline));
}

void KernelConfig::setParams(KernelParams params) noexcept
{
  params_ = std::move(params);

  if (params_.bus.empty())
  {
    params_.bus = "local.kernel";
  }

  if (params_.clockName.empty())
  {
    params_.clockName = "clock";
  }

  if (params_.clockBus.empty())
  {
    params_.clockBus = params_.bus;
  }
}

const KernelParams& KernelConfig::getParams() const noexcept { return params_; }

const std::vector<KernelConfig::ComponentToLoad>& KernelConfig::getComponentsToLoad() const noexcept
{
  return componentsToLoad_;
}

const std::vector<KernelConfig::PipelineToLoad>& KernelConfig::getPipelinesToLoad() const noexcept
{
  return pipelinesToLoad_;
}

const std::vector<KernelConfig::ComponentPluginToLoad>& KernelConfig::getPluginsToLoad() const noexcept
{
  return pluginsToLoad_;
}

}  // namespace sen::kernel
