// === cli_input.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_CLI_INPUT_H
#define SEN_APPS_CLI_GEN_SRC_CLI_INPUT_H

// sen
#include "sen/core/lang/stl_resolver.h"

// cli11
#include <CLI/App.hpp>
// NOLINTNEXTLINE (misc-include-cleaner): cli11 needs all headers to correctly link required vtables
#include <CLI/CLI.hpp>

// std
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace sen::cli_gen
{

struct StlArgs
{
  std::vector<std::filesystem::path> inputs;
  std::vector<std::filesystem::path> includePaths;
  // Read by the cpp generator only; ignored on other paths so the shared CLI surface
  // can still accept the flags.
  std::string basePath;
  std::string codegenOptionsFile;
};

struct FomArgs
{
  std::vector<std::filesystem::path> paths;
  std::vector<std::filesystem::path> mappingFiles;
  std::string codegenOptionsFile;
};

CLI::App* setupStlInput(CLI::App& app, std::function<void(std::shared_ptr<StlArgs>)>&& action);

CLI::App* setupFomInput(CLI::App& app, std::function<void(std::shared_ptr<FomArgs>)>&& action);

/// Reads the type settings out of a JSON file.
[[nodiscard]] sen::lang::TypeSettings readTypeSettings(const std::filesystem::path& path);

}  // namespace sen::cli_gen

#endif  // SEN_APPS_CLI_GEN_SRC_CLI_INPUT_H
