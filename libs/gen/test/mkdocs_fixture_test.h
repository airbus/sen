#include "sen/gen/mkdocs.h"

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

class AMkdocsGenerator: public ::testing::Test
{
protected:
  void generateStl(const std::string& stl, std::string title = "Documentation")
  {
    sen::lang::StlScanner scanner {stl};
    sen::lang::StlParser parser {scanner.scanTokens()};
    statements_ = parser.parse();

    sen::lang::ResolverContext resolverContext {};
    sen::lang::StlResolver resolver {statements_, resolverContext, context_};
    resolver.resolve({});

    content_ = sen::gen::MkDocsGenerator {}.generate(context_, title);
  }

  void generateFom(const std::vector<std::filesystem::path>& paths, std::string title = "Documentation")
  {
    context_ = sen::lang::parseFomDocuments(paths, {}, sen::lang::TypeSettings {});
    content_ = sen::gen::MkDocsGenerator {}.generate(context_, title);
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
