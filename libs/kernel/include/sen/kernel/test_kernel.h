// === test_kernel.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_KERNEL_TEST_KERNEL_H
#define SEN_KERNEL_TEST_KERNEL_H

#include "sen/kernel/component.h"
#include "sen/kernel/kernel.h"

// generated code
#include "sen/kernel/kernel_config.h"
#include "stl/sen/kernel/kernel_objects.stl.h"

// std
#include <filesystem>

namespace sen::kernel
{

/// Kernel class only meant for unit testing your packages or components.
/// It starts in virtual time mode.
class TestKernel
{
public:
  SEN_NOCOPY_NOMOVE(TestKernel)

public:  // special members
  explicit TestKernel(KernelConfig config);
  explicit TestKernel(Component* component);
  ~TestKernel();

public:  // factories
  static TestKernel fromYamlFile(const std::filesystem::path& configFilePath);
  static TestKernel fromYamlString(const std::string& configString);

public:
  void step(std::size_t count = 1);
  [[nodiscard]] TimeStamp getTime() const;
  [[nodiscard]] std::shared_ptr<ObjectSource> getComponentBus(std::string_view componentName,
                                                              const BusAddress& busAddress) const;
  [[nodiscard]] std::optional<const ComponentContext*> getComponentContext(std::string_view componentName) const;
  [[nodiscard]] CustomTypeRegistry& getTypes() const;

private:
  void init(KernelConfig config);

private:
  std::unique_ptr<Kernel> kernel_;
  TimeStamp virtualTime_;
};

/// Convenience class for using lambdas instead of inheritance when defining test components.
class TestComponent: public Component
{
  SEN_NOCOPY_NOMOVE(TestComponent)

public:
  using InitFunc = std::function<PassResult(InitApi&&)>;
  using RunFunc = std::function<FuncResult(RunApi&)>;
  using UnloadFunc = std::function<FuncResult(UnloadApi&&)>;

public:
  TestComponent() = default;
  ~TestComponent() override = default;

public:
  void onInit(InitFunc func);
  void onRun(RunFunc func);
  void onUnload(UnloadFunc func);

public:
  PassResult init(InitApi&& api) override;
  FuncResult run(RunApi& api) override;
  FuncResult unload(UnloadApi&& api) override;

private:
  RunFunc run_;
  InitFunc init_;
  UnloadFunc unload_;
};

}  // namespace sen::kernel

#endif  // SEN_KERNEL_TEST_KERNEL_H
