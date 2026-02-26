// === python_generator.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "python/python_generator.h"

// app
#include "cpp/json_type_storage.h"
#include "cpp/util.h"

// templates
#include "python_templates/alias_decl.h"
#include "python_templates/class_decl.h"
#include "python_templates/enum_decl.h"
#include "python_templates/file_decl.h"
#include "python_templates/optional_decl.h"
#include "python_templates/quantity_decl.h"
#include "python_templates/sequence_decl.h"
#include "python_templates/struct_decl.h"
#include "python_templates/variant_decl.h"

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
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

struct PythonTemplates
{
  inja::Template structTemplate;
  inja::Template enumTemplate;
  inja::Template variantTemplate;
  inja::Template classTemplate;
  inja::Template sequenceTemplate;
  inja::Template aliasTemplate;
  inja::Template quantityTemplate;
};

PythonTemplates makeTemplates(inja::Environment& env)
{
  PythonTemplates result;
  result.structTemplate = env.parse(sen::decompressSymbolToString(struct_decl, struct_declSize));
  result.enumTemplate = env.parse(sen::decompressSymbolToString(enum_decl, enum_declSize));
  result.variantTemplate = env.parse(sen::decompressSymbolToString(variant_decl, variant_declSize));
  result.classTemplate = env.parse(sen::decompressSymbolToString(class_decl, class_declSize));
  result.sequenceTemplate = env.parse(sen::decompressSymbolToString(sequence_decl, sequence_declSize));
  result.aliasTemplate = env.parse(sen::decompressSymbolToString(alias_decl, alias_declSize));
  result.quantityTemplate = env.parse(sen::decompressSymbolToString(quantity_decl, quantity_declSize));
  return result;
}

class TemplateVisitor: protected sen::TypeVisitor
{
public:
  static std::string generate(const sen::CustomType& type,
                              inja::Environment& env,
                              JsonTypeStorage& typeStorage,
                              const std::string& packageName,
                              const PythonTemplates& templates)
  {
    TemplateVisitor visitor(env, typeStorage, packageName, templates);
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
    static auto enumTypeTemplate = sen::decompressSymbolToString(enum_decl, enum_declSize);
    compute(type, enumTypeTemplate);
  }

  void apply(const sen::VariantType& type) override
  {
    static auto variantTypeTemplate = sen::decompressSymbolToString(variant_decl, variant_declSize);
    compute(type, variantTypeTemplate);
  }

  void apply(const sen::SequenceType& type) override
  {
    static auto sequenceTypeTemplate = sen::decompressSymbolToString(sequence_decl, sequence_declSize);
    compute(type, sequenceTypeTemplate);
  }

  void apply(const sen::AliasType& type) override
  {
    static auto aliasTypeTemplate = sen::decompressSymbolToString(alias_decl, alias_declSize);
    compute(type, aliasTypeTemplate);
  }

  void apply(const sen::OptionalType& type) override
  {
    static auto optionalTypeTemplate = sen::decompressSymbolToString(optional_decl, optional_declSize);
    compute(type, optionalTypeTemplate);
  }

  void apply(const sen::QuantityType& type) override
  {
    static auto quantityTypeTemplate = sen::decompressSymbolToString(quantity_decl, quantity_declSize);
    compute(type, quantityTypeTemplate);
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
                  const PythonTemplates& templates) noexcept
    : env_(env), typeStorage_(typeStorage), packageName_(std::move(packageName)), templates_(templates)
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
  const PythonTemplates& templates_;
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
                         const PythonTemplates& templates)
{
  std::string visitorResult;
  inja::json fileData;
  std::vector<std::string> imports;

  for (auto& storageElem: storage)
  {
    auto set = storageElem->getTypeSet();
    auto packageName = computePackageName(set);

    for (const auto& element: set.importedSets)
    {
      std::filesystem::path path(element->fileName);
      std::string modulePath = path.parent_path().string();
      std::replace(modulePath.begin(), modulePath.end(), '/', '.');
      std::replace(modulePath.begin(), modulePath.end(), '\\', '.');
      modulePath.append(".");

      // once the path is obtained, replace the invalid characters to ensure that the included file complies with
      // the python naming standard.
      auto validImportFilename = path.stem().string();
      PythonGenerator::replaceInvalidCharacters(validImportFilename);

      modulePath.append(validImportFilename);
      imports.push_back(modulePath);
    }

    for (const auto& type: set.types)
    {
      visitorResult.append(TemplateVisitor::generate(*type, env, *storageElem, packageName, templates));
    }
  }

  fileData["visitorResult"] = visitorResult;
  if (!imports.empty())
  {
    fileData["imports"] = imports;
  }

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
// PythonGenerator
//--------------------------------------------------------------------------------------------------------------

void PythonGenerator::replaceInvalidCharacters(std::string& str)
{
  std::vector<char> pythonForbiddenChars {'.', '-'};

  for (auto charToReplace: pythonForbiddenChars)
  {
    std::replace(str.begin(),
                 str.end(),
                 charToReplace,
                 '_');  // replace all the forbidden characters on the file with _
  }
}

PythonGenerator::PythonGenerator(const sen::lang::TypeSet& typeSet): typeSet_(std::move(typeSet))
{
  storage_.reserve(1U);
  storage_.emplace_back(std::make_shared<JsonTypeStorage>(typeSet_));
}

void PythonGenerator::write(const std::filesystem::path& inputFile)
{
  inja::Environment env;
  configureEnv(env);
  auto templates = makeTemplates(env);

  auto outputFile = inputFile.stem().string();

  // replace python invalid characters from target file name
  replaceInvalidCharacters(outputFile);

  outputFile.append(".py");

  std::ofstream stream;
  openFile(outputFile, stream);
  stream << generateFile(env, storage_, templates);
  SEN_ENSURE(stream.good());
  stream.close();

  std::cout << "stl|python> " << outputFile << std::endl;
}
