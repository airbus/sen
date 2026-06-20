// === typescript_generator.cpp ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/gen/typescript.h"

// lib
#include "common/type_storage.h"
#include "common/util.h"
#include "typescript_templates.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/event.h"
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

// std
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace sen::gen
{

namespace
{

/// Dispatches one type to the matching template; throws if the type is not supported.
class TemplateVisitor: protected sen::TypeVisitor
{
public:
  static std::string generate(const sen::CustomType& type,
                              inja::Environment& env,
                              detail::TypeStorage& typeStorage,
                              const detail::TypeScriptTemplateSet& templates)
  {
    TemplateVisitor visitor(env, typeStorage, templates);
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
  void apply(const sen::EnumType& type) override { compute(type, templates_.enumTemplate); }
  void apply(const sen::VariantType& type) override { compute(type, templates_.variantTemplate); }
  void apply(const sen::SequenceType& type) override { compute(type, templates_.sequenceTemplate); }
  void apply(const sen::AliasType& type) override { compute(type, templates_.aliasTemplate); }
  void apply(const sen::OptionalType& type) override { compute(type, templates_.optionalTemplate); }
  void apply(const sen::QuantityType& type) override { compute(type, templates_.quantityTemplate); }
  void apply(const sen::ClassType& type) override { compute(type, templates_.classTemplate); }

private:
  TemplateVisitor(inja::Environment& env,
                  detail::TypeStorage& typeStorage,
                  const detail::TypeScriptTemplateSet& templates) noexcept
    : env_(env), typeStorage_(typeStorage), templates_(templates)
  {
  }

  template <typename T>
  void compute(const T& type, const inja::Template& tpl)
  {
    result_ = env_.render(tpl, typeStorage_.getOrCreate(type));
  }

  inja::Environment& env_;
  detail::TypeStorage& typeStorage_;
  std::string result_;
  const detail::TypeScriptTemplateSet& templates_;
};

/// `<stem>` of an STL file, e.g. `jsonrpc.stl` -> `jsonrpc`. Used both as the output `.ts`
/// basename and as the module specifier in cross-file imports / the barrel.
std::string stemOf(const std::string& stlFileName) { return std::filesystem::path(stlFileName).stem().string(); }

/// Walk one type's direct references to other CustomTypes, calling `visit` with each one.
template <typename F>
void forEachReferencedCustomType(const sen::Type& type, F&& visit)
{
  auto consider = [&](const sen::Type& ref)
  {
    if (ref.isCustomType())
    {
      visit(*ref.asCustomType());
    }
  };

  if (type.isStructType())
  {
    const auto& st = *type.asStructType();
    for (const auto& field: st.getFields())
    {
      consider(*field.type);
    }
    if (st.getParent())
    {
      consider(*st.getParent().value());
    }
  }
  else if (type.isVariantType())
  {
    for (const auto& arm: type.asVariantType()->getFields())
    {
      consider(*arm.type);
    }
  }
  else if (type.isSequenceType())
  {
    consider(*type.asSequenceType()->getElementType());
  }
  else if (type.isOptionalType())
  {
    consider(*type.asOptionalType()->getType());
  }
  else if (type.isAliasType())
  {
    consider(*type.asAliasType()->getAliasedType());
  }
  else if (type.isClassType())
  {
    // Only event-arg types are emitted as notification structs, so only those types need cross-
    // file imports. Methods / properties / parents are not emitted from classes.
    constexpr auto searchMode = sen::ClassType::SearchMode::doNotIncludeParents;
    for (const auto& event: type.asClassType()->getEvents(searchMode))
    {
      for (const auto& arg: event->getArgs())
      {
        consider(*arg.type);
      }
    }
  }
}

}  // namespace

class TypeScriptGenerator::Impl
{
public:
  Impl()
  {
    detail::configureEnv(env_);
    templates_ = detail::makeTypeScriptTypeTemplates(env_);
    fileTemplate_ = detail::makeTypeScriptFileTemplate(env_);
  }

  FileContents generate(const sen::lang::TypeSetContext& typeSets)
  {
    const auto allTypeSets = detail::collectAllTypeSets(typeSets);

    // qualified-name -> owning-stem map across every TypeSet we'll touch. Used to classify each
    // type reference as "lives in this file" vs "lives in <stem>.ts" so per-file imports are
    // minimal (only what the file actually uses) rather than re-importing the whole imported
    // set.
    std::map<std::string, std::string> qualNameToStem;
    // Two qualified-name-distinct types sharing an unqualified name would clash in the
    // barrel `export *`. Detect at generation time and fail loud.
    std::map<std::string, std::string> nameToFirstQualName;
    for (const auto* typeSet: allTypeSets)
    {
      const auto stem = stemOf(typeSet->fileName);
      for (const auto& type: typeSet->types)
      {
        std::string qualName(type->getQualifiedName());
        std::string unqualName(type->getName());
        auto [it, inserted] = nameToFirstQualName.emplace(unqualName, qualName);
        if (!inserted && it->second != qualName)
        {
          std::string err;
          err.append("sen::gen::TypeScriptGenerator: unqualified name collision: '");
          err.append(unqualName);
          err.append("' defined in both '");
          err.append(it->second);
          err.append("' and '");
          err.append(qualName);
          err.append("'. TS barrel re-exports would conflict; rename one or split into separate generation runs.");
          sen::throwRuntimeError(err);
        }
        qualNameToStem.emplace(std::move(qualName), stem);
      }
    }

    FileContents outputs;
    std::set<std::string> writtenTypes;
    std::vector<std::string> emittedStems;  // tracks barrel ordering, depth-first by import order.

    for (const auto* typeSet: allTypeSets)
    {
      const std::string stem = stemOf(typeSet->fileName);

      detail::TypeStorage storage(*typeSet);
      std::string visitorResult;
      // Stem -> sorted set of type names imported from that stem. Sorted for deterministic
      // output.
      std::map<std::string, std::set<std::string>> refsByStem;

      for (const auto& type: storage.getTypeSet().types)
      {
        const std::string typeQualName(type->getQualifiedName());
        if (writtenTypes.count(typeQualName) != 0)
        {
          continue;
        }
        writtenTypes.insert(typeQualName);
        // Blank line separator between declarations. Done generator-side so per-template
        // trailing whitespace policies (pre-commit trims trailing blank lines) don't change the
        // output shape.
        if (!visitorResult.empty())
        {
          visitorResult.push_back('\n');
        }
        visitorResult.append(TemplateVisitor::generate(*type, env_, storage, templates_));

        forEachReferencedCustomType(*type,
                                    [&](const sen::CustomType& ref)
                                    {
                                      auto it = qualNameToStem.find(std::string(ref.getQualifiedName()));
                                      if (it == qualNameToStem.end() || it->second == stem)
                                      {
                                        return;
                                      }
                                      refsByStem[it->second].insert(std::string(ref.getName()));
                                    });
      }

      // Named imports per imported file: only the types this file actually references.
      std::vector<inja::json> imports;
      for (const auto& [importedStem, names]: refsByStem)
      {
        inja::json entry;
        entry["stem"] = importedStem;
        entry["names"] = inja::json::array();
        for (const auto& n: names)
        {
          entry["names"].push_back(n);
        }
        imports.push_back(entry);
      }

      inja::json fileData;
      fileData["stem"] = stem;
      fileData["visitorResult"] = visitorResult;
      if (!imports.empty())
      {
        fileData["imports"] = imports;
      }

      outputs.emplace(std::filesystem::path(stem + ".ts"), env_.render(fileTemplate_, fileData));
      emittedStems.push_back(stem);
    }

    // Barrel: re-exports every emitted file under the shared `index.ts` entry point.
    if (!emittedStems.empty())
    {
      std::ostringstream barrel;
      barrel << "// Auto-generated. Do not edit.\n\n";
      for (const auto& stem: emittedStems)
      {
        barrel << "export * from \"./" << stem << ".js\";\n";
      }
      outputs.emplace(std::filesystem::path("index.ts"), barrel.str());
    }

    return outputs;
  }

private:
  inja::Environment env_;
  detail::TypeScriptTemplateSet templates_;
  inja::Template fileTemplate_;
};

TypeScriptGenerator::TypeScriptGenerator(): pimpl_(std::make_unique<Impl>()) {}
TypeScriptGenerator::~TypeScriptGenerator() = default;

TypeScriptGenerator::FileContents TypeScriptGenerator::generate(const sen::lang::TypeSetContext& typeSets)
{
  return pimpl_->generate(typeSets);
}

}  // namespace sen::gen
