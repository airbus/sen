// === cpp_generator.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "cpp/cpp_generator.h"

// app
#include "common/json_type_storage.h"
#include "common/util.h"
#include "cpp/cpp_templates.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/hash32.h"
#include "sen/core/io/util.h"
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

void configureExportsCommand(CLI::App* cmd)
{
  auto args = std::make_shared<Args>();
  cmd->add_option("-p, --package", args->packageName, "Name of the package to create the exports for");
  cmd->add_option("-f, --file", args->stlFiles, "Name of the STL file containing types");
  cmd->add_option("-d, --depends", args->dependentPackages, "Names of the packages we depend on");
  cmd->add_option("-i, --implements", args->userTypes, "Names of types implemented in this package");
  cmd->callback(
    [args]()
    {
      CppGenerator::generatePackageExportsFile(
        args->packageName, args->stlFiles, args->dependentPackages, args->userTypes);
    });
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

void CppGenerator::setup(CLI::App& app)
{
  auto cppExtraArgs = std::make_shared<CppExtraArgs>();

  auto cpp = app.add_subcommand("cpp", "Generate C++ code");
  cpp->allow_extras();
  cpp->require_subcommand();

  cpp->add_option("-r, --recursive", cppExtraArgs->recursive, "Recursively generate imported packages");
  cpp->add_flag("--public-symbols", cppExtraArgs->publicSymbols, "Make generated classes public");

  auto stl = setupStlInput(*cpp,
                           [cppExtraArgs](auto args)
                           {
                             CppGenerator generator;
                             const auto settings = readTypeSettings(args->codegenOptionsFile);

                             sen::lang::TypeSetContext context;
                             for (const auto& fileName: args->inputs)
                             {
                               auto typeSet = sen::lang::readTypesFile(fileName, args->includePaths, context, settings);
                               generator.writeFiles(
                                 *typeSet, cppExtraArgs->recursive, cppExtraArgs->publicSymbols, args->basePath);
                             }
                           });

  auto fom = setupFomInput(*cpp,
                           [cppExtraArgs](auto args)
                           {
                             CppGenerator generator;
                             const auto settings = readTypeSettings(args->codegenOptionsFile);
                             const auto typeSets =
                               sen::lang::parseFomDocuments(args->paths, args->mappingFiles, settings);

                             for (const auto& set: typeSets)
                             {
                               generator.writeFiles(set, cppExtraArgs->recursive, cppExtraArgs->publicSymbols, "");
                             }
                           });

  // Canonical location for the exports command is under 'cpp', next to 'stl' and 'fom'.
  auto exports = cpp->add_subcommand("exports", "Generate the export symbols file for a sen STL package");
  configureExportsCommand(exports);

  stl->excludes(fom);
}

void CppGenerator::exportsSetup(CLI::App& app)
{
  // Hidden backward-compat alias for 'generate cpp exports'. Kept so existing CMake tooling
  // that invokes 'cli_gen exp_package ...' continues to work. The empty group hides it from
  // --help.
  auto exp = app.add_subcommand("exp_package", "Alias for 'generate cpp exports'");
  exp->group("");
  configureExportsCommand(exp);
}
