// === typescript_cli.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// app
#include "cli_input.h"
#include "io.h"

// lib
#include "sen/gen/typescript.h"

// sen
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

void writeOutputs(const std::filesystem::path& outDir, const sen::gen::TypeScriptGenerator::FileContents& contents)
{
  std::filesystem::create_directories(outDir);

  for (const auto& [relPath, body]: contents)
  {
    const auto outPath = outDir / relPath;
    writeFile(outPath, body);
    std::cout << "stl|ts> " << outPath.string() << std::endl;
  }
}

}  // namespace

namespace sen::cli_gen
{

// TS generates from STL only; no FOM subcommand today.
void setupTypeScriptCli(CLI::App& app)
{
  auto ts = app.add_subcommand("ts", "Generate per-input TypeScript modules + a barrel from STL inputs");
  ts->require_subcommand();

  auto outDir = std::make_shared<std::string>();

  auto stl = setupStlInput(*ts,
                           [outDir](auto args)
                           {
                             sen::lang::TypeSetContext typeSets;
                             for (const auto& fileName: args->inputs)
                             {
                               std::ignore = sen::lang::readTypesFile(fileName, args->includePaths, typeSets, {});
                             }
                             sen::gen::TypeScriptGenerator generator;
                             writeOutputs(*outDir, generator.generate(typeSets));
                           });
  stl->add_option("-d, --out-dir", *outDir, "Output directory for the generated .ts files")->required();
}

}  // namespace sen::cli_gen
