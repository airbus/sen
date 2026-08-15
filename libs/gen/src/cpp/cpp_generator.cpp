// === cpp_generator.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/gen/cpp.h"

// lib
#include "common/type_storage.h"
#include "common/util.h"
#include "cpp/cpp_templates.h"

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
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace sen::gen
{

namespace
{

std::filesystem::path typesSourceFileFromSTLFile(const std::filesystem::path& file)
{
  return file.filename().concat(".h");
}

std::filesystem::path implSourceFileFromSTLFile(const std::filesystem::path& file)
{
  return file.filename().concat(".cpp");
}

std::string computeFileId(const std::string& fileName)
{
  std::filesystem::path filePath(fileName);

  std::string stringToUse = filePath.stem().string();
  if (filePath.has_parent_path())
  {
    stringToUse.append(filePath.parent_path().stem().string());
  }

  return detail::intToHex2(sen::crc32(stringToUse));
}

class TemplateVisitor: protected sen::TypeVisitor
{
public:
  static std::string generate(const sen::CustomType& type,
                              inja::Environment& env,
                              detail::TypeStorage& typeStorage,
                              const detail::CppTemplateSet& templates)
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
                  const detail::CppTemplateSet& templates) noexcept
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
  const detail::CppTemplateSet& templates_;
  std::string result_;
};

}  // namespace

class CppGenerator::Impl
{
  SEN_NOCOPY_NOMOVE(Impl)

public:
  Impl()
  {
    detail::configureEnv(env_);
    baseHeaderTemplates_ = detail::makeCppBaseHeaderTemplates(env_);
    baseHeaderFileTemplate_ = detail::makeCppBaseHeaderFileTemplate(env_);
    implTemplates_ = detail::makeCppImplTemplates(env_);
    implFileTemplate_ = detail::makeCppImplFileTemplate(env_);
    packageExportTemplate_ = detail::makeCppPackageExportTemplate(env_);
  }

  ~Impl() = default;

public:
  void generateInto(FileContents& result,
                    const sen::lang::TypeSet& typeSet,
                    const CppOptions& opts,
                    std::set<const sen::lang::TypeSet*>& visited)
  {
    if (!visited.insert(&typeSet).second)
    {
      return;
    }
    const auto& typesFile = typeSet.fileName;

    auto typesFileName = typesSourceFileFromSTLFile(typesFile);
    auto implFileName = implSourceFileFromSTLFile(typesFile);

    inja::json fileData;
    fileData["namespace"] = detail::computeCppNamespace(typeSet);
    fileData["basePath"] = opts.basePath;
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

    detail::TypeStorage storage(typeSet, opts.publicSymbols);

    const auto headerPath = typeSet.parentDirectory / typesFileName;
    if (result.find(headerPath) == result.end())
    {
      result[headerPath] =
        generateBaseHeaderFile(baseHeaderTemplates_, baseHeaderFileTemplate_, env_, fileData, storage);
    }

    const auto implPath = typeSet.parentDirectory / implFileName;
    if (result.find(implPath) == result.end())
    {
      result[implPath] = generateFile(implTemplates_, implFileTemplate_, env_, fileData, storage);
    }

    if (opts.recursive && !typeSet.importedSets.empty())
    {
      for (const auto& importedSet: typeSet.importedSets)
      {
        generateInto(result, *importedSet, opts, visited);
      }
    }
  }

  std::string generateExports(const CppExportsOptions& opts)
  {
    inja::json fileData;
    fileData["packageId"] = detail::intToHex2(sen::crc32(opts.packageName));
    fileData["packageName"] = opts.packageName;

    // stlFilesIds
    {
      std::vector<inja::json> genFiles;
      for (const auto& file: opts.stlFiles)
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
      for (const auto& package: opts.packageDependencies)
      {
        inja::json packageData;
        packageData["id"] = detail::intToHex2(sen::crc32(package));
        packageData["name"] = package;
        packagesData.push_back(packageData);
      }
      fileData["packageIds"] = packagesData;
      fileData["packageDependencies"] = opts.packageDependencies;
    }

    // implTypes
    {
      std::vector<inja::json> implTypes;

      for (const auto& qualName: opts.userImplementedClasses)
      {
        auto namespacePath = detail::tokenize(qualName, '.');
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

    return env_.render(packageExportTemplate_, fileData);
  }

private:
  static std::string generateFile(const detail::CppTemplateSet& typeTemplates,
                                  const inja::Template& fileTemplate,
                                  inja::Environment& env,
                                  const inja::json& fileData,
                                  detail::TypeStorage& storage)
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

  static std::string generateBaseHeaderFile(const detail::CppTemplateSet& declTemplates,
                                            const inja::Template& fileTemplate,
                                            inja::Environment& env,
                                            const inja::json& fileData,
                                            detail::TypeStorage& storage)
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
  detail::CppTemplateSet baseHeaderTemplates_;
  inja::Template baseHeaderFileTemplate_;
  detail::CppTemplateSet implTemplates_;
  inja::Template implFileTemplate_;
  inja::Template packageExportTemplate_;
};

//--------------------------------------------------------------------------------------------------------------
// CppGenerator
//--------------------------------------------------------------------------------------------------------------

CppGenerator::CppGenerator(): pimpl_(std::make_unique<Impl>()) {}

CppGenerator::~CppGenerator() = default;

CppGenerator::FileContents CppGenerator::generate(const sen::lang::TypeSet& typeSet, const CppOptions& opts)
{
  FileContents result;
  std::set<const sen::lang::TypeSet*> visited;
  pimpl_->generateInto(result, typeSet, opts, visited);
  return result;
}

std::string CppGenerator::generateExports(const CppExportsOptions& opts) { return pimpl_->generateExports(opts); }

}  // namespace sen::gen
