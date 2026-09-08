// === cli_archive_setup.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "cli_archive_setup.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/unit_registry.h"
#include "sen/db/input.h"
#include "sen/db/recording_merger.h"

// cli11
#include <CLI/App.hpp>
#include <CLI/Validators.hpp>

// std
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

float64_t toSeconds(const sen::Duration& duration)
{
  static auto secondsUnit = sen::UnitRegistry::get().searchUnitByAbbreviation("s").value();
  const auto& durationUnit = sen::QuantityTraits<sen::Duration>::unit();
  return sen::Unit::convert(durationUnit, *secondsUnit, static_cast<float64_t>(duration.get()));
}

sen::db::RecordingMergeMode parseMergeMode(const std::string& mode)
{
  if (mode == "normal")
  {
    return sen::db::RecordingMergeMode::normalMerge;
  }
  if (mode == "zero")
  {
    return sen::db::RecordingMergeMode::zeroAligned;
  }
  if (mode == "offset")
  {
    return sen::db::RecordingMergeMode::offsetAligned;
  }

  std::string err;
  err.append("unknown merge mode '");
  err.append(mode);
  err.append("'");
  sen::throwRuntimeError(err);
}

[[nodiscard]] std::string readTerminalLine(std::string_view inputText)
{
  std::cout << inputText;
  std::string answer;
  if (!std::getline(std::cin, answer))
  {
    sen::throwRuntimeError("could not read duplicate object resolution from terminal");
  }
  return answer;
}

[[nodiscard]] std::optional<std::size_t> parseOption(const std::string& answer, std::size_t maxValue)
{
  try
  {
    std::size_t processedCharacters = 0U;
    const auto option = std::stoul(answer, &processedCharacters);
    if (processedCharacters == answer.size() && option >= 1U && option <= maxValue)
    {
      return option - 1U;
    }
  }
  catch (const std::invalid_argument&)
  {
    return std::nullopt;
  }
  catch (const std::out_of_range&)
  {
    return std::nullopt;
  }

  return std::nullopt;
}

[[nodiscard]] std::size_t chooseOption(std::string_view inputText, std::size_t maxValue)
{
  while (true)
  {
    if (const auto option = parseOption(readTerminalLine(inputText), maxValue))
    {
      return option.value();
    }

    std::cout << "Please enter a number between 1 and " << maxValue << ".\n";
  }

  SEN_UNREACHABLE();
}

[[nodiscard]] sen::db::RecordingMergeDuplicateObjectResolution keepSelectedDuplicateObject(
  const sen::db::RecordingMergeDuplicateObject& duplicateObject)
{
  for (std::size_t i = 0U; i < duplicateObject.objects.size(); ++i)
  {
    std::cout << "  [" << std::to_string(i + 1U) << "] " << duplicateObject.objects[i].formatFullName()
              << " from: " << duplicateObject.objects[i].getArchivePath() << "\n";
  }

  const auto selectedObject =
    chooseOption("Choose the object to keep [1-" + std::to_string(duplicateObject.objects.size()) + "]: ",
                 duplicateObject.objects.size());
  return sen::db::RecordingMergeKeepSelectedObject {selectedObject};
}

[[nodiscard]] bool hasDuplicateObjectNames(const sen::db::RecordingMergeRenameObjects& resolution,
                                           const sen::db::RecordingMergeDuplicateObject& duplicateObject)
{
  for (std::size_t i = 0U; i < duplicateObject.objects.size(); ++i)
  {
    auto lhsName = duplicateObject.objects[i].getName();
    for (const auto& rename: resolution.renamedObjects)
    {
      if (rename.objectIndex == i)
      {
        lhsName = rename.name;
      }
    }

    for (std::size_t j = i + 1U; j < duplicateObject.objects.size(); ++j)
    {
      auto rhsName = duplicateObject.objects[j].getName();
      for (const auto& rename: resolution.renamedObjects)
      {
        if (rename.objectIndex == j)
        {
          rhsName = rename.name;
        }
      }
      if (lhsName == rhsName)
      {
        return true;
      }
    }
  }

  return false;
}

[[nodiscard]] sen::db::RecordingMergeDuplicateObjectResolution renameDuplicateObjects(
  const sen::db::RecordingMergeDuplicateObject& duplicateObject)
{
  while (true)
  {
    sen::db::RecordingMergeRenameObjects resolution;

    for (std::size_t i = 0U; i < duplicateObject.objects.size(); ++i)
    {
      const auto& object = duplicateObject.objects[i];
      std::cout << "  [" << std::to_string(i + 1U) << "]" << " from: " << object.getArchivePath() << "\n";

      std::cout << "(Leave empty to keep the same name)\n\n";
      const auto inputText = "New object name [" + object.getName() + "]: ";
      auto name = readTerminalLine(inputText);
      if (!name.empty() && name != object.getName())
      {
        resolution.renamedObjects.push_back({i, std::move(name)});
      }
    }

    if (!hasDuplicateObjectNames(resolution, duplicateObject))
    {
      return resolution;
    }

    std::cout << "Renamed duplicate objects must have unique names. Please try again.\n";
  }
}

sen::db::RecordingMergeDuplicateObjectResolution resolveDuplicateObjectFromTerminal(
  const sen::db::RecordingMergeDuplicateObject& duplicateObject)
{
  std::cout << "\n********************************************************\n";
  std::cout << "Duplicate object name detected while merging recordings:\n\n";
  std::cout << duplicateObject.objects.begin()->getSession() << "." << duplicateObject.objects.begin()->getBus() << "."
            << duplicateObject.objects.begin()->getName() << " -- TYPE: " << duplicateObject.objects.begin()->getType()
            << "\n\n";
  std::cout << "From archives: ";
  for (std::size_t i = 0U; i < duplicateObject.objects.size(); ++i)
  {
    std::cout << duplicateObject.objects[i].getArchivePath();
    if (i < duplicateObject.objects.size() - 1)
    {
      std::cout << ", ";
    }
    else
    {
      std::cout << "\n";
    }
  }
  std::cout << "  [1] keep one object and discard the others\n";
  std::cout << "  [2] keep all objects and rename duplicates\n";

  const auto action = chooseOption("Choose how to resolve this duplicate [1-2]: ", 2U);
  if (action == 0U)
  {
    return keepSelectedDuplicateObject(duplicateObject);
  }

  return renameDuplicateObjects(duplicateObject);
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

void setupMerger(CLI::App& app)
{
  struct Args
  {
    std::vector<std::filesystem::path> inputPaths;
    std::filesystem::path outputPath = "merged_recording";
    std::string mergeMode = "normal";
    std::vector<float64_t> offsets;
  };
  auto args = std::make_shared<Args>();

  auto cmd = app.add_subcommand("merge", "combine multiple archives into one");
  cmd->add_option("input_paths", args->inputPaths, "Recording path")
    ->required()
    ->expected(2, -1)
    ->check(CLI::ExistingDirectory);
  cmd->add_option("-o,--output", args->outputPath, "Output recording path");
  cmd->add_option("--mode", args->mergeMode, "Merge mode")->check(CLI::IsMember({"normal", "zero", "offset"}));
  cmd->add_option("--offset", args->offsets, "offset for each input recording (in seconds)");

  cmd->callback(
    [args]()
    {
      try
      {
        const auto mode = parseMergeMode(args->mergeMode);
        if (mode == sen::db::RecordingMergeMode::offsetAligned && args->offsets.size() != args->inputPaths.size())
        {
          sen::throwRuntimeError("offset mode requires one --offset value for each input archive");
        }
        if (mode != sen::db::RecordingMergeMode::offsetAligned && !args->offsets.empty())
        {
          sen::throwRuntimeError("--offset can only be used with --mode offset");
        }

        sen::db::RecordingMergeSettings settings;
        settings.outputArchive = args->outputPath;
        settings.mode = mode;
        settings.duplicateObjectResolver = resolveDuplicateObjectFromTerminal;
        settings.inputArchives.reserve(args->inputPaths.size());
        for (std::size_t i = 0U; i < args->inputPaths.size(); ++i)
        {
          sen::db::RecordingMergeInput input;
          input.archivePath = args->inputPaths[i];
          if (mode == sen::db::RecordingMergeMode::offsetAligned)
          {
            input.offset = sen::Duration(std::chrono::duration<float64_t>(args->offsets[i]));
          }
          settings.inputArchives.push_back(std::move(input));
        }

        sen::db::mergeRecordings(settings);
        std::cout << "  output: " << settings.outputArchive.string() << "\n";
      }
      catch (const std::exception& err)
      {
        std::cerr << err.what() << "\n";
        exit(1);
      }
    });
}
