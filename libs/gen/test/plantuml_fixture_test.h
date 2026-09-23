#include "sen/gen/plantuml.h"

// sen
#include "sen/core/lang/fom_parser.h"
#include "sen/core/lang/stl_parser.h"
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/lang/stl_scanner.h"
#include "sen/core/lang/stl_statement.h"

// 3rd party
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

// std
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

class APlantUMLGenerator: public ::testing::Test
{
protected:
  void generateStl(const std::string& stl,
                   sen::gen::PlantUMLGenerationMode generationMode = sen::gen::PlantUMLGenerationMode::all,
                   sen::gen::PlantUMLEnumMode enumMode = sen::gen::PlantUMLEnumMode::all)
  {
    sen::lang::StlScanner scanner {stl};
    sen::lang::StlParser parser {scanner.scanTokens()};
    statements_ = parser.parse();

    sen::lang::ResolverContext resolverContext {};
    sen::lang::StlResolver resolver {statements_, resolverContext, context_};
    ASSERT_NE(resolver.resolve({}), nullptr);

    content_ = sen::gen::PlantUMLGenerator {}.generate(context_, generationMode, enumMode);
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

  [[nodiscard]] std::filesystem::path archivePath() const { return TEST_DATA_DIR; }

private:
  std::vector<sen::lang::StlStatement> statements_;
  sen::lang::TypeSetContext context_;
  std::string content_;
};
