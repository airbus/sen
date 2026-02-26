// === main.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// app
#include "cpp/cpp_generator.h"
#include "cpp/util.h"
#include "json/json_generator.h"
#include "mkdocs/mkdocs_generator.h"
#include "plantuml/plantuml_generator.h"
#include "python/python_generator.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/lang/fom_parser.h"
#include "sen/core/lang/stl_resolver.h"

// cli11
#include <CLI/App.hpp>
#include <CLI/CLI.hpp>  // NOLINT (misc-include-cleaner): to correctly link
#include <CLI/Validators.hpp>

// std
#include <cstdio>
#include <exception>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

struct StlArgs
{
  std::vector<std::filesystem::path> inputs;
  std::vector<std::filesystem::path> includePaths;
  std::string basePath;
  std::string codegenOptionsFile;
};

struct FomArgs
{
  std::vector<std::filesystem::path> paths;
  std::vector<std::filesystem::path> mappingFiles;
  std::string codegenOptionsFile;
};

CLI::App* setupStlInput(CLI::App& app, std::function<void(std::shared_ptr<StlArgs>)>&& action)
{
  auto args = std::make_shared<StlArgs>();

  auto stl = app.add_subcommand("stl", "process STL files");
  stl->add_option("stl_files", args->inputs, "STL files")->required()->check(CLI::ExistingFile);
  stl->add_option("-i, --import", args->includePaths, "Paths where other STL files can be found");
  stl->add_option("-b, --base_path", args->basePath, "base path for including generated files");
  stl->add_option("-s, --settings", args->codegenOptionsFile, "code generation settings file")
    ->check(CLI::ExistingFile);
  stl->callback([args, act = std::move(action)]() { act(args); });
  return stl;
}

CLI::App* setupFomInput(CLI::App& app, std::function<void(std::shared_ptr<FomArgs>)>&& action)
{
  auto args = std::make_shared<FomArgs>();

  auto fom = app.add_subcommand("fom", "process HLA FOM files");

  fom->add_option("-m, --mappings", args->mappingFiles, "XML defining custom mappings between sen and hla")
    ->delimiter(',')
    ->check(CLI::ExistingFile);

  fom->add_option("-d, --directories", args->paths, "directories containing FOM XML files")
    ->check(CLI::ExistingDirectory)
    ->required();

  fom->add_option("-s, --settings", args->codegenOptionsFile, "code generation settings file")
    ->check(CLI::ExistingFile);

  fom->callback([args, act = std::move(action)]() { act(args); });

  return fom;
}

//--------------------------------------------------------------------------------------------------------------
// C++
//--------------------------------------------------------------------------------------------------------------

void setupGenCpp(CLI::App& app)
{
  struct CppExtraArgs
  {
    bool recursive = false;
    bool publicSymbols = false;
  };

  auto cppExtraArgs = std::make_shared<CppExtraArgs>();

  auto cpp = app.add_subcommand("cpp", "Generates C++ code");
  cpp->allow_extras();
  cpp->require_subcommand();

  cpp->add_option("-r, --recursive", cppExtraArgs->recursive, "recursively generate imported packages");
  cpp->add_flag("--public-symbols", cppExtraArgs->publicSymbols, "make generated classes public");

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

  stl->excludes(fom);
}

//--------------------------------------------------------------------------------------------------------------
// UML
//--------------------------------------------------------------------------------------------------------------

struct UmlArgs
{
  std::string outputFile = "output.plantuml";
  bool onlyClasses = false;
  bool onlyTypes = false;
  bool noEnumerators = false;
};

void setupUmlArgs(CLI::App& app, const std::shared_ptr<UmlArgs>& args)
{
  app.add_option("-o, --output", args->outputFile, "output plantuml file");
  auto onlyClasses = app.add_flag("--only-classes", args->onlyClasses, "only generate class diagrams");
  auto onlyTypes = app.add_flag("--only-types", args->onlyTypes, "only generate diagrams for basic types");
  auto noEnumerators = app.add_flag("--no-enumerators", args->noEnumerators, "do not generate enumerations");

  onlyClasses->excludes(onlyTypes);
  onlyClasses->excludes(noEnumerators);
}

UMLGenerationMode getUmlGenerationMode(const std::shared_ptr<UmlArgs>& args)
{
  if (args->onlyClasses)
  {
    return UMLGenerationMode::onlyClasses;
  }
  if (args->onlyTypes)
  {
    return UMLGenerationMode::onlyBasicTypes;
  }
  return UMLGenerationMode::all;
}

UMLEnumMode getUmlEnumMode(const std::shared_ptr<UmlArgs>& args)
{
  return args->noEnumerators ? UMLEnumMode::noEnumerators : UMLEnumMode::all;
}

void setupGenUml(CLI::App& app)
{
  auto umlArgs = std::make_shared<UmlArgs>();
  auto uml = app.add_subcommand("uml", "generates UML diagrams");
  uml->allow_extras();
  uml->require_subcommand();

  auto stl = setupStlInput(*uml,
                           [umlArgs](auto args)
                           {
                             sen::lang::TypeSetContext typeSets;
                             for (const auto& fileName: args->inputs)
                             {
                               std::ignore = sen::lang::readTypesFile(fileName, args->includePaths, typeSets, {});
                             }

                             PlantUMLGenerator(typeSets).write(
                               umlArgs->outputFile, getUmlGenerationMode(umlArgs), getUmlEnumMode(umlArgs));
                           });

  setupUmlArgs(*stl, umlArgs);

  auto fom = setupFomInput(
    *uml,
    [umlArgs](auto args)
    {
      const sen::lang::TypeSetContext typeSets = sen::lang::parseFomDocuments(args->paths, args->mappingFiles, {});

      PlantUMLGenerator(typeSets).write(
        std::filesystem::path(umlArgs->outputFile), getUmlGenerationMode(umlArgs), getUmlEnumMode(umlArgs));
    });

  setupUmlArgs(*fom, umlArgs);
  stl->excludes(fom);
}

//--------------------------------------------------------------------------------------------------------------
// MKDocs
//--------------------------------------------------------------------------------------------------------------

struct MKDocsArgs
{
  std::filesystem::path outputFile;
  std::string title = "Package Documentation";
};

void setupMKDocsArgs(CLI::App& app, const std::shared_ptr<MKDocsArgs>& args)
{
  app.add_option("-o, --output", args->outputFile, "output file");
  app.add_option("-t, --title", args->title, "document title");
}

void setupGenMKDocs(CLI::App& app)
{
  auto mkdocsArgs = std::make_shared<MKDocsArgs>();
  auto mkdocs = app.add_subcommand("mkdocs", "generates MKDocs documentation");
  mkdocs->allow_extras();
  mkdocs->require_subcommand();

  auto stl = setupStlInput(*mkdocs,
                           [mkdocsArgs](auto args)
                           {
                             sen::lang::TypeSetContext typeSets;
                             for (const auto& fileName: args->inputs)
                             {
                               std::ignore = sen::lang::readTypesFile(fileName, args->includePaths, typeSets, {});
                             }

                             MkDocsGenerator(typeSets).write(mkdocsArgs->outputFile, mkdocsArgs->title);
                           });

  setupMKDocsArgs(*stl, mkdocsArgs);

  auto fom = setupFomInput(
    *mkdocs,
    [mkdocsArgs](auto args)
    {
      const sen::lang::TypeSetContext typeSets = sen::lang::parseFomDocuments(args->paths, args->mappingFiles, {});

      MkDocsGenerator(typeSets).write(std::filesystem::path(mkdocsArgs->outputFile), mkdocsArgs->title);
    });

  setupMKDocsArgs(*fom, mkdocsArgs);
  stl->excludes(fom);
}

//--------------------------------------------------------------------------------------------------------------
// Python
//--------------------------------------------------------------------------------------------------------------

struct PyArgs
{
};

void setupPyArgs(CLI::App& app, const std::shared_ptr<PyArgs>& args)
{
  std::ignore = app;
  std::ignore = args;
}

void setupGenPy(CLI::App& app)
{
  auto pyArgs = std::make_shared<PyArgs>();
  auto py = app.add_subcommand("py", "generates Python dataclasses");
  py->allow_extras();
  py->require_subcommand();

  auto stl = setupStlInput(*py,
                           [](auto args)
                           {
                             sen::lang::TypeSetContext context;
                             for (const auto& fileName: args->inputs)
                             {
                               const auto* newDoc = sen::lang::readTypesFile(fileName, args->includePaths, context, {});
                               SEN_ASSERT(newDoc && "There should always be a document returned.");
                               PythonGenerator(*newDoc).write(fileName);
                             }
                           });

  setupPyArgs(*stl, pyArgs);

  auto fom = setupFomInput(*py,
                           [](auto args)
                           {
                             const sen::lang::TypeSetContext typeSets =
                               sen::lang::parseFomDocuments(args->paths, args->mappingFiles, {});

                             for (const auto& typeSet: typeSets)
                             {
                               PythonGenerator(typeSet).write(std::filesystem::path(typeSet.fileName));
                             }
                           });

  setupPyArgs(*fom, pyArgs);
  stl->excludes(fom);
}

//--------------------------------------------------------------------------------------------------------------
// Json - Package Mode
//--------------------------------------------------------------------------------------------------------------

struct JsonPackageArgs
{
  std::string outFile;
  std::vector<std::string> classes;
};

void setupJsonPackageArgs(CLI::App& app, const std::shared_ptr<JsonPackageArgs>& args)
{
  app.add_option("-o, --output", args->outFile, "output file");
  app.add_option("-c, --class", args->classes, "Classes implemented by the user");
}

void setupGenJsonPackage(CLI::App& app)
{
  auto jsonPackageArgs = std::make_shared<JsonPackageArgs>();
  auto jsonPackage = app.add_subcommand("package", "generates JSON schemas for packages");
  jsonPackage->allow_extras();
  jsonPackage->require_subcommand();

  auto stl = setupStlInput(*jsonPackage,
                           [jsonPackageArgs](auto args)
                           {
                             JsonGenerator generator;
                             sen::lang::TypeSetContext typeSets;

                             for (const auto& fileName: args->inputs)
                             {
                               std::ignore = sen::lang::readTypesFile(fileName, args->includePaths, typeSets, {});
                             }

                             generator.writePackageFiles(typeSets, jsonPackageArgs->outFile, jsonPackageArgs->classes);
                           });

  setupJsonPackageArgs(*stl, jsonPackageArgs);

  auto fom = setupFomInput(*jsonPackage,
                           [jsonPackageArgs](auto args)
                           {
                             const sen::lang::TypeSetContext typeSets =
                               sen::lang::parseFomDocuments(args->paths, args->mappingFiles, {});

                             JsonGenerator generator;
                             generator.writePackageFiles(typeSets, jsonPackageArgs->outFile, jsonPackageArgs->classes);
                           });

  setupJsonPackageArgs(*fom, jsonPackageArgs);
  stl->excludes(fom);
}

//--------------------------------------------------------------------------------------------------------------
// Json - Component Mode
//--------------------------------------------------------------------------------------------------------------

struct JsonComponentArgs
{
  std::string outFile;
  std::string componentName;
};

void setupJsonComponentArgs(CLI::App& app, const std::shared_ptr<JsonComponentArgs>& args)
{
  app.add_option("-o, --output", args->outFile, "output file");
  app.add_option("-n, --name", args->componentName, "name of the component");
}

void setupGenJsonComponent(CLI::App& app)
{
  auto jsonComponentArgs = std::make_shared<JsonComponentArgs>();
  auto jsonComponent = app.add_subcommand("component", "generates JSON schemas for components");
  jsonComponent->allow_extras();
  jsonComponent->require_subcommand();

  auto stl = setupStlInput(*jsonComponent,
                           [jsonComponentArgs](auto args)
                           {
                             sen::lang::TypeSetContext typeSets;

                             for (const auto& fileName: args->inputs)
                             {
                               std::ignore = sen::lang::readTypesFile(fileName, args->includePaths, typeSets, {});
                             }

                             JsonGenerator generator;
                             generator.writeComponentFiles(
                               typeSets, jsonComponentArgs->outFile, jsonComponentArgs->componentName);
                           });

  setupJsonComponentArgs(*stl, jsonComponentArgs);

  auto fom = setupFomInput(
    *jsonComponent,
    [jsonComponentArgs](auto args)
    {
      const sen::lang::TypeSetContext typeSets = sen::lang::parseFomDocuments(args->paths, args->mappingFiles, {});

      JsonGenerator generator;
      generator.writeComponentFiles(typeSets, jsonComponentArgs->outFile, jsonComponentArgs->componentName);
    });

  setupJsonComponentArgs(*fom, jsonComponentArgs);
  stl->excludes(fom);
}

//--------------------------------------------------------------------------------------------------------------
// Json - Schema Mode
//--------------------------------------------------------------------------------------------------------------

struct JsonSchemaArgs
{
  std::string outFile;
  std::vector<std::filesystem::path> schemas;
};

void setupGenJsonSchemaArgs(CLI::App& app)
{
  auto jsonSchemaArgs = std::make_shared<JsonSchemaArgs>();
  auto jsonSchema = app.add_subcommand("schema", "generates JSON schemas for sen configurations");
  jsonSchema->allow_extras();

  jsonSchema->add_option("schema_files", jsonSchemaArgs->schemas, "JSON schema files")
    ->required()
    ->check(CLI::ExistingFile);

  jsonSchema->add_option("-o, --output", jsonSchemaArgs->outFile, "output file");

  jsonSchema->callback(
    [jsonSchemaArgs]()
    {
      JsonGenerator generator;
      generator.writeCombination(jsonSchemaArgs->outFile, jsonSchemaArgs->schemas);
    });
}

//--------------------------------------------------------------------------------------------------------------
// Json
//--------------------------------------------------------------------------------------------------------------

void setupGenJson(CLI::App& app)
{
  auto json = app.add_subcommand("json", "generates JSON schemas");
  json->allow_extras();
  json->require_subcommand();

  setupGenJsonPackage(*json);
  setupGenJsonComponent(*json);
  setupGenJsonSchemaArgs(*json);
}

//--------------------------------------------------------------------------------------------------------------
// Exports
//--------------------------------------------------------------------------------------------------------------

void setupGenExp(CLI::App& app)
{
  struct Args
  {
    std::string packageName;
    std::vector<std::string> stlFiles;
    std::vector<std::string> dependentPackages;
    std::vector<std::string> userTypes;
  };

  auto args = std::make_shared<Args>();
  auto exp = app.add_subcommand("exp_package", "generates exports symbols for a sen STL package");
  exp->add_option("-p, --package", args->packageName, "name of the package to create the exports");
  exp->add_option("-f, --file", args->stlFiles, "name of the stl file containing types");
  exp->add_option("-d, --depends", args->dependentPackages, "name of the packages we depend on");
  exp->add_option("-i, --implements", args->userTypes, "name of types implemented in this package");
  exp->callback(
    [args]()
    {
      CppGenerator::generatePackageExportsFile(
        args->packageName, args->stlFiles, args->dependentPackages, args->userTypes);
    });
}

//--------------------------------------------------------------------------------------------------------------
// Main application logic
//--------------------------------------------------------------------------------------------------------------

int runApp(int argc, char* argv[])
{
  CLI::App app {"sen code generator\n"};
  app.name("sen gen");
  app.get_formatter()->column_width(35);  // NOLINT
  app.require_subcommand();

  bool expectFailure = false;
  app.add_flag("--expect-failure", expectFailure, "expects a failure in the generation process (for testing purposes)");

  try
  {
    setupGenCpp(app);
    setupGenUml(app);
    setupGenExp(app);
    setupGenPy(app);
    setupGenMKDocs(app);
    setupGenJson(app);

    app.footer("For help on specific commands run 'sen gen <command> --help'");

    CLI11_PARSE(app, argc, argv)

    if (expectFailure)
    {
      return 1;
    }
    return 0;
  }
  catch (const std::exception& /*e*/)
  {
    if (expectFailure)
    {
      return 0;
    }
    throw;
  }
}

int main(int argc, char* argv[])
{
  sen::registerTerminateHandler();
  try
  {
    return runApp(argc, argv);
  }
  catch (const std::exception& err)
  {
    fprintf(stderr, "Error detected: %s\n", err.what());  // NOLINT
    return 1;
  }
  catch (...)
  {
    std::fputs("Unknown error detected\n", stderr);
    return 1;
  }
}
