// === mkdocs_generator.h ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_MKDOCS_GENERATOR_H
#define SEN_APPS_CLI_GEN_SRC_MKDOCS_GENERATOR_H

#include "cpp/json_type_storage.h"

// sen
#include "sen/core/lang/stl_resolver.h"

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
  // Generates the file or throws std::exception.
  void write(const std::filesystem::path& outputFile, const std::string& title);

private:
  const sen::lang::TypeSetContext& typeSets_;
  std::vector<std::shared_ptr<JsonTypeStorage>> storage_;
};

#endif  // SEN_APPS_CLI_GEN_SRC_MKDOCS_GENERATOR_H
