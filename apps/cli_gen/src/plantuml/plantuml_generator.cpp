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
#include "cpp/json_type_storage.h"
#include "cpp/util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/hash32.h"
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
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

struct PlantUmlTemplates
{
  inja::Template structTemplate;
  inja::Template enumTemplate;
  inja::Template variantTemplate;
  inja::Template classTemplate;
};

PlantUmlTemplates makeTemplates(inja::Environment& env)
{
  PlantUmlTemplates result;
  result.structTemplate = env.parse(sen::decompressSymbolToString(struct_decl, struct_declSize));
  result.enumTemplate = env.parse(sen::decompressSymbolToString(enum_decl, enum_declSize));
  result.variantTemplate = env.parse(sen::decompressSymbolToString(variant_decl, variant_declSize));
  result.classTemplate = env.parse(sen::decompressSymbolToString(class_decl, class_declSize));
  return result;
}

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

  void apply(const sen::StructType& type) override
  {
    static auto structTypeTemplate = sen::decompressSymbolToString(struct_decl, struct_declSize);
    compute(type, structTypeTemplate);
  }

  void apply(const sen::EnumType& type) override
  {
    if (enumMode_ != UMLEnumMode::noEnumerators)
    {
      static auto enumTypeTemplate = sen::decompressSymbolToString(enum_decl, enum_declSize);
      compute(type, enumTypeTemplate);
    }
  }

  void apply(const sen::VariantType& type) override
  {
    static auto variantTypeTemplate = sen::decompressSymbolToString(variant_decl, variant_declSize);
    compute(type, variantTypeTemplate);
  }

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

  void apply(const sen::ClassType& type) override
  {
    static auto classTypeTemplate = sen::decompressSymbolToString(class_decl, class_declSize);
    compute(type, classTypeTemplate);
  }

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
  inline void compute(const T& type, std::string_view templateStr)
  {
    if (!templateStr.empty())
    {
      auto typeInfo = typeStorage_.getOrCreate(type);
      typeInfo["package"] = packageName_;

      result_ = env_.render(templateStr, typeInfo);
    }
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

void openFile(const std::filesystem::path& outputFile, std::ofstream& stream)
{
  stream.open(outputFile, std::ios_base::trunc | std::ios_base::out);
  if (!stream.is_open() || stream.fail())
  {
    std::string err;
    err.append("could not open file '");
    err.append(outputFile.generic_string());
    err.append("' for writing");
    sen::throwRuntimeError(err);
  }
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
  auto templates = makeTemplates(env);

  std::ofstream stream;
  openFile(outputFile, stream);
  stream << generateFile(env, storage_, generationMode, enumMode, templates);
  SEN_ENSURE(stream.good());
  stream.close();

  std::cout << "stl|uml> " << outputFile << std::endl;
}
