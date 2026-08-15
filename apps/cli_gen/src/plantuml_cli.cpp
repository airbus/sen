// === plantuml_cli.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// app
#include "cli_input.h"
#include "io.h"

// lib
#include "sen/gen/plantuml.h"

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

struct UmlArgs
{
  std::string outputFile = "output.plantuml";
  bool onlyClasses = false;
  bool onlyTypes = false;
  bool noEnumerators = false;
};

void setupUmlArgs(CLI::App& app, UmlArgs& args)
{
  app.add_option("-o, --output", args.outputFile, "Output PlantUML file");
  auto onlyClasses = app.add_flag("--only-classes", args.onlyClasses, "Only generate class diagrams");
  auto onlyTypes = app.add_flag("--only-types", args.onlyTypes, "Only generate diagrams for basic types");
  auto noEnumerators = app.add_flag("--no-enumerators", args.noEnumerators, "Do not generate enumerations");

  onlyClasses->excludes(onlyTypes);
  onlyClasses->excludes(noEnumerators);
}

sen::gen::PlantUMLGenerationMode getUmlGenerationMode(const UmlArgs& args)
{
  if (args.onlyClasses)
  {
    return sen::gen::PlantUMLGenerationMode::onlyClasses;
  }
  if (args.onlyTypes)
  {
    return sen::gen::PlantUMLGenerationMode::onlyBasicTypes;
  }
  return sen::gen::PlantUMLGenerationMode::all;
}

sen::gen::PlantUMLEnumMode getUmlEnumMode(const UmlArgs& args)
{
  return args.noEnumerators ? sen::gen::PlantUMLEnumMode::noEnumerators : sen::gen::PlantUMLEnumMode::all;
}

void writeOutput(const sen::lang::TypeSetContext& typeSets, const UmlArgs& args)
{
  sen::gen::PlantUMLGenerator generator;
  const std::string contents = generator.generate(typeSets, getUmlGenerationMode(args), getUmlEnumMode(args));

  writeFile(args.outputFile, contents);
  std::cout << "stl|uml> " << args.outputFile << std::endl;
}

}  // namespace

namespace sen::cli_gen
{

void setupPlantUMLCli(CLI::App& app)
{
  auto umlArgs = std::make_shared<UmlArgs>();
  auto uml = app.add_subcommand("uml", "Generate UML diagrams");
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
                             writeOutput(typeSets, *umlArgs);
                           });

  setupUmlArgs(*stl, *umlArgs);

  auto fom = setupFomInput(*uml,
                           [umlArgs](auto args)
                           {
                             const sen::lang::TypeSetContext typeSets =
                               sen::lang::parseFomDocuments(args->paths, args->mappingFiles, {});
                             writeOutput(typeSets, *umlArgs);
                           });

  setupUmlArgs(*fom, *umlArgs);
  stl->excludes(fom);
}

}  // namespace sen::cli_gen
