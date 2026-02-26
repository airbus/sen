// === kernel_config.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_KERNEL_KERNEL_CONFIG_H
#define SEN_KERNEL_KERNEL_CONFIG_H

// sen
#include "sen/kernel/detail/kernel_fwd.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <filesystem>

namespace sen::kernel
{

/// Holds a component instance and basic info. \ingroup kernel
struct ComponentContext
{
  Component* instance = nullptr;
  ComponentConfig config {};
  ComponentInfo info {};
};

/// Holds the kernel configuration information. \ingroup kernel
class KernelConfig final
{
public:
  /// Information about a component to load from a plugin
  struct ComponentPluginToLoad
  {
    const std::string path;  ///< Path to the shared object
    ComponentConfig config;  ///< Component configuration parameters
    VarMap params;           ///< Dynamic user-defined parameters
  };

  /// Information about a component to load
  struct ComponentToLoad
  {
    ComponentContext component;  ///< Component instance
    ComponentConfig config;      ///< Component configuration parameters
    VarMap params;               ///< Dynamic user-defined parameters
  };

  /// Information about how to create an object
  struct ObjectConfig
  {
    std::string name;       ///< The unique name of the object (process-wide)
    std::string className;  ///< The name of its class.
    BusAddress bus;         ///< The bus where to publish it, if any
    VarMap params;          ///< Dynamic user-defined parameters
  };

  /// Information about how to create a pipeline
  struct PipelineToLoad
  {
    std::string name;                   /// The name of the pipeline
    ComponentConfig config;             /// Config data (of the owning component)
    StringList imports;                 /// Libraries to import
    std::vector<ObjectConfig> objects;  /// Objects to create
    Duration period;                    /// Time between updates (0 for event-triggered execution)
    VarMap params;                      /// Dynamic user-defined parameters
  };

public:  // special members
  KernelConfig() = default;
  ~KernelConfig() noexcept = default;
  SEN_COPY_ASSIGN(KernelConfig) = default;
  SEN_MOVE_ASSIGN(KernelConfig) = default;
  SEN_COPY_CONSTRUCT(KernelConfig) = default;
  SEN_MOVE_CONSTRUCT(KernelConfig) = default;

public:  // execution management
  /// Sets general parameters
  void setParams(KernelParams params) noexcept;

  /// Gets the behavior of the kernel when calling run.
  /// By default it is blockUntilDone.
  [[nodiscard]] const KernelParams& getParams() const noexcept;

public:  // main configuration settings
  /// Adds a plugin to be loaded by the kernel upon startup.
  /// Overrides previous parameters in case of repeated plugins.
  void addToLoad(ComponentPluginToLoad plugin);

  /// Adds a component to be loaded by the kernel upon startup.
  /// Overrides previous parameters in case of repeated plugins.
  void addToLoad(ComponentToLoad component);

  /// Adds a pipeline to be loaded by the kernel upon startup.
  /// Overrides previous parameters in case of repeated plugins.
  void addToLoad(PipelineToLoad pipeline);

public:
  /// The components to be loaded from plugins on startup.
  [[nodiscard]] const std::vector<ComponentPluginToLoad>& getPluginsToLoad() const noexcept;

  /// The in-memory components to be loaded on startup.
  [[nodiscard]] const std::vector<ComponentToLoad>& getComponentsToLoad() const noexcept;

  /// The pipelines to be loaded on startup.
  [[nodiscard]] const std::vector<PipelineToLoad>& getPipelinesToLoad() const noexcept;

public:
  /// Sets the path to the configuration file's path used to construct the kernel.
  void setConfigFilePath(const std::filesystem::path& path) noexcept { path_ = path; }

  /// Gets the path to the configuration file used to construct the kernel.
  [[nodiscard]] std::filesystem::path getConfigFilePath() const noexcept { return path_; }

private:
  std::vector<ComponentPluginToLoad> pluginsToLoad_;
  std::vector<ComponentToLoad> componentsToLoad_;
  std::vector<PipelineToLoad> pipelinesToLoad_;
  KernelParams params_ {};
  std::filesystem::path path_ {};
};

}  // namespace sen::kernel

#endif  // SEN_KERNEL_KERNEL_CONFIG_H
