// === util.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/hash32.h"
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/meta/custom_type.h"

// cli11
#include <CLI/App.hpp>
// NOLINTNEXTLINE (misc-include-cleaner): cli11 needs all headers to correctly link required vtables
#include <CLI/CLI.hpp>
#include <CLI/Validators.hpp>

// std
#include <algorithm>
#include <cctype>
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
#include <vector>

// inja
#include <inja/environment.hpp>
#include <inja/inja.hpp>  // NOLINT(misc-include-cleaner)
#include <inja/json.hpp>

namespace
{
[[nodiscard]] std::string getPassedType(const inja::json& data, bool byValueOnly)
{
  return byValueOnly
           ? inja::render(R"({{ qualType }})", data)
           : inja::render(R"({% if passByRef %}const {{ qualType }}&{% else %}{{ qualType }}{% endif %})", data);
}

[[nodiscard]] std::string commaSeparatedList(const std::vector<inja::json>& list,
                                             std::function<std::string(const inja::json&)> content)
{
  std::string result;
  for (std::size_t i = 0U; i < list.size(); ++i)
  {
    result.append(content(list[i]));
    if (i != list.size() - 1)
    {
      result.append(", ");
    }
  }
  return result;
}

[[nodiscard]] std::string listOf(const std::string& templ, const inja::json& data)
{
  auto list = data.get<std::vector<inja::json>>();
  return commaSeparatedList(list, [&](const inja::json& itemData) { return inja::render(templ, itemData); });
}

[[nodiscard]] std::string getArgList(const inja::json& data, const inja::json& moreArgs, const inja::json& allByValue)
{
  auto list = data.get<std::vector<inja::json>>();

  auto result =
    commaSeparatedList(list,
                       [&](const inja::json& item)
                       { return getPassedType(item, allByValue.get<bool>()) + inja::render(" {{ name }}", item); });

  if (!list.empty() && moreArgs.get<bool>())
  {
    result.append(", ");
  }

  return result;
}

[[nodiscard]] std::string inlineComment(const inja::json& data)
{
  auto result = inja::render("{% if description != \"\" %}  ///< {{ description }}{% endif %}", data);
  return result;
}

[[nodiscard]] std::string blockComment(const inja::json& data, int indent)
{
  auto description = data.get<std::string>();
  if (description.empty())
  {
    return {};
  }

  const std::string indentStr = std::string(indent, ' ');

  // get rid of new lines
  std::replace(description.begin(), description.end(), '\n', ' ');
  auto words = tokenize(description, ' ');

  const std::string lineStart = "/// ";
  std::string result = lineStart;

  std::size_t currentLineSize = lineStart.size();
  for (std::size_t i = 0; i < words.size(); ++i)
  {
    const auto& word = words.at(i);

    // add the current word
    result.append(word);
    result.append(" ");
    currentLineSize += word.size() + 1;

    // create a new line if the current is too long
    if (currentLineSize > impl::maxCommentLineLength && i != words.size() - 1)  // NOLINT
    {
      result.back() = '\n';
      result.append(indentStr);
      result.append(lineStart);
      currentLineSize = lineStart.size() + indent;
    }
  }

  return result;
}

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

CLI::App* setupStlInput(CLI::App& app, std::function<void(std::shared_ptr<impl::StlArgs>)>&& action)
{
  auto args = std::make_shared<impl::StlArgs>();

  auto stl = app.add_subcommand("stl", "process STL files");
  stl->add_option("stl_files", args->inputs, "STL files")->required()->check(CLI::ExistingFile);
  stl->add_option("-i, --import", args->includePaths, "Paths where other STL files can be found");
  stl->add_option("-b, --base_path", args->basePath, "base path for including generated files");
  stl->add_option("-s, --settings", args->codegenOptionsFile, "code generation settings file")
    ->check(CLI::ExistingFile);
  stl->callback([args, act = std::move(action)]() { act(args); });
  return stl;
}

CLI::App* setupFomInput(CLI::App& app, std::function<void(std::shared_ptr<impl::FomArgs>)>&& action)
{
  auto args = std::make_shared<impl::FomArgs>();

  auto fom = app.add_subcommand("fom", "process HLA FOM files");

  fom->add_option("-m, --mappings", args->mappingFiles, "XML defining custom mappings between sen and hla")
    ->delimiter(',')
    ->check(CLI::ExistingFile);

  fom->add_option("-d, --directories", args->paths, "directories containing FOM XML files")
    ->check(CLI::ExistingDirectory)
    ->required();

  fom->add_option("-s, --settings", args->codegenOptionsFile, "code generation settings file")
    ->check(CLI::ExistingFile);

  fom->callback([args, act = std::move(action)]() { act(args); });

  return fom;
}

[[nodiscard]] std::string prepend(const std::string& prefix, const std::string str)
{
  std::string result(prefix);
  result.append(str);
  return result;
}

[[nodiscard]] std::string append(const std::string& lhs, const std::string rhs)
{
  std::string result(lhs);
  result.append(rhs);
  return result;
}

[[nodiscard]] std::string capitalize(const std::string& str)
{
  std::string result(str);
  result[0] = static_cast<char>(std::toupper(result[0]));
  return result;
}

[[nodiscard]] std::string capitalize(std::string_view str) { return capitalize(std::string(str)); }

[[nodiscard]] std::vector<std::string> tokenize(const std::string& str, const char delim)
{
  std::vector<std::string> out;
  size_t start;
  size_t end = 0;

  while ((start = str.find_first_not_of(delim, end)) != std::string::npos)
  {
    end = str.find(delim, start);
    out.push_back(str.substr(start, end - start));
  }

  return out;
}

void configureEnv(inja::Environment& env)
{
  env.add_callback("passedType", [](auto& args) { return getPassedType(*args.front(), false); });
  env.add_callback("inlineComment", 1, [](auto& args) { return inlineComment(*args.front()); });
  env.add_callback(
    "blockComment", 2, [](auto& args) { return blockComment(*args.at(0U), (*args.at(1U)).template get<int>()); });

  env.add_callback("argList",
                   [](auto& args)
                   {
                     inja::json moreArgs(args.size() == 2 ? *args.at(1U) : inja::json(false));
                     inja::json allByValue(args.size() == 3 ? *args.at(2U) : inja::json(false));
                     return getArgList(*args.front(), moreArgs, allByValue);
                   });

  env.add_callback("listOf", [](auto& args) { return listOf(args.at(0U)->template get<std::string>(), *args.at(1U)); });
}

void openFile(const std::filesystem::path& path, std::ofstream& stream)
{
  stream.open(path, std::ios_base::trunc | std::ios_base::out);
  if (!stream.is_open() || stream.fail())
  {
    std::string err;
    err.append("could not open file '");
    err.append(path.string());
    err.append("' for writing");
    sen::throwRuntimeError(err);
  }
}

void readFile(const std::filesystem::path& fileName, std::string& contents)
{
  std::ifstream in(fileName);

  // reserve required memory in one go
  in.seekg(0U, std::ios::end);
  contents.reserve(static_cast<std::size_t>(in.tellg()));
  in.seekg(0U, std::ios::beg);

  // read contents
  contents.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::filesystem::path typesSourceFileFromSTLFile(const std::filesystem::path& file)
{
  return file.filename().concat(".h");
}

std::filesystem::path implSourceFileFromSTLFile(const std::filesystem::path& file)
{
  return file.filename().concat(".cpp");
}

std::string computeFileId(const std::string& fileName)
{
  std::filesystem::path filePath(fileName);

  std::string stringToUse = filePath.stem().string();
  if (filePath.has_parent_path())
  {
    stringToUse.append(filePath.parent_path().stem().string());
  }

  return intToHex2(sen::crc32(stringToUse));
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

std::string computeCppNamespace(const std::vector<std::string>& package)
{
  std::string result;
  for (std::size_t i = 0U; i < package.size(); ++i)
  {
    result.append(package[i]);
    if (i != package.size() - 1U)
    {
      result.append("::");
    }
  }

  return result;
}

std::string computeCppNamespace(const sen::lang::TypeSet& set) { return computeCppNamespace(set.package); }

std::string computeCppNamespace(const sen::CustomType& type)
{
  auto tokens = tokenize(std::string(type.getQualifiedName()), '.');
  if (tokens.size() > 1)
  {
    tokens.pop_back();
  }
  return computeCppNamespace(tokens);
}
