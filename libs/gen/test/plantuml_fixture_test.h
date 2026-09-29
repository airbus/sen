// === plantuml_fixture_test.h =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_GEN_TEST_PLANTUML_FIXTURE_TEST_H
#define SEN_LIBS_GEN_TEST_PLANTUML_FIXTURE_TEST_H

#include "sen/gen/plantuml.h"

// sen
#include "sen/core/lang/fom_parser.h"
#include "sen/core/lang/stl_parser.h"
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/lang/stl_scanner.h"
#include "sen/core/lang/stl_statement.h"

// 3rd party
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

class APlantUMLGenerator: public ::testing::Test
{
protected:
  bool generateStl(const std::string& stl,
                   sen::gen::PlantUMLGenerationMode generationMode = sen::gen::PlantUMLGenerationMode::all,
                   sen::gen::PlantUMLEnumMode enumMode = sen::gen::PlantUMLEnumMode::all)
  {
    sen::lang::StlScanner scanner {stl};
    sen::lang::StlParser parser {scanner.scanTokens()};
    statements_ = parser.parse();

    sen::lang::ResolverContext resolverContext {};
    sen::lang::StlResolver resolver {statements_, resolverContext, context_};

    if (resolver.resolve({}) == nullptr)
    {
      return false;
    }

    content_ = sen::gen::PlantUMLGenerator {}.generate(context_, generationMode, enumMode);
    return true;
  }

  void generateFom(const std::vector<std::filesystem::path>& paths,
                   sen::gen::PlantUMLGenerationMode generationMode = sen::gen::PlantUMLGenerationMode::all,
                   sen::gen::PlantUMLEnumMode enumMode = sen::gen::PlantUMLEnumMode::all)
  {
    context_ = sen::lang::parseFomDocuments(paths, {}, sen::lang::TypeSettings {});
    content_ = sen::gen::PlantUMLGenerator {}.generate(context_, generationMode, enumMode);
  }

  [[nodiscard]] std::size_t find(const std::string& phrase, std::size_t from = 0) const
  {
    return content_.find(phrase, from);
  }

  [[nodiscard]] std::string content() const { return content_; }

  [[nodiscard]] std::filesystem::path dataDirectoryPath() const { return TEST_DATA_DIR; }

private:
  std::vector<sen::lang::StlStatement> statements_;
  sen::lang::TypeSetContext context_;
  std::string content_;
};

#endif  // SEN_LIBS_GEN_TEST_PLANTUML_FIXTURE_TEST_H
