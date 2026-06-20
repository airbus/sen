// === json_cli.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// app
#include "cli_input.h"
#include "io.h"

// lib
#include "sen/gen/json.h"

// sen
#include "sen/core/lang/fom_parser.h"
#include "sen/core/lang/stl_resolver.h"

// cli11
#include <CLI/App.hpp>
// NOLINTNEXTLINE (misc-include-cleaner): cli11 needs all headers to correctly link required vtables
#include <CLI/CLI.hpp>
#include <CLI/Validators.hpp>

// std
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

namespace sen::cli_gen
{

namespace
{

struct JsonPackageArgs
{
  std::string outFile;
  std::vector<std::string> classes;
};

struct JsonComponentArgs
{
  std::string outFile;
  std::string componentName;
};

struct JsonSchemaArgs
{
  std::string outFile;
  std::vector<std::filesystem::path> schemas;
};

void setupJsonPackageArgs(CLI::App& app, JsonPackageArgs& args)
{
  app.add_option("-o, --output", args.outFile, "Output file");
  app.add_option("-c, --class", args.classes, "Classes implemented by the user");
}

void setupJsonComponentArgs(CLI::App& app, JsonComponentArgs& args)
{
  app.add_option("-o, --output", args.outFile, "Output file");
  app.add_option("-n, --name", args.componentName, "Name of the component");
}

void writePackageOutput(const sen::lang::TypeSetContext& typeSets, const JsonPackageArgs& args)
{
  sen::gen::JsonGenerator generator;
  sen::gen::PackageOptions opts;
  opts.schemaName = std::filesystem::path(args.outFile).stem().string();
  opts.classes = args.classes;

  const std::string contents = generator.generatePackage(typeSets, opts);

  writeFile(args.outFile, contents);
  std::cout << "stl|json> " << args.outFile << std::endl;
}

void writeComponentOutput(const sen::lang::TypeSetContext& typeSets, const JsonComponentArgs& args)
{
  sen::gen::JsonGenerator generator;
  sen::gen::ComponentOptions opts;
  opts.schemaName = std::filesystem::path(args.outFile).stem().string();
  opts.componentName = args.componentName;

  const std::string contents = generator.generateComponent(typeSets, opts);

  writeFile(args.outFile, contents);
  std::cout << "stl|json> " << args.outFile << std::endl;
}

void setupGenJsonPackage(CLI::App& app)
{
  auto pkgArgs = std::make_shared<JsonPackageArgs>();
  auto pkg = app.add_subcommand("package", "Generate JSON schemas for packages");
  pkg->allow_extras();
  pkg->require_subcommand();

  auto stl = setupStlInput(*pkg,
                           [pkgArgs](auto args)
                           {
                             sen::lang::TypeSetContext typeSets;
                             for (const auto& fileName: args->inputs)
                             {
                               std::ignore = sen::lang::readTypesFile(fileName, args->includePaths, typeSets, {});
                             }
                             writePackageOutput(typeSets, *pkgArgs);
                           });

  setupJsonPackageArgs(*stl, *pkgArgs);

  auto fom = setupFomInput(*pkg,
                           [pkgArgs](auto args)
                           {
                             const sen::lang::TypeSetContext typeSets =
                               sen::lang::parseFomDocuments(args->paths, args->mappingFiles, {});
                             writePackageOutput(typeSets, *pkgArgs);
                           });

  setupJsonPackageArgs(*fom, *pkgArgs);
  stl->excludes(fom);
}

void setupGenJsonComponent(CLI::App& app)
{
  auto compArgs = std::make_shared<JsonComponentArgs>();
  auto comp = app.add_subcommand("component", "Generate JSON schemas for components");
  comp->allow_extras();
  comp->require_subcommand();

  auto stl = setupStlInput(*comp,
                           [compArgs](auto args)
                           {
                             sen::lang::TypeSetContext typeSets;
                             for (const auto& fileName: args->inputs)
                             {
                               std::ignore = sen::lang::readTypesFile(fileName, args->includePaths, typeSets, {});
                             }
                             writeComponentOutput(typeSets, *compArgs);
                           });

  setupJsonComponentArgs(*stl, *compArgs);

  auto fom = setupFomInput(*comp,
                           [compArgs](auto args)
                           {
                             const sen::lang::TypeSetContext typeSets =
                               sen::lang::parseFomDocuments(args->paths, args->mappingFiles, {});
                             writeComponentOutput(typeSets, *compArgs);
                           });

  setupJsonComponentArgs(*fom, *compArgs);
  stl->excludes(fom);
}

void setupGenJsonSchema(CLI::App& app)
{
  auto schemaArgs = std::make_shared<JsonSchemaArgs>();
  auto schema = app.add_subcommand("schema", "Generate JSON schemas for sen configurations");
  schema->allow_extras();

  schema->add_option("schema_files", schemaArgs->schemas, "JSON schema files")->required()->check(CLI::ExistingFile);

  schema->add_option("-o, --output", schemaArgs->outFile, "Output file");

  schema->callback(
    [schemaArgs]()
    {
      std::vector<std::string> contents;
      contents.reserve(schemaArgs->schemas.size());
      for (const auto& path: schemaArgs->schemas)
      {
        std::string fileContent;
        readFile(path, fileContent);
        contents.push_back(std::move(fileContent));
      }

      sen::gen::JsonGenerator generator;
      const std::string schemaName = std::filesystem::path(schemaArgs->outFile).stem().string();
      const std::string combined = generator.combineSchemas(contents, schemaName);

      writeFile(schemaArgs->outFile, combined);
    });
}

}  // namespace

void setupJsonCli(CLI::App& app)
{
  auto json = app.add_subcommand("json", "Generate JSON schemas");
  json->allow_extras();
  json->require_subcommand();

  setupGenJsonPackage(*json);
  setupGenJsonComponent(*json);
  setupGenJsonSchema(*json);
}

}  // namespace sen::cli_gen
