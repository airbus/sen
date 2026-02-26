// === test_kernel.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/kernel/test_kernel.h"

// implementation
#include "kernel_impl.h"

// sen
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/bootloader.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/kernel.h"
#include "sen/kernel/kernel_config.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace sen::kernel
{

//--------------------------------------------------------------------------------------------------------------
// TestKernel
//--------------------------------------------------------------------------------------------------------------

TestKernel::TestKernel(Component* component)
{
  KernelConfig::ComponentToLoad componentConfig;
  {
    componentConfig.config.group = 2;
    componentConfig.component.instance = component;
    componentConfig.component.config.group = 2;
  }

  KernelConfig config;
  config.addToLoad(componentConfig);

  init(std::move(config));
}

TestKernel::TestKernel(KernelConfig config) { init(std::move(config)); }

void TestKernel::init(KernelConfig config)
{
  // edit the parameters
  auto params = config.getParams();
  params.runMode = kernel::RunMode::virtualTime;
  config.setParams(params);

  kernel_ = std::make_unique<Kernel>(std::move(config));
  std::ignore = kernel_->run(KernelBlockMode::doNotBlock);
}

TestKernel::~TestKernel() { kernel_->requestStop(); }

void TestKernel::step(std::size_t count)
{
  for (std::size_t i = 0; i < count; ++i)
  {
    auto nextDelta = kernel_->pimpl_->computeNextExecutionDeltaTime();
    kernel_->pimpl_->advanceTimeDrainInputsAndUpdate(nextDelta);
    kernel_->pimpl_->flushVirtualTime();

    if (nextDelta.getNanoseconds() == 0)
    {
      nextDelta = kernel_->pimpl_->computeNextExecutionDeltaTime();
    }
    virtualTime_ += nextDelta;
  }
}

TimeStamp TestKernel::getTime() const { return virtualTime_; }

std::shared_ptr<ObjectSource> TestKernel::getComponentBus(std::string_view componentName,
                                                          const BusAddress& busAddress) const
{
  auto& runners = kernel_->pimpl_->getRunners();
  auto itr = std::find_if(runners.begin(),
                          runners.end(),
                          [componentName](const auto& runner)
                          { return runner->getComponentContext().info.name == componentName; });

  return itr != runners.end() ? (*itr)->getOrCreateLocalParticipant(busAddress) : nullptr;
}

[[nodiscard]] std::optional<const ComponentContext*> TestKernel::getComponentContext(
  std::string_view componentName) const
{
  auto& runners = kernel_->pimpl_->getRunners();
  auto itr = std::find_if(runners.begin(),
                          runners.end(),
                          [componentName](const auto& runner)
                          { return runner->getComponentContext().info.name == componentName; });

  return itr != runners.end() ? std::make_optional(&(*itr)->getComponentContext()) : std::nullopt;
}

[[nodiscard]] CustomTypeRegistry& TestKernel::getTypes() const { return kernel_->pimpl_->getTypes(); }

TestKernel TestKernel::fromYamlFile(const std::filesystem::path& configFilePath)
{
  return TestKernel(Bootloader::fromYamlFile(configFilePath, false)->getConfig());
}

TestKernel TestKernel::fromYamlString(const std::string& configString)
{
  return TestKernel(Bootloader::fromYamlString(configString, false)->getConfig());
}

//--------------------------------------------------------------------------------------------------------------
// TestComponent
//--------------------------------------------------------------------------------------------------------------

void TestComponent::onInit(InitFunc func) { init_ = std::move(func); }

void TestComponent::onRun(RunFunc func) { run_ = std::move(func); }

void TestComponent::onUnload(UnloadFunc func) { unload_ = std::move(func); }

PassResult TestComponent::init(InitApi&& api)
{
  if (init_)
  {
    return init_(std::move(api));
  }
  return done();
}

FuncResult TestComponent::run(RunApi& api)
{
  if (run_)
  {
    return run_(api);
  }
  return done();
}

FuncResult TestComponent::unload(UnloadApi&& api)
{
  if (unload_)
  {
    return unload_(std::move(api));
  }
  return done();
}

}  // namespace sen::kernel
