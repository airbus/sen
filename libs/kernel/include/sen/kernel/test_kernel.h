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

/// Lightweight kernel wrapper for unit-testing components in virtual (simulated) time.
///
/// A `TestKernel` is not driven by wall-clock time; instead, time advances only when
/// `step()` is called explicitly. This makes tests deterministic and fast.
/// \ingroup kernel
class TestKernel
{
public:
  SEN_NOCOPY_NOMOVE(TestKernel)

public:  // special members
  /// Constructs a `TestKernel` from a fully-populated configuration.
  /// @param config  Configuration describing which components to load.
  explicit TestKernel(KernelConfig config);

  /// Constructs a `TestKernel` with a single in-process component.
  /// @param component  Non-owning pointer to the component; must outlive this kernel.
  explicit TestKernel(Component* component);
  ~TestKernel();

public:  // factories
  /// Creates a `TestKernel` from a YAML configuration file.
  /// @param configFilePath  Path to the YAML file to parse.
  /// @return Fully initialised `TestKernel` ready to `step()`.
  static TestKernel fromYamlFile(const std::filesystem::path& configFilePath);

  /// Creates a `TestKernel` from an inline YAML string.
  /// @param configString  YAML content describing the kernel configuration.
  /// @return Fully initialised `TestKernel` ready to `step()`.
  static TestKernel fromYamlString(const std::string& configString);

public:
  /// Advances virtual time by @p count simulation cycles, running all component logic.
  /// @param count  Number of cycles to execute (default 1).
  void step(std::size_t count = 1);

  /// Returns the current virtual simulation time.
  /// @return `TimeStamp` reflecting how many cycles have been stepped so far.
  [[nodiscard]] TimeStamp getTime() const;

  /// Returns the `ObjectSource` for a specific bus owned by a named component.
  /// @param componentName  Name of the component that owns the bus.
  /// @param busAddress     Structured `{sessionName, busName}` address.
  /// @return Shared pointer to the `ObjectSource`, or `nullptr` if not found.
  [[nodiscard]] std::shared_ptr<ObjectSource> getComponentBus(std::string_view componentName,
                                                              const BusAddress& busAddress) const;

  /// Returns the `ComponentContext` for a named component, if it has been loaded.
  /// @param componentName  Name of the component to look up.
  /// @return `optional` holding a pointer to the `ComponentContext`, or `nullopt` if not found.
  [[nodiscard]] std::optional<const ComponentContext*> getComponentContext(std::string_view componentName) const;

  /// Returns the type registry shared by all components in this test kernel.
  /// @return Mutable reference to the `CustomTypeRegistry`.
  [[nodiscard]] CustomTypeRegistry& getTypes() const;

private:
  void init(KernelConfig config);

private:
  std::unique_ptr<Kernel> kernel_;
  TimeStamp virtualTime_;
};

/// Convenience component that delegates lifecycle phases to user-supplied lambdas.
/// Use this instead of subclassing `Component` when writing concise unit tests.
/// \ingroup kernel
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
  /// Registers the callable to invoke during the `init` lifecycle phase.
  /// @param func  Callable invoked with the `InitApi`; return `done()` when ready.
  void onInit(InitFunc func);

  /// Registers the callable to invoke during the `run` lifecycle phase.
  /// @param func  Callable invoked with the `RunApi`; should exit when `stopRequested()` is set.
  void onRun(RunFunc func);

  /// Registers the callable to invoke during the `unload` lifecycle phase.
  /// @param func  Callable invoked with the `UnloadApi` for cleanup.
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
