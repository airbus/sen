// === python_generator.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_PYTHON_PYTHON_GENERATOR_H
#define SEN_APPS_CLI_GEN_SRC_PYTHON_PYTHON_GENERATOR_H

#include "cpp/json_type_storage.h"
#include "sen/core/lang/stl_resolver.h"

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
  /// Replace the characters of the filename that will be considered invalid in Python with underscores.
  static void replaceInvalidCharacters(std::string& str);

  // Generates the files or throws std::exception.
  void write(const std::filesystem::path& inputFile);

private:
  const sen::lang::TypeSet& typeSet_;
  std::vector<std::shared_ptr<JsonTypeStorage>> storage_;
};

#endif  // SEN_APPS_CLI_GEN_SRC_PYTHON_PYTHON_GENERATOR_H
