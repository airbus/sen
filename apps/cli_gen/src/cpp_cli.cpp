// === cpp_cli.cpp =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// app
#include "cli_input.h"
#include "io.h"

// lib
#include "sen/gen/cpp.h"

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
#include <vector>

namespace
{

struct CppExtraArgs
{
  bool recursive = false;
  bool publicSymbols = false;
};

struct ExportsArgs
{
  std::string packageName;
  std::vector<std::string> stlFiles;
  std::vector<std::string> dependentPackages;
  std::vector<std::string> userTypes;
};

void writeGenerated(const sen::gen::CppGenerator::FileContents& contents)
{
  for (const auto& [path, body]: contents)
  {
    writeFile(path, body);
    std::cout << "stl|cpp> " << path << std::endl;
  }
}

void runExports(const ExportsArgs& args)
{
  sen::gen::CppGenerator generator;
  sen::gen::CppExportsOptions opts;
  opts.packageName = args.packageName;
  opts.stlFiles = args.stlFiles;
  opts.packageDependencies = args.dependentPackages;
  opts.userImplementedClasses = args.userTypes;

  const std::string contents = generator.generateExports(opts);

  const std::string fileName = "sen_exported_types.cpp";

  writeFile(fileName, contents);
  std::cout << "stl|cpp> " << fileName << std::endl;
}

void configureExportsCommand(CLI::App* cmd)
{
  auto args = std::make_shared<ExportsArgs>();
  cmd->add_option("-p, --package", args->packageName, "Name of the package to create the exports for");
  cmd->add_option("-f, --file", args->stlFiles, "Name of the STL file containing types");
  cmd->add_option("-d, --depends", args->dependentPackages, "Names of the packages we depend on");
  cmd->add_option("-i, --implements", args->userTypes, "Names of types implemented in this package");
  cmd->callback([args]() { runExports(*args); });
}

}  // namespace

namespace sen::cli_gen
{

void setupCppCli(CLI::App& app)
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
                             sen::gen::CppGenerator generator;
                             sen::gen::CppOptions opts;
                             opts.recursive = cppExtraArgs->recursive;
                             opts.publicSymbols = cppExtraArgs->publicSymbols;
                             opts.basePath = args->basePath;

                             const auto settings = readTypeSettings(args->codegenOptionsFile);

                             sen::lang::TypeSetContext context;
                             for (const auto& fileName: args->inputs)
                             {
                               auto typeSet = sen::lang::readTypesFile(fileName, args->includePaths, context, settings);
                               writeGenerated(generator.generate(*typeSet, opts));
                             }
                           });

  auto fom = setupFomInput(*cpp,
                           [cppExtraArgs](auto args)
                           {
                             sen::gen::CppGenerator generator;
                             sen::gen::CppOptions opts;
                             opts.recursive = cppExtraArgs->recursive;
                             opts.publicSymbols = cppExtraArgs->publicSymbols;
                             // FOM input emits next to its sources; no basePath needed.

                             const auto settings = readTypeSettings(args->codegenOptionsFile);
                             const auto typeSets =
                               sen::lang::parseFomDocuments(args->paths, args->mappingFiles, settings);

                             for (const auto& set: typeSets)
                             {
                               writeGenerated(generator.generate(set, opts));
                             }
                           });

  auto exports = cpp->add_subcommand("exports", "Generate the export symbols file for a sen STL package");
  configureExportsCommand(exports);

  stl->excludes(fom);

  // Top-level alias for `generate cpp exports`; hidden from --help via empty group.
  auto exp = app.add_subcommand("exp_package", "Alias for 'generate cpp exports'");
  exp->group("");
  configureExportsCommand(exp);
}

}  // namespace sen::cli_gen
