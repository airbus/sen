// === plantuml_generator.h ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_PLANTUML_PLANTUML_GENERATOR_H
#define SEN_APPS_CLI_GEN_SRC_PLANTUML_PLANTUML_GENERATOR_H

#include "cpp/json_type_storage.h"
#include "sen/core/lang/stl_resolver.h"

enum class UMLGenerationMode
{
  all,
  onlyClasses,
  onlyBasicTypes
};

enum class UMLEnumMode
{
  all,
  noEnumerators
};

// Generates PlantUML code out of .stl files
class PlantUMLGenerator
{
  SEN_NOCOPY_NOMOVE(PlantUMLGenerator)

public:
  // Takes the type set that we will generate code for
  // and the folder where the files shall be written.
  explicit PlantUMLGenerator(const sen::lang::TypeSetContext& typeSets);
  ~PlantUMLGenerator() noexcept = default;

public:
  // Generates the files or throws std::exception.
  void write(const std::filesystem::path& outputFile, UMLGenerationMode generationMode, UMLEnumMode enumMode);

private:
  const sen::lang::TypeSetContext& typeSets_;
  std::vector<std::shared_ptr<JsonTypeStorage>> storage_;
};

#endif  // SEN_APPS_CLI_GEN_SRC_PLANTUML_PLANTUML_GENERATOR_H
