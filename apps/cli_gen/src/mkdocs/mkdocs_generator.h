// === mkdocs_generator.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_MKDOCS_GENERATOR_H
#define SEN_APPS_CLI_GEN_SRC_MKDOCS_GENERATOR_H

#include "common/json_type_storage.h"

// cli11
#include <CLI/App.hpp>
// NOLINTNEXTLINE (misc-include-cleaner): cli11 needs all headers to correctly link required vtables
#include <CLI/CLI.hpp>

// sen
#include "sen/core/lang/stl_resolver.h"

// std
#include <filesystem>
#include <string>
#include <vector>

struct MKDocsArgs
{
  std::filesystem::path outputFile;
  std::string title = "Package Documentation";
};

// Generates MKDocks code
class MkDocsGenerator
{
  SEN_NOCOPY_NOMOVE(MkDocsGenerator)

public:
  // Takes the type set that we will generate code for
  // and the folder where the files shall be written.
  explicit MkDocsGenerator(const sen::lang::TypeSetContext& typeSets);
  ~MkDocsGenerator() noexcept = default;

public:
  /// Initializes mkdocs generator
  static void setup(CLI::App& app);

  // Generates the file or throws std::exception.
  void write(const std::filesystem::path& outputFile, const std::string& title);

private:
  const sen::lang::TypeSetContext& typeSets_;
  std::vector<std::shared_ptr<JsonTypeStorage>> storage_;
};

#endif  // SEN_APPS_CLI_GEN_SRC_MKDOCS_GENERATOR_H
