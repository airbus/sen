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

/// Bundles a `Component` instance with its static metadata for in-process registration.
/// \ingroup kernel
struct ComponentContext
{
  Component* instance = nullptr;  ///< Non-owning pointer to the component; lifetime must exceed the kernel's.
  ComponentConfig config {};      ///< Configuration block passed to every lifecycle API.
  ComponentInfo info {};          ///< Static identity and build metadata of the component.
};

/// Immutable description of the kernel's startup configuration.
/// Build one of these (directly or via `KernelBootloader`) and pass it to `Kernel`.
/// \ingroup kernel
class KernelConfig final
{
public:
  /// Identifies a component shared library to load at startup.
  struct ComponentPluginToLoad
  {
    const std::string path;  ///< Filesystem path to the `.so` / `.dll` / `.dylib` plugin file.
    ComponentConfig config;  ///< Configuration block passed to every lifecycle API of the component.
    VarMap params;           ///< Dynamic user-defined parameters available via `ConfigGetter::getConfig()`.
  };

  /// Identifies an already-instantiated in-memory component to register at startup.
  struct ComponentToLoad
  {
    ComponentContext component;  ///< Component instance (pointer, config, and info).
    ComponentConfig config;      ///< Configuration block passed to every lifecycle API.
    VarMap params;               ///< Dynamic user-defined parameters.
  };

  /// Describes a single object to be created by a pipeline component.
  struct ObjectConfig
  {
    std::string name;       ///< Unique instance name for the object (process-wide).
    std::string className;  ///< Fully-qualified STL class name (e.g. `"sen.geometry.Point"`).
    BusAddress bus;         ///< Bus on which to publish the object at creation time.
    VarMap params;          ///< Initial property values and dynamic parameters for the object.
  };

  /// Describes a pipeline to load â€” a lightweight built-in component that hosts a set of objects.
  struct PipelineToLoad
  {
    std::string name;                   ///< Human-readable name of the pipeline (used in logs).
    ComponentConfig config;             ///< Configuration block of the owning pipeline component.
    StringList imports;                 ///< STL library paths to import before creating objects.
    std::vector<ObjectConfig> objects;  ///< Objects to instantiate when the pipeline starts.
    Duration period;                    ///< Cycle period; zero means event-triggered (no fixed rate).
    VarMap params;                      ///< Additional dynamic parameters.
  };

public:  // special members
  KernelConfig() = default;
  ~KernelConfig() noexcept = default;
  SEN_COPY_ASSIGN(KernelConfig) = default;
  SEN_MOVE_ASSIGN(KernelConfig) = default;
  SEN_COPY_CONSTRUCT(KernelConfig) = default;
  SEN_MOVE_CONSTRUCT(KernelConfig) = default;

public:  // execution management
  /// Sets the global kernel execution parameters (run mode, real-time settings, etc.).
  /// @param params New parameters to apply.
  void setParams(KernelParams params) noexcept;

  /// Returns the current kernel execution parameters.
  /// @return Const reference to the `KernelParams`; valid for the lifetime of this config.
  [[nodiscard]] const KernelParams& getParams() const noexcept;

public:  // main configuration settings
  /// Adds a shared-library plugin to the startup load list.
  /// If a plugin with the same path was already added, its parameters are overwritten.
  /// @param plugin Description of the shared library to load.
  void addToLoad(ComponentPluginToLoad plugin);

  /// Adds an in-memory component to the startup load list.
  /// If a component with the same name was already added, its parameters are overwritten.
  /// @param component Description of the in-process component to register.
  void addToLoad(ComponentToLoad component);

  /// Adds a pipeline to the startup load list.
  /// If a pipeline with the same name was already added, its parameters are overwritten.
  /// @param pipeline Description of the pipeline to create.
  void addToLoad(PipelineToLoad pipeline);

public:
  /// Returns all plugin components scheduled to load at startup.
  /// @return Const reference to the plugin list; valid for the lifetime of this config.
  [[nodiscard]] const std::vector<ComponentPluginToLoad>& getPluginsToLoad() const noexcept;

  /// Returns all in-memory components scheduled to load at startup.
  /// @return Const reference to the component list; valid for the lifetime of this config.
  [[nodiscard]] const std::vector<ComponentToLoad>& getComponentsToLoad() const noexcept;

  /// Returns all pipelines scheduled to load at startup.
  /// @return Const reference to the pipeline list; valid for the lifetime of this config.
  [[nodiscard]] const std::vector<PipelineToLoad>& getPipelinesToLoad() const noexcept;

public:
  /// Records the path to the YAML configuration file associated with this config object.
  /// @param path Filesystem path to the config file.
  void setConfigFilePath(const std::filesystem::path& path) noexcept { path_ = path; }

  /// Returns the filesystem path of the YAML configuration file, if one was set.
  /// @return The config file path, or an empty path if not set.
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
