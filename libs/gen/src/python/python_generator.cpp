// === python_generator.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/gen/python.h"

// lib
#include "common/type_storage.h"
#include "common/util.h"
#include "python/python_templates.h"

// templates (baked symbols)
#include "python_templates/file_decl.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
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
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sen::gen
{

namespace
{

class TemplateVisitor: protected sen::TypeVisitor
{
public:
  static std::string generate(const sen::CustomType& type,
                              inja::Environment& env,
                              detail::TypeStorage& typeStorage,
                              const std::string& packageName,
                              const detail::PythonTemplateSet& templates)
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

  void apply(const sen::StructType& type) override { compute(type, templates_.structType); }

  void apply(const sen::EnumType& type) override { compute(type, templates_.enumType); }

  void apply(const sen::VariantType& type) override { compute(type, templates_.variantType); }

  void apply(const sen::SequenceType& type) override { compute(type, templates_.sequenceType); }

  void apply(const sen::AliasType& type) override { compute(type, templates_.aliasType); }

  void apply(const sen::OptionalType& type) override { compute(type, templates_.optionalType); }

  void apply(const sen::QuantityType& type) override { compute(type, templates_.quantityType); }

  void apply(const sen::ClassType& type) override { compute(type, templates_.classType); }

private:
  TemplateVisitor(inja::Environment& env,
                  detail::TypeStorage& typeStorage,
                  std::string packageName,
                  const detail::PythonTemplateSet& templates) noexcept
    : env_(env), typeStorage_(typeStorage), packageName_(std::move(packageName)), templates_(templates)
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
  detail::TypeStorage& typeStorage_;
  std::string packageName_;
  std::string result_;
  const detail::PythonTemplateSet& templates_;
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

}  // namespace

class PythonGenerator::Impl
{
  SEN_NOCOPY_NOMOVE(Impl)

public:
  Impl()
  {
    detail::configureEnv(env_);
    templates_ = detail::makePythonTypeTemplates(env_);
    fileTemplate_ = env_.parse(sen::decompressSymbolToString(file_decl, file_declSize));
  }
  ~Impl() = default;

public:
  std::string generate(const sen::lang::TypeSet& typeSet)
  {
    auto storage = std::make_shared<detail::TypeStorage>(typeSet);
    auto packageName = computePackageName(typeSet);

    std::string visitorResult;
    std::vector<std::string> imports;

    for (const auto& element: typeSet.importedSets)
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

    for (const auto& type: typeSet.types)
    {
      visitorResult.append(TemplateVisitor::generate(*type, env_, *storage, packageName, templates_));
    }

    inja::json fileData;
    fileData["visitorResult"] = visitorResult;
    if (!imports.empty())
    {
      fileData["imports"] = imports;
    }

    return env_.render(fileTemplate_, fileData);
  }

private:
  inja::Environment env_;
  detail::PythonTemplateSet templates_;
  inja::Template fileTemplate_;
};

//--------------------------------------------------------------------------------------------------------------
// PythonGenerator
//--------------------------------------------------------------------------------------------------------------

void PythonGenerator::replaceInvalidCharacters(std::string& str)
{
  const std::vector<char> pythonForbiddenChars {'.', '-'};
  for (auto charToReplace: pythonForbiddenChars)
  {
    std::replace(str.begin(), str.end(), charToReplace, '_');
  }
}

PythonGenerator::PythonGenerator(): pimpl_(std::make_unique<Impl>()) {}

PythonGenerator::~PythonGenerator() = default;

std::string PythonGenerator::generateModule(const sen::lang::TypeSet& typeSet) { return pimpl_->generate(typeSet); }

}  // namespace sen::gen
