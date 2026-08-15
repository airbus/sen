// === util.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "util.h"

// sen
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/meta/custom_type.h"

// inja
#include <inja/environment.hpp>
#include <inja/inja.hpp>  // NOLINT(misc-include-cleaner)
#include <inja/json.hpp>

// std
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace sen::gen::detail
{

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

}  // namespace

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
  env.add_callback("firstUpper", 1, [](auto& args) { return capitalize(args.front()->template get<std::string>()); });
}

std::vector<std::string> tokenize(const std::string& str, const char delim)
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

std::string prepend(const std::string& prefix, const std::string str)
{
  std::string result(prefix);
  result.append(str);
  return result;
}

std::string append(const std::string& lhs, const std::string rhs)
{
  std::string result(lhs);
  result.append(rhs);
  return result;
}

std::string capitalize(const std::string& str)
{
  std::string result(str);
  result[0] = static_cast<char>(std::toupper(result[0]));
  return result;
}

std::string capitalize(std::string_view str) { return capitalize(std::string(str)); }

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

namespace
{

void collectDependentTypeSets(const sen::lang::TypeSet* set,
                              std::vector<const sen::lang::TypeSet*>& ordered,
                              std::unordered_set<const sen::lang::TypeSet*>& seen)
{
  if (!seen.insert(set).second)
  {
    return;
  }
  ordered.push_back(set);
  for (const auto* imported: set->importedSets)
  {
    collectDependentTypeSets(imported, ordered, seen);
  }
}

}  // namespace

std::vector<const sen::lang::TypeSet*> collectAllTypeSets(const sen::lang::TypeSetContext& typeSets)
{
  std::vector<const sen::lang::TypeSet*> ordered;
  std::unordered_set<const sen::lang::TypeSet*> seen;
  for (const auto& set: typeSets)
  {
    collectDependentTypeSets(&set, ordered, seen);
  }
  return ordered;
}

}  // namespace sen::gen::detail
