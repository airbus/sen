// === generators_test.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "every_kind_model.h"

// sen
#include "sen/gen/cpp.h"
#include "sen/gen/html.h"
#include "sen/gen/json.h"
#include "sen/gen/mkdocs.h"
#include "sen/gen/plantuml.h"
#include "sen/gen/python.h"
#include "sen/gen/typescript.h"

// 3rd party
#include <gtest/gtest.h>

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

}  // namespace
