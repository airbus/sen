// === python_generator.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_PYTHON_PYTHON_GENERATOR_H
#define SEN_APPS_CLI_GEN_SRC_PYTHON_PYTHON_GENERATOR_H

#include "common/json_type_storage.h"
#include "sen/core/lang/stl_resolver.h"

// cli11
#include <CLI/App.hpp>
// NOLINTNEXTLINE (misc-include-cleaner): cli11 needs all headers to correctly link required vtables
#include <CLI/CLI.hpp>

// std
#include <filesystem>
#include <string>
#include <vector>

// Generates Python code out of .stl files
class PythonGenerator
{
  SEN_NOCOPY_NOMOVE(PythonGenerator)

public:
  // Takes the type set that we will generate code for
  // and the folder where the files shall be written.
  explicit PythonGenerator(const sen::lang::TypeSet& typeSet);
  ~PythonGenerator() noexcept = default;

public:
  /// Initializes python generator
  static void setup(CLI::App& app);

  /// Replace the characters of the filename that will be considered invalid in Python with underscores.
  static void replaceInvalidCharacters(std::string& str);

  // Generates the files or throws std::exception.
  void write(const std::filesystem::path& inputFile);

private:
  const sen::lang::TypeSet& typeSet_;
  std::vector<std::shared_ptr<JsonTypeStorage>> storage_;
};

#endif  // SEN_APPS_CLI_GEN_SRC_PYTHON_PYTHON_GENERATOR_H
