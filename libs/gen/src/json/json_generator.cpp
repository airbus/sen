// === json_generator.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/gen/json.h"

// lib
#include "common/type_storage.h"
#include "common/util.h"
#include "json/json_templates.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/uuid.h"
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

// templates (baked symbols)
#include "json_templates/config_schema.h"

// inja
#include <inja/environment.hpp>
#include <inja/json.hpp>
#include <inja/template.hpp>

// std
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace kernel_files
{

#include "kernel_stls/basic_types.h"
#include "kernel_stls/log.h"

}  // namespace kernel_files

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
                              const detail::JsonTemplateSet& templates)
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
                  const detail::JsonTemplateSet& templates) noexcept
    : env_(env), typeStorage_(typeStorage), templates_(templates)
  {
  }

  template <typename T>
  inline void compute(const T& type, const inja::Template& aTemplate)
  {
    if (!aTemplate.content.empty())
    {
      result_ = env_.render(aTemplate, typeStorage_.getOrCreate(type));
    }
  }

private:
  inja::Environment& env_;
  detail::TypeStorage& typeStorage_;
  const detail::JsonTemplateSet& templates_;
  std::string result_;
};

void writeTempFile(const std::filesystem::path& path, const std::string& contents)
{
  if (const auto parentPath = path.parent_path(); !parentPath.empty() && !std::filesystem::exists(parentPath))
  {
    std::filesystem::create_directories(parentPath);
  }
  std::ofstream stream(path);
  stream << contents;
  SEN_ENSURE(stream.good());
  stream.close();
}

}  // namespace

class JsonGenerator::Impl
{
  SEN_NOCOPY_NOMOVE(Impl)

public:
  Impl()
  {
    detail::configureEnv(env_);
    templates_ = detail::makeJsonTypeTemplates(env_);
  }
  ~Impl() = default;

public:
  std::string generate(const sen::lang::TypeSetContext& typeSets,
                       const std::string& schemaName,
                       const std::vector<std::string>& classes,
                       const std::string& componentName,
                       bool componentMode)
  {
    return generateSchema(typeSets, schemaName, classes, componentName, componentMode);
  }

  std::string renderType(const sen::CustomType& type)
  {
    // TypeStorage caches per-type inja data; the TypeSet reference is only used for the
    // class-template namespace field, unused by the JSON output. An empty TypeSet is safe.
    static const sen::lang::TypeSet emptySet;
    detail::TypeStorage storage(emptySet);
    // Per-type templates emit `"<qualName>": { ... }` -- a key-value pair, the shape file-level
    // bundling needs inside a $defs map. For the standalone single-type API we want just the
    // value, so wrap to make valid JSON, parse, and extract.
    auto fragment = TemplateVisitor::generate(type, env_, storage, templates_);
    auto wrapped = nlohmann::json::parse(std::string {"{"} + fragment + "}");
    return wrapped.at(type.getQualifiedName()).dump();
  }

  std::string combine(const std::vector<std::string>& schemaContents, const std::string& schemaName)
  {
    std::set<std::string> capturedDefinitions;
    std::vector<std::string> definitionData;
    std::vector<inja::json> componentData;
    std::vector<inja::json> classesData;

    std::vector<std::string> schemaFilesContent;
    schemaFilesContent.push_back(getKernelSchema());

    std::set<std::string> components;
    std::set<std::string> classes;

    for (const auto& schemaContent: schemaContents)
    {
      schemaFilesContent.emplace_back(schemaContent);
    }

    // parse all the schemas
    for (const auto& schemaContent: schemaFilesContent)
    {
      auto schema = inja::json::parse(schemaContent);

      auto defs = schema["$defs"];
      for (const auto& def: defs.items())
      {
        if (capturedDefinitions.find(def.key()) == capturedDefinitions.end())
        {
          capturedDefinitions.insert(def.key());

          std::string defValue;
          defValue.append("\"");
          defValue.append(def.key());
          defValue.append("\":\n");
          defValue.append(def.value().dump(2));
          definitionData.emplace_back(defValue);
        }
      }

      if (auto itr = schema.find("component"); itr != schema.end())
      {
        if (components.count(itr.value()) == 0)
        {
          components.insert(itr.value());

          inja::json component;
          component["name"] = itr.value();

          if (auto configItr = schema.find("configType"); configItr != schema.end())
          {
            component["configType"] = configItr.value();
          }
          componentData.push_back(std::move(component));
        }
      }

      if (auto itr = schema.find("classes"); itr != schema.end())
      {
        for (const auto& aClass: itr.value())
        {
          if (classes.count(aClass) == 0)
          {
            classes.insert(aClass);
            classesData.push_back(aClass);
          }
        }
      }
    }

    inja::json fileData;
    fileData["definitions"] = definitionData;
    if (!componentData.empty())
    {
      fileData["components"] = componentData;
    }

    if (!classesData.empty())
    {
      fileData["classes"] = classesData;
    }

    fileData["schemaName"] = schemaName;

    return env_.render(sen::decompressSymbolToString(config_schema, config_schemaSize), fileData);
  }

private:
  void collectDependentTypeSets(const sen::lang::TypeSet* set, std::set<const sen::lang::TypeSet*>& allSets)
  {
    // do nothing if already present
    if (allSets.count(set) != 0)
    {
      return;
    }

    // add the current set
    allSets.insert(set);

    // add the imported sets
    for (auto& imported: set->importedSets)
    {
      collectDependentTypeSets(imported, allSets);
    }
  }

  bool canMakeInstances(const sen::ClassType* classType)
  {
    if (auto methods = classType->getMethods(sen::ClassType::SearchMode::includeParents); !methods.empty())
    {
      return false;
    }

    auto props = classType->getProperties(sen::ClassType::SearchMode::includeParents);
    for (const auto& prop: props)
    {
      if (prop->getCheckedSet())
      {
        return false;
      }
    }

    return true;
  }

  std::set<const sen::lang::TypeSet*> collectAllTypeSets(const sen::lang::TypeSetContext& typeSets)
  {
    std::set<const sen::lang::TypeSet*> result;
    for (auto& set: typeSets)
    {
      collectDependentTypeSets(&set, result);
    }
    return result;
  }

  std::string getConfigType(const sen::lang::TypeSetContext& typeSets, bool componentMode)
  {
    if (!componentMode)
    {
      return {};
    }

    for (auto& set: typeSets)
    {
      for (const auto& type: set.types)
      {
        if (type->getName() == "Configuration")
        {
          return std::string(type->getQualifiedName());
        }
      }
    }
    return {};
  }

  void addUserDefinedClasses(const std::vector<std::string>& classes,
                             std::set<std::string>& writtenTypes,
                             sen::ConstTypeHandle<sen::ClassType> classType,
                             std::vector<inja::json>& definitions,
                             std::vector<inja::json>& classesThatCanBeInstantiated,
                             detail::TypeStorage& storage,
                             const detail::JsonTemplateSet& templates)
  {
    for (const auto& userQualName: classes)
    {
      if (writtenTypes.count(userQualName) == 0)
      {
        if (userQualName.find(classType->getQualifiedName()) != std::string::npos)
        {
          sen::ClassSpec spec;
          spec.name = userQualName.substr(userQualName.find_last_of('.') + 1);
          spec.parents.push_back(classType);
          spec.qualifiedName = userQualName;
          spec.isInterface = false;

          // `userClass` owns the synthesized type only for this iteration: the template
          // render below and `storage.get()` both copy what they need before it dies.
          auto userClass = sen::ClassType::make(spec);
          writtenTypes.insert(userQualName);
          definitions.emplace_back(TemplateVisitor::generate(*userClass, env_, storage, templates));

          classesThatCanBeInstantiated.push_back(storage.get(*userClass));
        }
      }
    }
  }

  std::string generateSchema(const sen::lang::TypeSetContext& typeSets,
                             const std::string& schemaName,
                             const std::vector<std::string>& classes,
                             const std::string& componentName,
                             bool componentMode)
  {
    inja::json fileData;
    fileData["schemaName"] = schemaName;

    auto allTypeSets = collectAllTypeSets(typeSets);
    auto configType = getConfigType(typeSets, componentMode);

    std::vector<inja::json> definitions;
    std::vector<inja::json> classesThatCanBeInstantiated;
    std::set<std::string> writtenTypes;

    for (auto& typeSet: allTypeSets)
    {
      detail::TypeStorage storage(*typeSet);

      for (const auto& type: storage.getTypeSet().types)
      {
        const std::string typeQualName(type->getQualifiedName());
        if (writtenTypes.count(typeQualName) != 0)
        {
          continue;
        }

        writtenTypes.insert(typeQualName);
        definitions.emplace_back(TemplateVisitor::generate(*type, env_, storage, templates_));

        if (!type->isClassType())
        {
          continue;
        }
        auto classType = sen::dynamicTypeHandleCast<const sen::ClassType>(type).value();

        if (canMakeInstances(classType.type()))
        {
          classesThatCanBeInstantiated.push_back(storage.get(*classType));
        }

        addUserDefinedClasses(
          classes, writtenTypes, classType, definitions, classesThatCanBeInstantiated, storage, templates_);
      }
    }

    fileData["definitions"] = definitions;
    fileData["classesThatCanBeInstantiated"] = classesThatCanBeInstantiated;
    fileData["componentMode"] = componentMode;
    fileData["componentName"] = componentName;
    if (!configType.empty())
    {
      fileData["configType"] = configType;
    }

    return env_.render(templates_.schemaFile, fileData);
  }

  std::string getKernelSchema()
  {
    auto basePath = std::filesystem::temp_directory_path() / sen::Uuid().toString();
    auto targetPath = basePath / "stl" / "sen" / "kernel";

    auto logPath = targetPath / "log.stl";
    auto basicTypesPath = targetPath / "basic_types.stl";

    try
    {
      writeTempFile(logPath, sen::decompressSymbolToString(kernel_files::log, kernel_files::logSize));
      writeTempFile(basicTypesPath,
                    sen::decompressSymbolToString(kernel_files::basic_types, kernel_files::basic_typesSize));

      sen::lang::TypeSetContext typeSets;
      std::ignore = sen::lang::readTypesFile(logPath, {basePath}, typeSets, {});
      std::ignore = sen::lang::readTypesFile(basicTypesPath, {basePath}, typeSets, {});

      auto schema = generateSchema(typeSets, "sen_kernel", {}, {}, false);
      std::error_code ec;
      std::filesystem::remove_all(basePath, ec);
      return schema;
    }
    catch (...)
    {
      // Best-effort cleanup on the failure path; swallow remove_all errors so the
      // original exception propagates intact.
      std::error_code ec;
      std::filesystem::remove_all(basePath, ec);
      throw;
    }
  }

private:
  inja::Environment env_;
  detail::JsonTemplateSet templates_;
};

//--------------------------------------------------------------------------------------------------------------
// JsonGenerator
//--------------------------------------------------------------------------------------------------------------

JsonGenerator::JsonGenerator(): pimpl_(std::make_unique<Impl>()) {}

JsonGenerator::~JsonGenerator() = default;

std::string JsonGenerator::generatePackage(const sen::lang::TypeSetContext& typeSets, const PackageOptions& opts)
{
  return pimpl_->generate(typeSets, opts.schemaName, opts.classes, {}, false);
}

std::string JsonGenerator::generateComponent(const sen::lang::TypeSetContext& typeSets, const ComponentOptions& opts)
{
  return pimpl_->generate(typeSets, opts.schemaName, {}, opts.componentName, true);
}

std::string JsonGenerator::combineSchemas(const std::vector<std::string>& schemaContents, const std::string& schemaName)
{
  return pimpl_->combine(schemaContents, schemaName);
}

std::string JsonGenerator::renderTypeSchema(const sen::CustomType& type) { return pimpl_->renderType(type); }

}  // namespace sen::gen
