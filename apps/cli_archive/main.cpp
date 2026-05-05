// === main.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/unit_registry.h"
#include "sen/db/input.h"

// cli11
#include <CLI/App.hpp>
#include <CLI/CLI.hpp>  // NOLINT (misc-include-cleaner): to correctly link
#include <CLI/Validators.hpp>

// std
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

float64_t toSeconds(const sen::Duration& duration)
{
  static auto secondsUnit = sen::UnitRegistry::get().searchUnitByAbbreviation("s").value();
  const auto& durationUnit = sen::QuantityTraits<sen::Duration>::unit();
  return sen::Unit::convert(durationUnit, *secondsUnit, static_cast<float64_t>(duration.get()));
}

void setupInfo(CLI::App& app)
{
  struct Args
  {
    std::filesystem::path archivePath;
  };

  auto args = std::make_shared<Args>();

  auto cmd = app.add_subcommand("info", "Print basic information about an archive");
  cmd->add_option("archive_path", args->archivePath, "Archive path")->required()->check(CLI::ExistingDirectory);
  cmd->callback(
    [args]()
    {
      try
      {
        sen::CustomTypeRegistry nativeTypes;
        sen::db::Input input(args->archivePath, nativeTypes);

        const auto& summary = input.getSummary();

        std::cout << "  path:            " << args->archivePath.string() << "\n";
        std::cout << "  duration:        " << toSeconds(summary.lastTime - summary.firstTime) << "s\n";
        std::cout << "  start:           " << summary.firstTime.toLocalString() << "\n";
        std::cout << "  end:             " << summary.lastTime.toLocalString() << "\n";
        std::cout << "  objects:         " << summary.objectCount << "\n";
        std::cout << "  types:           " << summary.typeCount << "\n";
        std::cout << "  annotations:     " << summary.annotationCount << "\n";
        std::cout << "  keyframes:       " << summary.keyframeCount << "\n";
        std::cout << "  indexed objects: " << summary.indexedObjectCount << "\n";
      }
      catch (const std::exception& err)
      {
        std::cerr << err.what() << "\n";
        exit(1);
      }
    });
}

void setupIndexed(CLI::App& app)
{
  struct Args
  {
    std::filesystem::path archivePath;
  };
  auto args = std::make_shared<Args>();

  auto cmd = app.add_subcommand("indexed", "Print basic info about the indexed objects");
  cmd->add_option("archive_path", args->archivePath, "Archive path")->required()->check(CLI::ExistingDirectory);
  cmd->callback(
    [args]()
    {
      try
      {
        sen::CustomTypeRegistry nativeTypes;
        sen::db::Input input(args->archivePath, nativeTypes);

        const std::string header0 = "OBJECT NAME";
        const std::string header1 = "TYPE NAME";
        const std::string header2 = "BUS";

        // compute the maximum name length
        std::size_t maxNameLength = header0.size();
        std::size_t maxTypeLength = header1.size();
        for (const auto& elem: input.getObjectIndexDefinitions())
        {
          maxNameLength = std::max(maxNameLength, elem.name.size());
          maxTypeLength = std::max(maxTypeLength, elem.type->getQualifiedName().size());
        }

        // NOLINTNEXTLINE
        printf("%s%*s    %s%*s    %s\n",
               header0.c_str(),
               static_cast<int>(maxNameLength - header0.size()),
               "",
               header1.c_str(),
               static_cast<int>(maxTypeLength - header1.size()),
               "",
               header2.c_str());

        for (const auto& elem: input.getObjectIndexDefinitions())
        {
          const auto& typeName = elem.type->getQualifiedName();

          // NOLINTNEXTLINE
          printf("%s%*s    %s%*s    %s.%s\n",
                 elem.name.c_str(),
                 static_cast<int>(maxNameLength - elem.name.size()),
                 "",
                 typeName.data(),
                 static_cast<int>(maxTypeLength - typeName.size()),
                 "",
                 elem.session.c_str(),
                 elem.bus.c_str());
        }
      }
      catch (const std::exception& err)
      {
        std::cerr << err.what() << "\n";
        exit(1);
      }
    });
}

int runApp(int argc, char* argv[])
{
  CLI::App app {"Recording inspection utility\n"};
  app.name("sen archive");
  app.get_formatter()->column_width(12);  // NOLINT

  setupInfo(app);
  setupIndexed(app);

  app.footer("For help on specific commands run 'sen archive <command> --help'");

  CLI11_PARSE(app, argc, argv);
  return 0;
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
