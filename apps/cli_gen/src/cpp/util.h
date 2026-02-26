// === util.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_APPS_CLI_GEN_SRC_CPP_UTIL_H
#define SEN_APPS_CLI_GEN_SRC_CPP_UTIL_H

// sen
#include "sen/core/lang/stl_resolver.h"

// inja
#include <inja/environment.hpp>
#include <inja/json.hpp>

// std
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

inline std::string prepend(const std::string& prefix, const std::string str);

inline std::string append(const std::string& lhs, const std::string rhs);

inline std::string capitalize(const std::string& str);

inline std::string capitalize(std::string_view str);

inline std::vector<std::string> tokenize(const std::string& str, const char delim);

template <typename T>
inline std::string intToHex(T i);

// Registers functions used by the templates.
inline void configureEnv(inja::Environment& env);

/// Reads the type settings out of a JSON file.
[[nodiscard]] sen::lang::TypeSettings readTypeSettings(const std::filesystem::path& path);

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

namespace impl
{  // TODO (@EP): should these functions be in a different header?

constexpr std::size_t maxCommentLineLength = 80U;

inline std::string getPassedType(const inja::json& data, bool byValueOnly)
{
  return byValueOnly
           ? inja::render(R"({{ qualType }})", data)
           : inja::render(R"({% if passByRef %}const {{ qualType }}&{% else %}{{ qualType }}{% endif %})", data);
}

inline std::string commaSeparatedList(const std::vector<inja::json>& list,
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

inline std::string listOf(const std::string& templ, const inja::json& data)
{
  auto list = data.get<std::vector<inja::json>>();
  return commaSeparatedList(list, [&](const inja::json& itemData) { return inja::render(templ, itemData); });
}

inline std::string getArgList(const inja::json& data, const inja::json& moreArgs, const inja::json& allByValue)
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

inline std::string inlineComment(const inja::json& data)
{
  auto result = inja::render("{% if description != \"\" %}  ///< {{ description }}{% endif %}", data);
  return result;
}

inline std::string blockComment(const inja::json& data, int indent)
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
    if (currentLineSize > maxCommentLineLength && i != words.size() - 1)  // NOLINT
    {
      result.back() = '\n';
      result.append(indentStr);
      result.append(lineStart);
      currentLineSize = lineStart.size() + indent;
    }
  }

  return result;
}

}  // namespace impl

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

template <typename T>
[[nodiscard]] std::string intToHex(T i)
{
  std::stringstream stream;
  stream << "0x" << std::setfill('0') << std::setw(sizeof(T) + sizeof(T)) << std::hex << i;
  return stream.str();
}

template <typename T>
[[nodiscard]] std::string intToHex2(T i)
{
  std::stringstream stream;
  stream << "0x" << std::setfill('0') << std::setw(sizeof(T)) << std::hex << i;
  return stream.str();
}

void configureEnv(inja::Environment& env)
{
  env.add_callback("passedType", [](auto& args) { return impl::getPassedType(*args.front(), false); });
  env.add_callback("inlineComment", 1, [](auto& args) { return impl::inlineComment(*args.front()); });
  env.add_callback(
    "blockComment", 2, [](auto& args) { return impl::blockComment(*args.at(0U), (*args.at(1U)).template get<int>()); });

  env.add_callback("argList",
                   [](auto& args)
                   {
                     inja::json moreArgs(args.size() == 2 ? *args.at(1U) : inja::json(false));
                     inja::json allByValue(args.size() == 3 ? *args.at(2U) : inja::json(false));
                     return impl::getArgList(*args.front(), moreArgs, allByValue);
                   });

  env.add_callback("listOf",
                   [](auto& args) { return impl::listOf(args.at(0U)->template get<std::string>(), *args.at(1U)); });
}

#endif  // SEN_APPS_CLI_GEN_SRC_CPP_UTIL_H
