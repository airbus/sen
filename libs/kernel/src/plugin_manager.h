// === plugin_manager.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_PLUGIN_MANAGER_H
#define SEN_LIBS_KERNEL_SRC_PLUGIN_MANAGER_H

#include "operating_system.h"
#include "sen/core/base/class_helpers.h"
#include "sen/kernel/kernel_config.h"

#include <memory>
#include <string>
#include <vector>

namespace sen::kernel
{

class PluginContext;

/// Information about a plugin
struct PluginInfo
{
  std::string path;
  ComponentContext component;
};

/// Manages loading and unloading of shared objects
class PluginManager
{
  SEN_NOCOPY_NOMOVE(PluginManager)

public:
  explicit PluginManager(std::shared_ptr<OperatingSystem> os);
  ~PluginManager();

  /// Plug a shared object.
  [[nodiscard]] PluginInfo plug(std::string_view path);

  /// Unplug a shared object.
  /// Does nothing if not previously plugged.
  void unplug(std::string_view path);

  /// Unplug all open plugins
  void unplugAll();

  /// Currently open plugins
  [[nodiscard]] std::vector<PluginInfo> getOpenPlugins() const;

  /// True if already plugged.
  [[nodiscard]] bool isPlugged(std::string_view path) const noexcept;

private:
  std::shared_ptr<OperatingSystem> os_;
  std::vector<std::unique_ptr<PluginContext>> plugins_;
};

}  // namespace sen::kernel

#endif  // SEN_LIBS_KERNEL_SRC_PLUGIN_MANAGER_H
