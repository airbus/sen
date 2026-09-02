// === cli_input.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "cli_input.h"

// sen
#include "sen/core/lang/stl_resolver.h"

// cli11
#include <CLI/App.hpp>
// NOLINTNEXTLINE (misc-include-cleaner): cli11 needs all headers to correctly link required vtables
#include <CLI/CLI.hpp>
#include <CLI/Validators.hpp>

// nlohmann
#include <nlohmann/json.hpp>

// std
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ios>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace
{

[[nodiscard]] nlohmann::json readJsonFile(const std::filesystem::path& path)
{
  std::ifstream in(path);

  std::string contents;

  // reserve required memory in one go
  in.seekg(0U, std::ios::end);
  contents.reserve(static_cast<std::size_t>(in.tellg()));
  in.seekg(0U, std::ios::beg);

  // read contents
  contents.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());

  return nlohmann::json::parse(contents);
}

void populateStringSet(const nlohmann::json& settings, std::string_view listName, std::unordered_set<std::string>& set)
{
  const auto itr = settings.find(listName);

  // we might not have the list
  if (itr == settings.end())
  {
    return;
  }

  // if it has something inside, it has to be a list
  if (!itr->empty() && !itr->is_array())
  {
    throw std::runtime_error(std::string("expecting a list for ").append(listName));
  }

  for (auto& memberName: *itr)
  {
    // the list should contain strings
    if (!memberName.is_string())
    {
      throw std::runtime_error(std::string("expecting strings in the list ").append(listName));
    }

    set.insert(memberName);
  }
}

}  // namespace

namespace sen::cli_gen
{

CLI::App* setupStlInput(CLI::App& app, std::function<void(std::shared_ptr<StlArgs>)>&& action)
{
  auto args = std::make_shared<StlArgs>();

  auto stl = app.add_subcommand("stl", "Process STL files");
  stl->add_option("stl_files", args->inputs, "STL files")->required()->check(CLI::ExistingFile);
  stl->add_option("-i, --import", args->includePaths, "Paths where other STL files can be found");
  // `--base_path` accepted as a snake_case alias for `--base-path`.
  stl->add_option("-b,--base-path,--base_path", args->basePath, "Base path for including generated files");
  stl->add_option("-s, --settings", args->codegenOptionsFile, "Code generation settings file")
    ->check(CLI::ExistingFile);
  stl->callback([args, act = std::move(action)]() { act(args); });
  return stl;
}

CLI::App* setupFomInput(CLI::App& app, std::function<void(std::shared_ptr<FomArgs>)>&& action)
{
  auto args = std::make_shared<FomArgs>();

  auto fom = app.add_subcommand("fom", "Process HLA FOM files");

  fom->add_option("-m, --mappings", args->mappingFiles, "XML defining custom mappings between sen and HLA")
    ->delimiter(',')
    ->check(CLI::ExistingFile);

  fom->add_option("-d, --directories", args->paths, "Directories containing FOM XML files")
    ->check(CLI::ExistingDirectory)
    ->required();

  fom->add_option("-s, --settings", args->codegenOptionsFile, "Code generation settings file")
    ->check(CLI::ExistingFile);

  fom->callback([args, act = std::move(action)]() { act(args); });

  return fom;
}

sen::lang::TypeSettings readTypeSettings(const std::filesystem::path& path)
{
  if (path.empty())
  {
    return {};
  }

  auto file = readJsonFile(path);
  sen::lang::TypeSettings result;

  if (auto classesItr = file.find("classes"); classesItr != file.end())
  {
    for (const auto& classElem: classesItr->items())
    {
      sen::lang::ClassAnnotations classAnnotations;
      populateStringSet(classElem.value(), "deferredMethods", classAnnotations.deferredMethods);
      populateStringSet(classElem.value(), "checkedProperties", classAnnotations.checkedProperties);
      result.classAnnotations[classElem.key()] = classAnnotations;
    }
  }

  return result;
}

}  // namespace sen::cli_gen
