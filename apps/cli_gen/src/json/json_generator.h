// === json_generator.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_JSON_GENERATOR_H
#define SEN_APPS_CLI_GEN_SRC_JSON_GENERATOR_H

#include "sen/core/lang/stl_resolver.h"

// cli11
#include <CLI/App.hpp>
// NOLINTNEXTLINE (misc-include-cleaner): cli11 needs all headers to correctly link required vtables
#include <CLI/CLI.hpp>

// std
#include <filesystem>
#include <string>
#include <vector>

struct JsonPackageArgs
{
  std::string outFile;
  std::vector<std::string> classes;
};

struct JsonComponentArgs
{
  std::string outFile;
  std::string componentName;
};

struct JsonSchemaArgs
{
  std::string outFile;
  std::vector<std::filesystem::path> schemas;
};

/// Generates a JSON schema out of a type set
class JsonGenerator
{
  SEN_NOCOPY_NOMOVE(JsonGenerator)

public:
  /// Takes the type set that we will generate code for
  /// and the folder where the files shall be written.
  JsonGenerator();
  ~JsonGenerator();

public:
  /// Initializes json package
  static void setupGenJsonPackage(CLI::App& app);

  /// Initializes json component
  static void setupGenJsonComponent(CLI::App& app);

  /// Initializes json schema
  static void setupGenJsonSchemaArgs(CLI::App& app);

  /// Initializes json generator
  static void setup(CLI::App& app);

  /// Generates the schema file for a package (may throw std::exception).
  void writePackageFiles(const sen::lang::TypeSetContext& typeSets,
                         const std::string& outputFile,
                         const std::vector<std::string>& classes);

  /// Generates the schema file for a component (may throw std::exception).
  void writeComponentFiles(const sen::lang::TypeSetContext& typeSets,
                           const std::string& outputFile,
                           const std::string& componentName);

  /// Generates a sen kernel configuration file that can load all the schemas.
  void writeCombination(const std::string& outFile, const std::vector<std::filesystem::path>& schemas);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

#endif  // SEN_APPS_CLI_GEN_SRC_JSON_GENERATOR_H
