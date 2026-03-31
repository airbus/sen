// === plantuml_generator.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "plantuml/plantuml_generator.h"

#include "plantuml_templates/class_decl.h"
#include "plantuml_templates/enum_decl.h"
#include "plantuml_templates/file_decl.h"
#include "plantuml_templates/struct_decl.h"
#include "plantuml_templates/variant_decl.h"

// app
#include "common/json_type_storage.h"
#include "common/util.h"
#include "plantuml/plantuml_templates.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/hash32.h"
#include "sen/core/lang/fom_parser.h"
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_visitor.h"
#include "sen/core/meta/variant_type.h"

// cli11
#include <CLI/App.hpp>
// NOLINTNEXTLINE (misc-include-cleaner): cli11 needs all headers to correctly link required vtables
#include <CLI/CLI.hpp>

// inja
#include <inja/environment.hpp>
#include <inja/json.hpp>
#include <inja/template.hpp>

// std
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

class TemplateVisitor: protected sen::TypeVisitor
{
public:
  static std::string generate(const sen::CustomType& type,
                              inja::Environment& env,
                              JsonTypeStorage& typeStorage,
                              const std::string& packageName,
                              UMLEnumMode enumMode,
                              const PlantUmlTemplates& templates)
  {
    TemplateVisitor visitor(env, typeStorage, packageName, enumMode, templates);
    type.accept(visitor);
    return visitor.result_;
  }

protected:
  void apply(const sen::Type& type) override
  {
    std::string err;
    err.append("unsupported type '");
    err.append(type.getName());
    err.append("'");
    sen::throwRuntimeError(err);
  }

  void apply(const sen::StructType& type) override { compute(type, templates_.structTemplate); }

  void apply(const sen::EnumType& type) override
  {
    if (enumMode_ != UMLEnumMode::noEnumerators)
    {
      compute(type, templates_.enumTemplate);
    }
  }

  void apply(const sen::VariantType& type) override { compute(type, templates_.variantTemplate); }

  void apply(const sen::SequenceType& type) override
  {
    std::ignore = type;  // not depicted
  }

  void apply(const sen::AliasType& type) override
  {
    std::ignore = type;  // not depicted
  }

  void apply(const sen::OptionalType& type) override
  {
    std::ignore = type;  // not depicted
  }

  void apply(const sen::QuantityType& type) override
  {
    std::ignore = type;  // not depicted
  }

  void apply(const sen::ClassType& type) override { compute(type, templates_.classTemplate); }

private:
  TemplateVisitor(inja::Environment& env,
                  JsonTypeStorage& typeStorage,
                  std::string packageName,
                  UMLEnumMode enumMode,
                  const PlantUmlTemplates& templates) noexcept
    : env_(env)
    , typeStorage_(typeStorage)
    , packageName_(std::move(packageName))
    , enumMode_(enumMode)
    , templates_(templates)
  {
  }

  template <typename T>
  inline void compute(const T& type, const inja::Template& templateStr)
  {
    auto typeInfo = typeStorage_.getOrCreate(type);
    typeInfo["package"] = packageName_;

    result_ = env_.render(templateStr, typeInfo);
  }

private:
  inja::Environment& env_;
  JsonTypeStorage& typeStorage_;
  std::string packageName_;
  std::string result_;
  UMLEnumMode enumMode_;
  const PlantUmlTemplates& templates_;
};

std::string computePackageName(const sen::lang::TypeSet& set)
{
  std::string result;
  for (std::size_t i = 0U; i < set.package.size(); ++i)
  {
    result.append(set.package[i]);
    if (i != set.package.size() - 1U)
    {
      result.append(".");
    }
  }

  return result;
}

std::string generateFile(inja::Environment& env,
                         std::vector<std::shared_ptr<JsonTypeStorage>>& storage,
                         UMLGenerationMode generationMode,
                         UMLEnumMode enumMode,
                         const PlantUmlTemplates& templates)
{
  std::string visitorResult;

  for (auto& storageElem: storage)
  {
    auto set = storageElem->getTypeSet();
    auto packageName = computePackageName(set);

    for (const auto& type: set.types)
    {
      // if only generate classes, and we do not have a class, skip
      if (generationMode == UMLGenerationMode::onlyClasses && !type->isClassType())
      {
        continue;
      }

      // if only generate non-classes, and we have a class, skip
      if (generationMode == UMLGenerationMode::onlyBasicTypes && type->isClassType())
      {
        continue;
      }

      visitorResult.append(TemplateVisitor::generate(*type, env, *storageElem, packageName, enumMode, templates));
    }
  }

  inja::json fileData;
  fileData["visitorResult"] = visitorResult;
  static auto fileTemplate = sen::decompressSymbolToString(file_decl, file_declSize);
  return env.render(fileTemplate, fileData);
}

void setupUmlArgs(CLI::App& app, UmlArgs& args)
{
  app.add_option("-o, --output", args.outputFile, "output plantuml file");
  auto onlyClasses = app.add_flag("--only-classes", args.onlyClasses, "only generate class diagrams");
  auto onlyTypes = app.add_flag("--only-types", args.onlyTypes, "only generate diagrams for basic types");
  auto noEnumerators = app.add_flag("--no-enumerators", args.noEnumerators, "do not generate enumerations");

  onlyClasses->excludes(onlyTypes);
  onlyClasses->excludes(noEnumerators);
}

UMLGenerationMode getUmlGenerationMode(const std::shared_ptr<UmlArgs>& args)
{
  if (args->onlyClasses)
  {
    return UMLGenerationMode::onlyClasses;
  }
  if (args->onlyTypes)
  {
    return UMLGenerationMode::onlyBasicTypes;
  }
  return UMLGenerationMode::all;
}

UMLEnumMode getUmlEnumMode(const std::shared_ptr<UmlArgs>& args)
{
  return args->noEnumerators ? UMLEnumMode::noEnumerators : UMLEnumMode::all;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// PlantUMLGenerator
//--------------------------------------------------------------------------------------------------------------

PlantUMLGenerator::PlantUMLGenerator(const sen::lang::TypeSetContext& typeSets): typeSets_(typeSets)
{
  storage_.reserve(typeSets_.size());
  for (const auto& elem: typeSets_)
  {
    storage_.emplace_back(std::make_shared<JsonTypeStorage>(elem));
  }
}

void PlantUMLGenerator::write(const std::filesystem::path& outputFile,
                              UMLGenerationMode generationMode,
                              UMLEnumMode enumMode)
{
  inja::Environment env;
  configureEnv(env);
  auto templates = makePlantumlTemplates(env);

  std::ofstream stream;
  openFile(outputFile, stream);
  stream << generateFile(env, storage_, generationMode, enumMode, templates);
  SEN_ENSURE(stream.good());
  stream.close();

  std::cout << "stl|uml> " << outputFile << std::endl;
}

void PlantUMLGenerator::setup(CLI::App& app)
{
  auto umlArgs = std::make_shared<UmlArgs>();
  auto uml = app.add_subcommand("uml", "generates UML diagrams");
  uml->allow_extras();
  uml->require_subcommand();

  auto stl = setupStlInput(*uml,
                           [umlArgs](auto args)
                           {
                             sen::lang::TypeSetContext typeSets;
                             for (const auto& fileName: args->inputs)
                             {
                               std::ignore = sen::lang::readTypesFile(fileName, args->includePaths, typeSets, {});
                             }

                             PlantUMLGenerator(typeSets).write(
                               umlArgs->outputFile, getUmlGenerationMode(umlArgs), getUmlEnumMode(umlArgs));
                           });

  setupUmlArgs(*stl, *umlArgs);

  auto fom = setupFomInput(
    *uml,
    [umlArgs](auto args)
    {
      const sen::lang::TypeSetContext typeSets = sen::lang::parseFomDocuments(args->paths, args->mappingFiles, {});

      PlantUMLGenerator(typeSets).write(
        std::filesystem::path(umlArgs->outputFile), getUmlGenerationMode(umlArgs), getUmlEnumMode(umlArgs));
    });

  setupUmlArgs(*fom, *umlArgs);
  stl->excludes(fom);
}
