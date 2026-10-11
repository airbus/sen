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
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace
{

/// Writes the module, returning its path so a depfile can name it.
std::filesystem::path writeModule(sen::gen::PythonGenerator& generator,
                                  const sen::lang::TypeSet& typeSet,
                                  const std::filesystem::path& inputFile)
{
  auto outputFile = inputFile.stem().string();
  sen::gen::PythonGenerator::replaceInvalidCharacters(outputFile);
  outputFile.append(".py");

  const std::string contents = generator.generateModule(typeSet);

  writeFile(outputFile, contents);
  std::cout << "stl|python> " << outputFile << std::endl;
  return outputFile;
}

}  // namespace

namespace sen::cli_gen
{

void setupPythonCli(CLI::App& app)
{
  auto py = app.add_subcommand("py", "Generate Python dataclasses");
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
                             std::vector<std::filesystem::path> outputs;
                             outputs.reserve(docs.size());
                             for (std::size_t i = 0; i < docs.size(); ++i)
                             {
                               outputs.emplace_back(writeModule(generator, *docs[i], args->inputs[i]));
                             }

                             if (!args->depFile.empty())
                             {
                               writeDepFile(args->depFile, outputs, context.readFiles());
                             }
                           });

  auto fom = setupFomInput(*py,
                           [](auto args)
                           {
                             const sen::lang::TypeSetContext typeSets =
                               sen::lang::parseFomDocuments(args->paths, args->extensions, args->mappingFiles, {});

                             sen::gen::PythonGenerator generator;
                             for (const auto& typeSet: typeSets)
                             {
                               writeModule(generator, typeSet, std::filesystem::path(typeSet.fileName));
                             }
                           });
  stl->excludes(fom);
}

}  // namespace sen::cli_gen
