// === json_generator.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "json/json_generator.h"

// app
#include "cpp/json_type_storage.h"
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

// templates
#include "json_templates/config_schema.h"

// inja
#include <inja/environment.hpp>
#include <inja/json.hpp>
#include <inja/template.hpp>

// std
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef WIN32
#  include <cstdlib>
#endif

namespace kernel_files
{

#include "json_templates/basic_types.h"
#include "json_templates/log.h"

}  // namespace kernel_files

namespace
{

class TemplateVisitor: protected sen::TypeVisitor
{
public:
  static std::string generate(const sen::CustomType& type,
                              inja::Environment& env,
                              JsonTypeStorage& typeStorage,
                              const JsonTemplateSet& templates)
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
  TemplateVisitor(inja::Environment& env, JsonTypeStorage& typeStorage, const JsonTemplateSet& templates) noexcept
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
  JsonTypeStorage& typeStorage_;
  const JsonTemplateSet& templates_;
  std::string result_;
};

void openFile(const std::filesystem::path& path, std::ofstream& stream)
{
  stream.open(path, std::ios_base::trunc | std::ios_base::out);
  if (!stream.is_open() || stream.fail())
  {
    std::string err;
    err.append("could not open file '");
    err.append(path.string());
    err.append("' for writing");
    sen::throwRuntimeError(err);
  }
}

template <typename F>
void writeFile(const std::filesystem::path& path, std::vector<std::filesystem::path>& writtenFiles, F&& generator)
{
  // don't do anything if already written
  if (std::find(writtenFiles.begin(), writtenFiles.end(), path) != writtenFiles.end())
  {
    return;
  }

  const auto fileContents = generator();
  writtenFiles.push_back(path);

  if (const auto parentPath = path.parent_path(); !parentPath.empty() && !std::filesystem::exists(parentPath))
  {
    std::filesystem::create_directories(parentPath);
  }

  // do not rewrite the file if it has the same content
  if (std::filesystem::exists(path))
  {
    std::ifstream in(path);
    std::string existingContents;

    // reserve required memory in one go
    in.seekg(0U, std::ios::end);
    existingContents.reserve(static_cast<std::size_t>(in.tellg()));
    in.seekg(0U, std::ios::beg);

    // read contents
    existingContents.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

    if (existingContents == fileContents)
    {
      return;
    }
  }

  std::ofstream stream;
  openFile(path, stream);
  stream << fileContents;
  SEN_ENSURE(stream.good());
  stream.close();
}

void readFile(const std::filesystem::path& fileName, std::string& contents)
{
  std::ifstream in(fileName);

  // reserve required memory in one go
  in.seekg(0U, std::ios::end);
  contents.reserve(static_cast<std::size_t>(in.tellg()));
  in.seekg(0U, std::ios::beg);

  // read contents
  contents.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}  // namespace

class JsonGenerator::Impl
{
  SEN_NOCOPY_NOMOVE(Impl)

public:
  Impl() = default;

  ~Impl() = default;

public:
  void writeFiles(const sen::lang::TypeSetContext& typeSets,
                  const std::string& outputFile,
                  const std::vector<std::string>& classes,
                  const std::string& componentName,
                  bool componentMode)
  {
    std::vector<std::filesystem::path> writtenFiles;
    writeFilesImpl(writtenFiles, typeSets, outputFile, classes, componentName, componentMode);

    for (const auto& file: writtenFiles)
    {
      std::cout << "stl|json> " << file << std::endl;
    }
  }

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
                             std::vector<sen::ClassType*>& userTypes,
                             std::vector<inja::json>& definitions,
                             std::vector<inja::json>& classesThatCanBeInstantiated,
                             JsonTypeStorage& storage,
                             const JsonTemplateSet& templates)
  {
    // look for any pending user-defined class
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

          auto userClass = sen::ClassType::make(spec);
          userTypes.push_back(userClass.type());  // TODO: check lifetime
          writtenTypes.insert(userQualName);
          // do we need mods on storage or only reading? Do we copy the content that is necessary or do we cross link?
          definitions.emplace_back(TemplateVisitor::generate(*userClass, env_, storage, templates));

          classesThatCanBeInstantiated.push_back(storage.get(*userClass));
        }
      }
    }
  }

  std::string generateSchema(const sen::lang::TypeSetContext& typeSets,
                             const std::string& outputFile,
                             const std::vector<std::string>& classes,
                             const std::string& componentName,
                             bool componentMode)
  {

    inja::json fileData;
    fileData["schemaFileName"] = outputFile;
    fileData["schemaName"] = std::filesystem::path(outputFile).stem();

    auto templates = getJsonTypeTemplates(env_);

    // collect all type sets
    auto allTypeSets = collectAllTypeSets(typeSets);
    auto configType = getConfigType(typeSets, componentMode);

    std::vector<inja::json> definitions;
    std::vector<inja::json> classesThatCanBeInstantiated;
    std::set<std::string> writtenTypes;
    std::vector<sen::ClassType*> userTypes;

    for (auto& typeSet: allTypeSets)
    {
      JsonTypeStorage storage(*typeSet);

      for (const auto& type: storage.getTypeSet().types)
      {
        const std::string typeQualName(type->getQualifiedName());
        if (writtenTypes.count(typeQualName) != 0)
        {
          continue;
        }

        writtenTypes.insert(typeQualName);
        definitions.emplace_back(TemplateVisitor::generate(*type, env_, storage, templates));

        // collect classes in the data model
        if (!type->isClassType())
        {
          continue;
        }
        auto classType = sen::dynamicTypeHandleCast<const sen::ClassType>(type).value();

        // check the class from the data definition
        if (canMakeInstances(classType.type()))
        {
          classesThatCanBeInstantiated.push_back(storage.get(*classType));
        }

        // look for any pending user-defined class
        addUserDefinedClasses(
          classes, writtenTypes, classType, userTypes, definitions, classesThatCanBeInstantiated, storage, templates);
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

    return env_.render(templates.schemaFile, fileData);
  }

  void writeFilesImpl(std::vector<std::filesystem::path>& writtenFiles,
                      const sen::lang::TypeSetContext& typeSets,
                      const std::string& outputFile,
                      const std::vector<std::string>& classes,
                      const std::string& componentName,
                      bool componentMode)
  {
    writeFile(outputFile,
              writtenFiles,
              [&]() { return generateSchema(typeSets, outputFile, classes, componentName, componentMode); });
  }

  std::string getKernelSchema()
  {
    std::vector<std::filesystem::path> writtenFiles;

    auto basePath = std::filesystem::temp_directory_path() / sen::Uuid().toString();
    auto targetPath = basePath / "stl" / "sen" / "kernel";

    writeFile(targetPath / "log.stl",
              writtenFiles,
              []() { return sen::decompressSymbolToString(kernel_files::log, kernel_files::logSize); });

    writeFile(targetPath / "basic_types.stl",
              writtenFiles,
              []() { return sen::decompressSymbolToString(kernel_files::basic_types, kernel_files::basic_typesSize); });

    sen::lang::TypeSetContext typeSets;
    for (const auto& file: writtenFiles)
    {
      std::ignore = sen::lang::readTypesFile(file, {basePath}, typeSets, {});
    }

    std::filesystem::remove_all(basePath);

    return generateSchema(typeSets, "sen_kernel.json", {}, {}, false);
  }

  void writeCombination(const std::string& outFile, const std::vector<std::filesystem::path>& schemas)
  {
    std::vector<std::filesystem::path> writtenFiles;

    std::set<std::string> capturedDefinitions;
    std::vector<std::string> definitionData;
    std::vector<inja::json> componentData;
    std::vector<inja::json> classesData;

    std::vector<std::string> schemaFilesContent;
    schemaFilesContent.push_back(getKernelSchema());

    std::set<std::string> components;
    std::set<std::string> classes;

    for (const auto& elem: schemas)
    {
      std::string schemaContent;
      readFile(elem, schemaContent);
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

    fileData["schemaName"] = std::filesystem::path(outFile).stem().string();

    writeFile(outFile,
              writtenFiles,
              [&]() { return env_.render(sen::decompressSymbolToString(config_schema, config_schemaSize), fileData); });
  }

private:
  inja::Environment env_;
};

//--------------------------------------------------------------------------------------------------------------
// JsonGenerator
//--------------------------------------------------------------------------------------------------------------

JsonGenerator::JsonGenerator(): pimpl_(std::make_unique<Impl>()) {}

JsonGenerator::~JsonGenerator() = default;

void JsonGenerator::writePackageFiles(const sen::lang::TypeSetContext& typeSets,
                                      const std::string& outputFile,
                                      const std::vector<std::string>& classes)
{
  pimpl_->writeFiles(typeSets, outputFile, classes, {}, false);
}

void JsonGenerator::writeComponentFiles(const sen::lang::TypeSetContext& typeSets,
                                        const std::string& outputFile,
                                        const std::string& componentName)
{
  pimpl_->writeFiles(typeSets, outputFile, {}, componentName, true);
}

void JsonGenerator::writeCombination(const std::string& outFile, const std::vector<std::filesystem::path>& schemas)
{
  pimpl_->writeCombination(outFile, schemas);
}
