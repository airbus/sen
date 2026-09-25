// === generators_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "every_kind_model.h"

// sen
#include "sen/core/lang/fom_parser.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/variant_type.h"
#include "sen/gen/cpp.h"
#include "sen/gen/html.h"
#include "sen/gen/json.h"
#include "sen/gen/mkdocs.h"
#include "sen/gen/plantuml.h"
#include "sen/gen/python.h"
#include "sen/gen/typescript.h"

// 3rd party
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

// std
#include <array>
#include <string>
#include <string_view>

namespace
{

using sen::gen::test::everyKindStl;
using sen::gen::test::ResolvedModel;

// What each generator says about the model, flattened so one assertion can ask whether a name
// reached the output. For html that is the model file alone: its other four files are the
// application, and searching ~60KB of prose and stylesheet alongside the data made assertions
// that could not fail — "radius" appears 19 times in the stylesheet and "measure" inside a
// comment.
[[nodiscard]] std::string renderedBy(std::string_view generator, const ResolvedModel& model)
{
  if (generator == "html")
  {
    const auto files = sen::gen::HtmlGenerator {}.generate(model.context(), "t");
    const auto modelFile = files.find("model.js");
    if (modelFile != files.end())
    {
      return modelFile->second;
    }
    ADD_FAILURE() << "the html generator emitted no model.js";
    return {};
  }
  if (generator == "json")
  {
    return sen::gen::JsonGenerator {}.generatePackage(model.context());
  }
  if (generator == "mkdocs")
  {
    return sen::gen::MkDocsGenerator {}.generate(model.context(), "t");
  }
  if (generator == "uml")
  {
    return sen::gen::PlantUMLGenerator {}.generate(
      model.context(), sen::gen::PlantUMLGenerationMode::all, sen::gen::PlantUMLEnumMode::all);
  }
  if (generator == "python")
  {
    return sen::gen::PythonGenerator {}.generateModule(model.set());
  }
  if (generator == "typescript")
  {
    std::string all;
    for (const auto& [path, contents]: sen::gen::TypeScriptGenerator {}.generate(model.context()))
    {
      all += contents;
    }
    return all;
  }
  if (generator == "cpp")
  {
    std::string all;
    for (const auto& [path, contents]: sen::gen::CppGenerator {}.generate(model.set()))
    {
      all += contents;
    }
    return all;
  }
  ADD_FAILURE() << "no generator named " << generator;
  return {};
}

constexpr std::array<std::string_view, 7> generators {"html", "json", "mkdocs", "uml", "python", "typescript", "cpp"};

// The data types. Every generator renders all of these, whatever else it does or does not do,
// and they are what the shared type storage produces. A regression there lands in all seven at
// once, which is why this is asserted across all seven rather than in any one of them.
constexpr std::array<std::string_view, 9>
  dataTypes {"Metres", "Colour", "Point", "Circle", "Figure", "Track", "Triple", "MaybeColour", "Distance"};

class EveryGenerator: public ::testing::TestWithParam<std::string_view>
{
};

TEST_P(EveryGenerator, rendersSomething)
{
  const ResolvedModel model {everyKindStl};

  EXPECT_FALSE(renderedBy(GetParam(), model).empty());
}

TEST_P(EveryGenerator, namesEveryDataTypeTheModelDeclares)
{
  const ResolvedModel model {everyKindStl};
  const auto output = renderedBy(GetParam(), model);

  for (const auto& name: dataTypes)
  {
    EXPECT_NE(output.find(name), std::string::npos) << GetParam() << " does not name " << name;
  }
}

// A type reaching the output says nothing about its contents. Blanking the field name in the
// shared type storage left every name-only assertion green while all seven emitted nameless
// fields.
TEST_P(EveryGenerator, namesTheFieldsOfAStructure)
{
  const ResolvedModel model {everyKindStl};
  const auto output = renderedBy(GetParam(), model);

  for (const auto& field: {"centre", "radius"})
  {
    EXPECT_NE(output.find(field), std::string::npos) << GetParam() << " does not name field " << field;
  }
}

TEST_P(EveryGenerator, listsTheValuesOfAnEnumeration)
{
  const ResolvedModel model {everyKindStl};
  const auto output = renderedBy(GetParam(), model);

  for (const auto& value: {"red", "green", "blue"})
  {
    EXPECT_NE(output.find(value), std::string::npos) << GetParam() << " does not name enumerator " << value;
  }
}

INSTANTIATE_TEST_SUITE_P(Generators,
                         EveryGenerator,
                         ::testing::ValuesIn(generators),
                         [](const auto& info) { return std::string {info.param}; });

// What a generator does with a class is its own decision, so each is stated rather than
// assumed. TypeScript renders only a notification interface per event, and Python renders the
// fields without the methods; both are deliberate.
TEST(GeneratorsAndClasses, renderTheClassHierarchyWhereThatIsTheirJob)
{
  const ResolvedModel model {everyKindStl};

  for (const auto* generator: {"html", "json", "mkdocs", "uml", "cpp", "python"})
  {
    const auto output = renderedBy(generator, model);
    EXPECT_NE(output.find("Base"), std::string::npos) << generator << " drops the base class";
    EXPECT_NE(output.find("Derived"), std::string::npos) << generator << " drops the derived class";

    for (const auto* property: {"one", "shape", "trail", "tint", "span", "fixed"})
    {
      EXPECT_NE(output.find(property), std::string::npos) << generator << " does not name property " << property;
    }
  }
}

TEST(GeneratorsAndClasses, renderMethodsAndEventsWhereThatIsTheirJob)
{
  const ResolvedModel model {everyKindStl};

  for (const auto* generator: {"html", "json", "mkdocs", "uml"})
  {
    const auto output = renderedBy(generator, model);
    EXPECT_NE(output.find("measure"), std::string::npos) << generator << " drops the method";
    EXPECT_NE(output.find("moved"), std::string::npos) << generator << " drops the event";
  }
}

// TypeScript emits an interface per event and nothing else from a class.
TEST(GeneratorsAndClasses, typeScriptRendersAnEventAsANotification)
{
  const ResolvedModel model {everyKindStl};
  const auto output = renderedBy("typescript", model);

  EXPECT_NE(output.find("MovedNotification"), std::string::npos);
}

TEST(GeneratorsAndFomVariants, appendEmptyStateToRprVariants)
{
  const auto context = sen::lang::parseFomDocuments({RPR_FOM_DIR}, {}, {});
  const auto schema = nlohmann::json::parse(sen::gen::JsonGenerator {}.generatePackage(context));
  size_t checked = 0U;

  for (const auto& set: context)
  {
    for (const auto& type: set.types)
    {
      const auto name = type->getName();
      if (name != "RFModulationTypeVariantStruct" && name != "SpreadSpectrumVariantStruct")
      {
        continue;
      }
      SCOPED_TRACE(name);
      ++checked;
      const auto* variant = type->asVariantType();
      ASSERT_NE(variant, nullptr);
      const auto fields = variant->getFields();
      ASSERT_EQ(fields.size(), name == "RFModulationTypeVariantStruct" ? 7U : 5U);
      for (size_t i = 0U; i < fields.size(); ++i)
      {
        EXPECT_EQ(fields[i].key, i);
      }
      EXPECT_EQ(fields[0].type->getName(),
                name == "RFModulationTypeVariantStruct" ? "AmplitudeModulationTypeEnum16" : "SINCGARSModulationStruct");
      const auto* empty = fields[fields.size() - 1U].type->asStructType();
      ASSERT_NE(empty, nullptr);
      EXPECT_EQ(empty->getQualifiedName(), "sen.Monostate");
      EXPECT_TRUE(empty->getFields().empty());

      std::string cpp;
      for (const auto& [path, contents]: sen::gen::CppGenerator {}.generate(set))
      {
        cpp += contents;
      }
      const auto declarationStart = cpp.find("using " + std::string(name) + " = std::variant<");
      ASSERT_NE(declarationStart, std::string::npos);
      const auto declaration = cpp.substr(declarationStart, cpp.find(';', declarationStart) - declarationStart);
      EXPECT_NE(declaration.find(", std::monostate>"), std::string::npos);

      const auto python = sen::gen::PythonGenerator {}.generateModule(set);
      EXPECT_NE(python.find("isinstance(val, dict)"), std::string::npos);
      EXPECT_NE(python.find("self.type = \"sen.Monostate\""), std::string::npos);

      const auto& emptySchema = schema.at("$defs").at(std::string(type->getQualifiedName())).at("allOf").back();
      EXPECT_EQ(emptySchema.at("if").at("properties").at("type").at("const"), "Monostate");
      EXPECT_EQ(emptySchema.at("then").at("properties").at("value"),
                (nlohmann::json {{"type", "object"}, {"maxProperties", 0}}));
    }
  }
  EXPECT_EQ(checked, 2U);

  std::string typescript;
  for (const auto& [path, contents]: sen::gen::TypeScriptGenerator {}.generate(context))
  {
    typescript += contents;
  }
  EXPECT_NE(typescript.find("type: \"sen.Monostate\"; value: Record<string, never>"), std::string::npos);
}

TEST(GeneratorsAndFomVariants, keepStlVariantsUnchanged)
{
  const ResolvedModel model {everyKindStl};
  const auto cpp = renderedBy("cpp", model);
  EXPECT_NE(cpp.find("using Figure = std::variant<Point, Circle>;"), std::string::npos);
  EXPECT_EQ(cpp.find("std::monostate"), std::string::npos);
}

}  // namespace
