// === plantuml_generator.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/gen/plantuml.h"

// lib
#include "common/type_storage.h"
#include "common/util.h"
#include "plantuml/plantuml_templates.h"

// templates (baked symbols)
#include "plantuml_templates/file_decl.h"

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
#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
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
                              PlantUMLEnumMode enumMode,
                              const detail::PlantUmlTemplateSet& templates)
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

  void apply(const sen::StructType& type) override { compute(type, templates_.structType); }

  void apply(const sen::EnumType& type) override
  {
    if (enumMode_ != PlantUMLEnumMode::noEnumerators)
    {
      compute(type, templates_.enumType);
    }
  }

  void apply(const sen::VariantType& type) override { compute(type, templates_.variantType); }

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

  void apply(const sen::ClassType& type) override { compute(type, templates_.classType); }

private:
  TemplateVisitor(inja::Environment& env,
                  detail::TypeStorage& typeStorage,
                  std::string packageName,
                  PlantUMLEnumMode enumMode,
                  const detail::PlantUmlTemplateSet& templates) noexcept
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
  detail::TypeStorage& typeStorage_;
  std::string packageName_;
  std::string result_;
  PlantUMLEnumMode enumMode_;
  const detail::PlantUmlTemplateSet& templates_;
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

class PlantUMLGenerator::Impl
{
  SEN_NOCOPY_NOMOVE(Impl)

public:
  Impl()
  {
    detail::configureEnv(env_);
    templates_ = detail::makePlantumlTypeTemplates(env_);
    fileTemplate_ = env_.parse(sen::decompressSymbolToString(file_decl, file_declSize));
  }
  ~Impl() = default;

public:
  std::string generate(const sen::lang::TypeSetContext& typeSets,
                       PlantUMLGenerationMode generationMode,
                       PlantUMLEnumMode enumMode)
  {
    std::vector<std::shared_ptr<detail::TypeStorage>> storage;
    storage.reserve(typeSets.size());
    for (const auto& elem: typeSets)
    {
      storage.emplace_back(std::make_shared<detail::TypeStorage>(elem));
    }

    std::string visitorResult;
    for (auto& storageElem: storage)
    {
      auto set = storageElem->getTypeSet();
      auto packageName = computePackageName(set);

      for (const auto& type: set.types)
      {
        if (generationMode == PlantUMLGenerationMode::onlyClasses && !type->isClassType())
        {
          continue;
        }

        if (generationMode == PlantUMLGenerationMode::onlyBasicTypes && type->isClassType())
        {
          continue;
        }

        visitorResult.append(TemplateVisitor::generate(*type, env_, *storageElem, packageName, enumMode, templates_));
      }
    }

    inja::json fileData;
    fileData["visitorResult"] = visitorResult;
    return env_.render(fileTemplate_, fileData);
  }

private:
  inja::Environment env_;
  detail::PlantUmlTemplateSet templates_;
  inja::Template fileTemplate_;
};

//--------------------------------------------------------------------------------------------------------------
// PlantUMLGenerator
//--------------------------------------------------------------------------------------------------------------

PlantUMLGenerator::PlantUMLGenerator(): pimpl_(std::make_unique<Impl>()) {}

PlantUMLGenerator::~PlantUMLGenerator() = default;

std::string PlantUMLGenerator::generate(const sen::lang::TypeSetContext& typeSets,
                                        PlantUMLGenerationMode generationMode,
                                        PlantUMLEnumMode enumMode)
{
  return pimpl_->generate(typeSets, generationMode, enumMode);
}

}  // namespace sen::gen
