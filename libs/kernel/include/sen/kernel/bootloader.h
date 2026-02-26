// === bootloader.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_KERNEL_BOOTLOADER_H
#define SEN_KERNEL_BOOTLOADER_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/kernel/kernel.h"
#include "sen/kernel/kernel_config.h"

// std
#include <filesystem>
#include <memory>
#include <string>

namespace sen::kernel
{

/// Loads a configuration YAML file and returns its contents as a sen::VarMap.
/// This function does not validate the correctness of the configuration, but simply
/// translates the contents into the Var. It resolves included yamls. \ingroup kernel
[[nodiscard]] VarMap getConfigAsVarFromYaml(const std::filesystem::path& path, bool printFinalConfig = false);

/// Loads a configuration YAML file and returns its contents as a sen::VarMap.
/// This function does not validate the correctness of the configuration, but simply
/// translates the contents into the Var. It resolves included yamls.
/// If the "path" argument is not correct, it will not resolve the inclusions. \ingroup kernel
[[nodiscard]] VarMap getConfigAsVarFromYaml(const std::string& content,
                                            const std::filesystem::path& path,
                                            bool printFinalConfig = false);

/// Configures a kernel for running. \ingroup kernel
class Bootloader final
{
  SEN_NOCOPY_NOMOVE(Bootloader)

public:
  /// Opens a YAML configuration file and stores it in a KernelConfig structure.
  static std::unique_ptr<Bootloader> fromYamlFile(const std::filesystem::path& path, bool printConfig);

  /// Reads the configuration from a string in YAML format
  static std::unique_ptr<Bootloader> fromYamlString(const std::string& config, bool printConfig);

  ~Bootloader();

public:
  /// The loaded configuration info
  [[nodiscard]] KernelConfig& getConfig() noexcept;

private:
  Bootloader();

private:
  void loadConfigFromString(const std::string& content, const std::filesystem::path& path, bool printFinalConfig);

  void loadConfigFromFile(const std::filesystem::path& path, bool printFinalConfig);

  void loadKernelConfig(const VarMap& data);

  void loadComponents(const VarMap& data);

  void loadPipelines(const VarMap& data);

  void component(std::string_view path, const Var& data);

  void pipeline(std::string_view name, const Var& data);

  static void fetchPipelineObjects(KernelConfig::PipelineToLoad& pipelineConfig, const VarMap& map);

  static void fetchPipelineComponentConfig(KernelConfig::PipelineToLoad& pipelineConfig, const Var& data);

  static void fetchPipelineExecPeriod(KernelConfig::PipelineToLoad& pipelineConfig, const VarMap& map);

  static void fetchPipelineUserDefinedParams(KernelConfig::PipelineToLoad& pipelineConfig, const VarMap& map);

private:
  KernelConfig config_;
  VarMap configVar_;
};

}  // namespace sen::kernel

#endif  // SEN_KERNEL_BOOTLOADER_H
