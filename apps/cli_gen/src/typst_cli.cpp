// === typst_cli.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// app
#include "cli_input.h"
#include "io.h"

// lib
#include "sen/gen/typst.h"

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

struct TypstArgs
{
  std::filesystem::path outputDir;
  sen::gen::TypstOptions options;
};

// What goes in the document is the caller's decision, never something inferred from the
// shape of the model: two runs over the same model must be able to produce two documents.
void setupTypstArgs(CLI::App& app, TypstArgs& args)
{
  auto& options = args.options;
  app.add_option("-o, --output", args.outputDir, "Directory to write the document into")->required();
  app.add_option("-t, --title", options.title, "Name of the model, shown on the running header");

  app.add_option("--include-package", options.includePackages, "Only document this package; repeatable")
    ->allow_extra_args(false);
  app.add_option("--exclude-package", options.excludePackages, "Leave this package out; repeatable")
    ->allow_extra_args(false);

  app.add_flag("!--no-overview", options.overview, "Drop the model overview");
  app.add_flag("!--no-hierarchy", options.hierarchy, "Drop the class hierarchy");
  app.add_flag("!--no-summaries", options.summaries, "Drop the per-package summary lists");
  app.add_flag("!--no-index", options.index, "Drop the index of every type");
  app.add_flag("!--no-used-by", options.usedBy, "Drop the list of what refers to each type");
  app.add_flag("!--no-flag-legend", options.flagLegend, "Drop the explanation of the property flags");
  app.add_flag("!--no-built-ins", options.builtIns, "Drop the list of the language's built-in types");

  app.add_option("--front-matter", options.frontMatter, "Typst file to include before the reference, as a title page")
    ->check(CLI::ExistingFile);
  app.add_option("--before-reference", options.beforeReference, "Typst file to include after the front matter")
    ->check(CLI::ExistingFile);
  app.add_option("--after-reference", options.afterReference, "Typst file to include after the reference")
    ->check(CLI::ExistingFile);
  app.add_option("--style", options.style, "Typst file defining the look, replacing the one shipped")
    ->check(CLI::ExistingFile);
}

// Typst resolves an include against the directory holding the document and refuses a path
// that climbs out of it, so a page can only be named by a bare filename. Copying the
// caller's pages in beside the document is what lets them keep theirs wherever they like.
void writeOutput(const sen::lang::TypeSetContext& typeSets, const TypstArgs& args)
{
  auto options = args.options;
  std::vector<std::filesystem::path> pages;
  for (auto* point: {&options.frontMatter, &options.beforeReference, &options.afterReference, &options.style})
  {
    if (!point->empty())
    {
      pages.push_back(*point);
      *point = point->filename();
    }
  }

  sen::gen::TypstGenerator generator;
  const auto files = generator.generate(typeSets, options);

  for (const auto& [relPath, body]: files)
  {
    writeFile(args.outputDir / relPath, body);
  }

  std::filesystem::create_directories(args.outputDir);
  for (const auto& page: pages)
  {
    std::filesystem::copy_file(
      page, args.outputDir / page.filename(), std::filesystem::copy_options::overwrite_existing);
  }

  std::cout << "stl|typst> " << files.size() << " files in " << args.outputDir << std::endl;
  std::cout << "           compile with: typst compile " << (args.outputDir / "document.typ").string() << std::endl;
}

}  // namespace

namespace sen::cli_gen
{

void setupTypstCli(CLI::App& app)
{
  auto typstArgs = std::make_shared<TypstArgs>();
  auto typst = app.add_subcommand("typst", "Generate a typesettable reference for the data model");
  typst->require_subcommand();

  auto stl = setupStlInput(*typst,
                           [typstArgs](auto args)
                           {
                             sen::lang::TypeSetContext typeSets;
                             for (const auto& fileName: args->inputs)
                             {
                               std::ignore = sen::lang::readTypesFile(fileName, args->includePaths, typeSets, {});
                             }
                             writeOutput(typeSets, *typstArgs);
                           });

  setupTypstArgs(*stl, *typstArgs);

  auto fom = setupFomInput(*typst,
                           [typstArgs](auto args)
                           {
                             const sen::lang::TypeSetContext typeSets =
                               sen::lang::parseFomDocuments(args->paths, args->mappingFiles, {});
                             writeOutput(typeSets, *typstArgs);
                           });

  setupTypstArgs(*fom, *typstArgs);
  stl->excludes(fom);
}

}  // namespace sen::cli_gen
