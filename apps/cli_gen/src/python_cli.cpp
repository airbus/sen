// === python_cli.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// app
#include "cli_input.h"
#include "io.h"

// lib
#include "sen/gen/python.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/lang/fom_parser.h"
#include "sen/core/lang/stl_resolver.h"

// cli11
#include <CLI/App.hpp>
// NOLINTNEXTLINE (misc-include-cleaner): cli11 needs all headers to correctly link required vtables
#include <CLI/CLI.hpp>

// std
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{

void writeModule(sen::gen::PythonGenerator& generator,
                 const sen::lang::TypeSet& typeSet,
                 const std::filesystem::path& inputFile)
{
  auto outputFile = inputFile.stem().string();
  sen::gen::PythonGenerator::replaceInvalidCharacters(outputFile);
  outputFile.append(".py");

  const std::string contents = generator.generateModule(typeSet);

  writeFile(outputFile, contents);
  std::cout << "stl|python> " << outputFile << std::endl;
}

}  // namespace

namespace sen::cli_gen
{

void setupPythonCli(CLI::App& app)
{
  auto py = app.add_subcommand("py", "Generate Python dataclasses");
  py->allow_extras();
  py->require_subcommand();

  auto stl = setupStlInput(*py,
                           [](auto args)
                           {
                             // One shared context across all inputs so common imports parse once.
                             sen::lang::TypeSetContext context;
                             std::vector<const sen::lang::TypeSet*> docs;
                             docs.reserve(args->inputs.size());
                             for (const auto& fileName: args->inputs)
                             {
                               const auto* newDoc = sen::lang::readTypesFile(fileName, args->includePaths, context, {});
                               SEN_ASSERT(newDoc && "There should always be a document returned.");
                               docs.push_back(newDoc);
                             }
                             sen::gen::PythonGenerator generator;
                             for (std::size_t i = 0; i < docs.size(); ++i)
                             {
                               writeModule(generator, *docs[i], args->inputs[i]);
                             }
                           });

  auto fom = setupFomInput(*py,
                           [](auto args)
                           {
                             const sen::lang::TypeSetContext typeSets =
                               sen::lang::parseFomDocuments(args->paths, args->mappingFiles, {});

                             sen::gen::PythonGenerator generator;
                             for (const auto& typeSet: typeSets)
                             {
                               writeModule(generator, typeSet, std::filesystem::path(typeSet.fileName));
                             }
                           });
  stl->excludes(fom);
}

}  // namespace sen::cli_gen
