// === cpp_generator.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "cpp/cpp_generator.h"

// app
#include "cpp/cpp_templates.h"
#include "cpp/json_type_storage.h"
#include "cpp/util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/hash32.h"
#include "sen/core/io/util.h"
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
#include <ios>
#include <iostream>
#include <iterator>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace
{

class TemplateVisitor: protected sen::TypeVisitor
{
public:
  static std::string generate(const sen::CustomType& type,
                              inja::Environment& env,
                              JsonTypeStorage& typeStorage,
                              const TemplateSet& templates)
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
  TemplateVisitor(inja::Environment& env, JsonTypeStorage& typeStorage, const TemplateSet& templates) noexcept
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
  const TemplateSet& templates_;
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

std::filesystem::path typesSourceFileFromSTLFile(const std::filesystem::path& file)
{
  return file.filename().concat(".h");
}

std::filesystem::path implSourceFileFromSTLFile(const std::filesystem::path& file)
{
  return file.filename().concat(".cpp");
}

std::string computeCppNamespace(const sen::lang::TypeSet& set)
{
  std::string result;
  for (std::size_t i = 0U; i < set.package.size(); ++i)
  {
    result.append(set.package[i]);
    if (i != set.package.size() - 1U)
    {
      result.append("::");
    }
  }

  return result;
}

std::string computeFileId(const std::string& fileName)
{
  std::filesystem::path filePath(fileName);

  std::string stringToUse = filePath.stem().string();
  if (filePath.has_parent_path())
  {
    stringToUse.append(filePath.parent_path().stem().string());
  }

  return intToHex2(sen::crc32(stringToUse));
}

}  // namespace

class CppGenerator::Impl
{
  SEN_NOCOPY_NOMOVE(Impl)

public:
  Impl() { configureEnv(env_); }

  ~Impl() = default;

public:
  void writeFiles(const sen::lang::TypeSet& typeSet, bool recursive, bool publicSymbols, const std::string& basePath)
  {
    std::vector<std::filesystem::path> writtenFiles;
    writeFilesImpl(writtenFiles, typeSet, recursive, publicSymbols, basePath);

    for (const auto& file: writtenFiles)
    {
      std::cout << "stl|cpp> " << file << std::endl;
    }
  }

  void writeFilesImpl(std::vector<std::filesystem::path>& writtenFiles,
                      const sen::lang::TypeSet& typeSet,
                      bool recursive,
                      bool publicSymbols,
                      const std::string& basePath)
  {
    const auto& typesFile = typeSet.fileName;

    auto typesFileName = typesSourceFileFromSTLFile(typesFile);
    auto implFileName = implSourceFileFromSTLFile(typesFile);

    inja::json fileData;
    fileData["namespace"] = computeCppNamespace(typeSet);
    fileData["basePath"] = basePath;
    fileData["typesHeader"] = (typeSet.parentDirectory / typesFileName).string();

    // imports
    {
      std::vector<std::string> imports;

      for (const auto& element: typeSet.importedSets)
      {
        const auto importPath = std::filesystem::path(element->parentDirectory) / element->fileName;

        imports.push_back(importPath.string() + ".h");
      }

      if (!imports.empty())
      {
        fileData["imports"] = imports;
      }
    }

    JsonTypeStorage storage(typeSet, publicSymbols);

    writeFile(typeSet.parentDirectory / typesFileName,
              writtenFiles,
              [&]()
              {
                return generateBaseHeaderFile(
                  getBaseHeaderTemplates(env_), getBaseHeaderFileTemplate(env_), env_, fileData, storage);
              });

    writeFile(typeSet.parentDirectory / implFileName,
              writtenFiles,
              [&]()
              { return generateFile(getImplTemplates(env_), getImplFileTemplate(env_), env_, fileData, storage); });

    if (recursive && !typeSet.importedSets.empty())
    {
      // ensure the imported packages are also generated
      for (const auto& importedSet: typeSet.importedSets)
      {
        writeFilesImpl(writtenFiles, *importedSet, recursive, publicSymbols, basePath);
      }
    }
  }

private:
  static std::string generateFile(const TemplateSet& typeTemplates,
                                  const inja::Template& fileTemplate,
                                  inja::Environment& env,
                                  const inja::json& fileData,
                                  JsonTypeStorage& storage)
  {
    std::vector<inja::json> types;
    std::string visitorResult;
    for (const auto& type: storage.getTypeSet().types)
    {
      visitorResult.append(TemplateVisitor::generate(*type, env, storage, typeTemplates));

      if (type->isCustomType())
      {
        types.push_back(storage.get(*type->asCustomType()));
      }
    }

    inja::json data = fileData;
    data["visitorResult"] = visitorResult;

    // file
    {
      inja::json file;
      file["id"] = computeFileId(storage.getTypeSet().fileName);
      file["name"] = storage.getTypeSet().fileName;

      data["file"] = file;
    }
    data["types"] = types;

    return env.render(fileTemplate, data);
  }

  static std::string generateBaseHeaderFile(const TemplateSet& declTemplates,
                                            const inja::Template& fileTemplate,
                                            inja::Environment& env,
                                            const inja::json& fileData,
                                            JsonTypeStorage& storage)
  {
    std::vector<inja::json> types;

    std::string declVisitorResult;
    std::string inlVisitorResult;

    for (const auto& type: storage.getTypeSet().types)
    {
      declVisitorResult.append(TemplateVisitor::generate(*type, env, storage, declTemplates));

      if (type->isCustomType())
      {
        types.push_back(storage.get(*type->asCustomType()));
      }
    }

    inja::json data = fileData;
    data["declVisitorResult"] = declVisitorResult;

    // file
    {
      inja::json file;
      file["id"] = computeFileId(storage.getTypeSet().fileName);
      file["name"] = storage.getTypeSet().fileName;

      data["file"] = file;
    }
    data["types"] = types;

    return env.render(fileTemplate, data);
  }

private:
  inja::Environment env_;
};

//--------------------------------------------------------------------------------------------------------------
// CppGenerator
//--------------------------------------------------------------------------------------------------------------

CppGenerator::CppGenerator(): pimpl_(std::make_unique<Impl>()) {}

CppGenerator::~CppGenerator() = default;

void CppGenerator::writeFiles(const sen::lang::TypeSet& typeSet,
                              bool recursive,
                              bool publicSymbols,
                              const std::string& basePath)
{
  pimpl_->writeFiles(typeSet, recursive, publicSymbols, basePath);
}

void CppGenerator::writeFilesImpl(std::vector<std::filesystem::path>& writtenFiles,
                                  const sen::lang::TypeSet& typeSet,
                                  bool recursive,
                                  bool publicSymbols,
                                  const std::string& basePath)
{
  pimpl_->writeFilesImpl(writtenFiles, typeSet, recursive, publicSymbols, basePath);
}

void CppGenerator::generatePackageExportsFile(const std::string& packageName,
                                              const std::vector<std::string>& stlFiles,
                                              const std::vector<std::string>& packageDependencies,
                                              const std::vector<std::string>& userImplementedClasses)
{
  inja::json fileData;
  fileData["packageId"] = intToHex2(sen::crc32(packageName));
  fileData["packageName"] = packageName;

  // stlFilesIds
  {
    std::vector<inja::json> genFiles;
    for (const auto& file: stlFiles)
    {
      genFiles.emplace_back();
      genFiles.back()["id"] = computeFileId(file);
      genFiles.back()["name"] = file;
    }
    fileData["genFiles"] = genFiles;
  }

  // packageIds
  {
    std::vector<inja::json> packagesData;
    for (const auto& package: packageDependencies)
    {
      inja::json packageData;
      packageData["id"] = intToHex2(sen::crc32(package));
      packageData["name"] = package;
      packagesData.push_back(packageData);
    }
    fileData["packageIds"] = packagesData;
    fileData["packageDependencies"] = packageDependencies;
  }

  // implTypes
  {
    std::vector<inja::json> implTypes;

    for (const auto& qualName: userImplementedClasses)
    {
      auto namespacePath = tokenize(qualName, '.');
      const auto className = namespacePath.size() > 1U ? namespacePath.back() : qualName;

      std::string cppNamespace;
      for (std::size_t i = 0U; i < namespacePath.size() - 1U; ++i)
      {
        cppNamespace.append(namespacePath[i]);
        if (i != namespacePath.size() - 2U)
        {
          cppNamespace.append("::");
        }
      }

      inja::json typeData;
      typeData["senQual"] = qualName;
      typeData["name"] = className;
      typeData["namespace"] = cppNamespace;
      typeData["typeGetterFunc"] = sen::ClassType::computeTypeGetterFuncName(qualName);
      typeData["instanceMakerFunc"] = sen::ClassType::computeInstanceMakerFuncName(qualName);

      // save the type data
      implTypes.push_back(typeData);
    }

    fileData["implTypes"] = implTypes;
  }

  std::string fileName = "sen_exported_types.cpp";

  std::vector<std::filesystem::path> writtenFiles;

  inja::Environment env;
  configureEnv(env);
  writeFile(fileName, writtenFiles, [&]() { return env.render(getPackageExportTemplate(env), fileData); });

  std::cout << "stl|cpp> " << fileName << std::endl;
}
