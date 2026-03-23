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

/// Parses a YAML configuration file into a `VarMap`, resolving any `!include` directives.
/// Does not validate configuration correctness; it only performs structural parsing.
/// @param path             Path to the top-level YAML configuration file.
/// @param printFinalConfig If `true`, prints the fully resolved YAML to stdout.
/// @return Parsed `VarMap` representing the configuration tree.
/// \ingroup kernel
[[nodiscard]] VarMap getConfigAsVarFromYaml(const std::filesystem::path& path, bool printFinalConfig = false);

/// Parses an in-memory YAML string into a `VarMap`, resolving `!include` directives relative to @p path.
/// If @p path does not point to a valid directory, includes will not be resolved.
/// @param content          YAML content to parse.
/// @param path             Base path used for resolving `!include` directives.
/// @param printFinalConfig If `true`, prints the fully resolved YAML to stdout.
/// @return Parsed `VarMap` representing the configuration tree.
/// \ingroup kernel
[[nodiscard]] VarMap getConfigAsVarFromYaml(const std::string& content,
                                            const std::filesystem::path& path,
                                            bool printFinalConfig = false);

/// Parses a kernel YAML configuration and produces a `KernelConfig` ready to be passed to `Kernel`.
/// \ingroup kernel
class Bootloader final
{
  SEN_NOCOPY_NOMOVE(Bootloader)

public:
  /// Parses the YAML file at @p path and returns a fully populated `Bootloader`.
  /// @param path         Path to the YAML configuration file.
  /// @param printConfig  If `true`, prints the resolved configuration to stdout.
  /// @return Unique pointer to the initialised `Bootloader`.
  static std::unique_ptr<Bootloader> fromYamlFile(const std::filesystem::path& path, bool printConfig);

  /// Parses an in-memory YAML string and returns a fully populated `Bootloader`.
  /// @param config       YAML configuration content.
  /// @param printConfig  If `true`, prints the resolved configuration to stdout.
  /// @return Unique pointer to the initialised `Bootloader`.
  static std::unique_ptr<Bootloader> fromYamlString(const std::string& config, bool printConfig);

  ~Bootloader();

public:
  /// Returns the `KernelConfig` built from the parsed YAML.
  /// @return Mutable reference to the `KernelConfig`; valid for the lifetime of this `Bootloader`.
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
