// === mkdocs_cli.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// app
#include "cli_input.h"
#include "io.h"

// lib
#include "sen/gen/mkdocs.h"

// sen
#include "sen/core/lang/fom_parser.h"
#include "sen/core/lang/stl_resolver.h"

// cli11
#include <CLI/App.hpp>
// NOLINTNEXTLINE (misc-include-cleaner): cli11 needs all headers to correctly link required vtables
#include <CLI/CLI.hpp>

// std
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <tuple>

namespace
{

struct MkDocsArgs
{
  std::filesystem::path outputFile;
  std::string title = "Package Documentation";
};

void setupMkDocsArgs(CLI::App& app, MkDocsArgs& args)
{
  app.add_option("-o, --output", args.outputFile, "Output file");
  app.add_option("-t, --title", args.title, "Document title");
}

void writeOutput(const sen::lang::TypeSetContext& typeSets, const MkDocsArgs& args)
{
  sen::gen::MkDocsGenerator generator;
  const std::string contents = generator.generate(typeSets, args.title);

  writeFile(args.outputFile, contents);
  std::cout << "stl|mkdocs> " << args.outputFile << std::endl;
}

}  // namespace

namespace sen::cli_gen
{

void setupMkDocsCli(CLI::App& app)
{
  auto mkdocsArgs = std::make_shared<MkDocsArgs>();
  auto mkdocs = app.add_subcommand("mkdocs", "Generate MkDocs documentation");
  mkdocs->require_subcommand();

  auto stl = setupStlInput(*mkdocs,
                           [mkdocsArgs](auto args)
                           {
                             sen::lang::TypeSetContext typeSets;
                             for (const auto& fileName: args->inputs)
                             {
                               std::ignore = sen::lang::readTypesFile(fileName, args->includePaths, typeSets, {});
                             }
                             writeOutput(typeSets, *mkdocsArgs);
                           });

  setupMkDocsArgs(*stl, *mkdocsArgs);

  auto fom = setupFomInput(*mkdocs,
                           [mkdocsArgs](auto args)
                           {
                             const sen::lang::TypeSetContext typeSets =
                               sen::lang::parseFomDocuments(args->paths, args->mappingFiles, {});
                             writeOutput(typeSets, *mkdocsArgs);
                           });

  setupMkDocsArgs(*fom, *mkdocsArgs);
  stl->excludes(fom);
}

}  // namespace sen::cli_gen
