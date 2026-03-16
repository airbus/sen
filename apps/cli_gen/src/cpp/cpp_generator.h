// === cpp_generator.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_CPP_CPP_GENERATOR_H
#define SEN_APPS_CLI_GEN_SRC_CPP_CPP_GENERATOR_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/lang/stl_resolver.h"

// cli11
#include <CLI/App.hpp>
// NOLINTNEXTLINE (misc-include-cleaner): cli11 needs all headers to correctly link required vtables
#include <CLI/CLI.hpp>

// std
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct Args
{
  std::string packageName;
  std::vector<std::string> stlFiles;
  std::vector<std::string> dependentPackages;
  std::vector<std::string> userTypes;
};

struct CppExtraArgs
{
  bool recursive = false;
  bool publicSymbols = false;
};

/// Generates C++ code out of a type set
class CppGenerator
{
  SEN_NOCOPY_NOMOVE(CppGenerator)

public:
  CppGenerator();
  ~CppGenerator();

public:
  /// Initializes cpp generator
  static void setup(CLI::App& app);

  /// Initializes cpp exports
  static void exportsSetup(CLI::App& app);

  /// Generates the files or throws std::exception.
  void writeFiles(const sen::lang::TypeSet& typeSet, bool recursive, bool publicSymbols, const std::string& basePath);

  /// Generates a cpp file containing a function that exports all the types used in a package.
  static void generatePackageExportsFile(const std::string& packageName,
                                         const std::vector<std::string>& stlFiles,
                                         const std::vector<std::string>& packageDependencies,
                                         const std::vector<std::string>& userImplementedClasses);

private:
  void writeFilesImpl(std::vector<std::filesystem::path>& writtenFiles,
                      const sen::lang::TypeSet& typeSet,
                      bool recursive,
                      bool publicSymbols,
                      const std::string& basePath);

private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

#endif  // SEN_APPS_CLI_GEN_SRC_CPP_CPP_GENERATOR_H
