// === html_cli.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// app
#include "cli_input.h"
#include "io.h"

// lib
#include "sen/gen/html.h"

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

struct HtmlArgs
{
  std::filesystem::path outputDir;
  std::string title = "Data model";
};

void setupHtmlArgs(CLI::App& app, HtmlArgs& args)
{
  app.add_option("-o, --output", args.outputDir, "Directory to write the reference into")->required();
  app.add_option("-t, --title", args.title, "Name of the model, shown on screen");
}

void writeOutput(const sen::lang::TypeSetContext& typeSets, const HtmlArgs& args)
{
  sen::gen::HtmlGenerator generator;
  const auto files = generator.generate(typeSets, args.title);

  for (const auto& [relPath, body]: files)
  {
    writeFile(args.outputDir / relPath, body);
  }

  std::cout << "stl|html> " << files.size() << " files in " << args.outputDir << std::endl;
}

}  // namespace

namespace sen::cli_gen
{

void setupHtmlCli(CLI::App& app)
{
  auto htmlArgs = std::make_shared<HtmlArgs>();
  auto html = app.add_subcommand("html", "Generate a browsable reference for the data model");
  html->require_subcommand();

  auto stl = setupStlInput(*html,
                           [htmlArgs](auto args)
                           {
                             sen::lang::TypeSetContext typeSets;
                             for (const auto& fileName: args->inputs)
                             {
                               std::ignore = sen::lang::readTypesFile(fileName, args->includePaths, typeSets, {});
                             }
                             writeOutput(typeSets, *htmlArgs);
                           });

  setupHtmlArgs(*stl, *htmlArgs);

  auto fom = setupFomInput(*html,
                           [htmlArgs](auto args)
                           {
                             const sen::lang::TypeSetContext typeSets =
                               sen::lang::parseFomDocuments(args->paths, args->mappingFiles, {});
                             writeOutput(typeSets, *htmlArgs);
                           });

  setupHtmlArgs(*fom, *htmlArgs);
  stl->excludes(fom);
}

}  // namespace sen::cli_gen
