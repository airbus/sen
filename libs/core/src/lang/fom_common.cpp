// === fom_common.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "fom_common.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"

// std
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace sen::lang::fom
{

namespace
{

[[nodiscard]] char asciiToLower(char in)
{
  if (in <= 'Z' && in >= 'A')
  {
    return in - ('Z' - 'z');  // NOLINT(bugprone-narrowing-conversions)
  }

  return in;
}

[[nodiscard]] bool charCompareIgnoreCase(char a, char b) noexcept
{
  return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

[[nodiscard]] std::string toLowerCamelCaseHelper(const std::string& original)
{
  // split the string into 'words'
  std::vector<std::string_view> words = splitString(original, "_-");

  std::string result;
  result.reserve(original.size());

  // correct each word and append it to the result
  for (auto& word: words)
  {
    // ignore empty words
    if (word.empty())
    {
      continue;
    }

    auto prevResultSize = result.size();
    result.append(word);

    auto wordStart = result.begin() + static_cast<int>(prevResultSize);
    auto wordEnd = result.end();

    // if the word is all upper, make it all lower
    if (std::all_of(wordStart, wordEnd, [](auto c) { return std::isupper(c) || std::isdigit(c); }))
    {
      std::transform(wordStart, wordEnd, wordStart, [](auto c) { return std::tolower(c); });
    }

    // ensure the first letter is upper case
    *wordStart = static_cast<char>(std::toupper(*wordStart));
  }

  if (!result.empty() && std::isdigit(static_cast<int>(result.at(0))) != 0)
  {
    result = std::string("n").append(result);
  }

  return result;
}

}  // namespace

[[nodiscard]] bool startsWith(const std::string& str, const std::string subStr) noexcept
{
  return str.rfind(subStr, 0) == 0;  // NOLINT
}

[[nodiscard]] bool isNumber(const std::string& s)
{
  return !s.empty() &&
         std::find_if(s.begin(), s.end(), [](unsigned char c) { return std::isdigit(c) == 0; }) == s.end();
}

std::optional<std::uint64_t> parseUnsigned(std::string_view text)
{
  // Surrounding whitespace and a leading + are how the value was written, not part of it.
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
  {
    text.remove_prefix(1);
  }
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
  {
    text.remove_suffix(1);
  }
  if (!text.empty() && text.front() == '+')
  {
    text.remove_prefix(1);
  }

  if (text.empty())
  {
    return std::nullopt;
  }

  std::uint64_t value = 0;
  const auto* const last = text.data() + text.size();
  const auto result = std::from_chars(text.data(), last, value);
  if (result.ec != std::errc {} || result.ptr != last)
  {
    return std::nullopt;
  }

  return value;
}

std::vector<std::string_view> splitString(const std::string_view input, const std::string_view delimiters)
{
  std::vector<std::string_view> result;
  std::size_t lastPos = 0;
  for (std::size_t i = 0; i < input.size(); ++i)
  {
    if (delimiters.find(input[i]) != std::string::npos)
    {
      result.push_back(input.substr(lastPos, i - lastPos));
      ++i;
      lastPos = i;
    }
  }

  if (lastPos < input.size())
  {
    result.push_back(input.substr(lastPos));
  }

  return result;
}

[[nodiscard]] std::string toLower(const std::string& original)
{
  std::string data(original);
  std::transform(data.begin(), data.end(), data.begin(), asciiToLower);
  return data;
}

[[nodiscard]] std::string toLowerCamelCase(const std::string& original)
{
  if (original.empty())
  {
    return original;
  }

  std::string result = toLowerCamelCaseHelper(original);
  while (std::any_of(result.begin(), result.end(), [](auto c) { return c == '_' || c == '-'; }))
  {
    result = toLowerCamelCaseHelper(result);
  }

  // ensure the first letter is lower case
  if (!result.empty())
  {
    result.front() = static_cast<char>(std::tolower(result.front()));
  }

  // ensure we are not hitting a reserved word in C++
  if (result == "true" || result == "false" || result == "class" || result == "enum" || result == "struct" ||
      result == "static")
  {
    return result + "Val";
  }

  return result;
}

[[nodiscard]] bool findStringIgnoreCase(const std::string& strHaystack, const std::string& strNeedle)
{
  auto it = std::search(strHaystack.begin(),
                        strHaystack.end(),
                        strNeedle.begin(),
                        strNeedle.end(),
                        [](unsigned char ch1, unsigned char ch2) { return std::toupper(ch1) == std::toupper(ch2); });
  return (it != strHaystack.end());
}

[[nodiscard]] std::string toSenPropertyName(const std::string& original)
{
  auto result = toLowerCamelCase(original);

  if (result == "name" || result == "id" || result == "localName" || result == "lastCommitTime" ||
      result == "propertyUntyped")
  {
    return result + "NonSen";
  }

  return result;
}

[[nodiscard]] std::string formatSemantics(std::string str)
{
  std::replace(str.begin(), str.end(), '\n', ' ');
  std::replace(str.begin(), str.end(), '"', '\'');
  return str;
}

[[nodiscard]] bool isTrue(std::string_view str, std::string_view attribute)
{
  if (str.empty())
  {
    return false;
  }

  constexpr std::string_view trueStr("true");
  constexpr std::string_view falseStr("false");

  if (std::equal(trueStr.begin(), trueStr.end(), str.begin(), str.end(), charCompareIgnoreCase))
  {
    return true;
  }

  if (std::equal(falseStr.begin(), falseStr.end(), str.begin(), str.end(), charCompareIgnoreCase))
  {
    return false;
  }

  std::string err;
  err.append("invalid value '");
  err.append(str);
  err.append("' for '");
  err.append(attribute);
  err.append("' in a mapping, which takes 'true' or 'false'");
  throwRuntimeError(err);
}

void collectClassesNodes(pugi::xml_node node, std::vector<pugi::xml_node>& result)
{
  // The HLA root is the root, not a class. It reaches here when it carries an attribute of its
  // own, as the MIM's privilegeToDelete does, and a class with no parent cannot be built.
  const bool isRoot = std::string(node.child_value("name")) == "HLAobjectRoot";

  if (!isRoot && (!node.select_nodes("semantics").empty() || !node.select_nodes("attribute").empty()))
  {
    result.push_back(node);
  }

  for (auto& child: node.select_nodes("objectClass"))
  {
    collectClassesNodes(child.node(), result);
  }
}

void collectInteractionNodes(pugi::xml_node node, std::vector<pugi::xml_node>& result)
{
  if (!node.select_nodes("semantics").empty() || !node.select_nodes("parameter").empty())
  {
    result.push_back(node);
  }

  for (auto& child: node.select_nodes("interactionClass"))
  {
    collectInteractionNodes(child.node(), result);
  }
}

[[nodiscard]] std::string objectClassPath(pugi::xml_node node)
{
  std::string path;
  path.reserve(256);

  for (auto current = node; !current.empty() && std::string(current.name()) == "objectClass";
       current = current.parent())
  {
    const std::string name = current.child_value("name");
    if (name == "HLAobjectRoot")
    {
      break;
    }

    if (!path.empty())
    {
      path.insert(0, 1, '.');
    }
    path.insert(0, name);
  }

  return path;
}

[[nodiscard]] std::string computeInteractionPath(const pugi::xpath_node& interactionNode)
{
  std::string path;
  path.reserve(256);
  auto node = interactionNode;

  while (node)
  {
    auto parentName = std::string(node.parent().child_value("name"));

    if (parentName == "HLAinteractionRoot")
    {
      break;
    }

    path =                                                  // NOLINT(performance-inefficient-string-concatenation)
      path.empty() ? parentName : parentName + '.' + path;  // NOLINT(performance-inefficient-string-concatenation)
    node = node.parent();
  }

  return path;
}

[[nodiscard]] std::string interactionClassPath(pugi::xml_node node)
{
  std::string path;
  path.reserve(256);

  for (auto current = node; !current.empty() && std::string(current.name()) == "interactionClass";
       current = current.parent())
  {
    const std::string name = current.child_value("name");
    if (name == "HLAinteractionRoot")
    {
      break;
    }

    if (!path.empty())
    {
      path.insert(0, 1, '.');
    }
    path.insert(0, name);
  }

  return path;
}

std::string computeClassPath(const ClassType* parent, const std::string& child, const ClassType* root)
{
  std::string classPath = child;

  while (parent != root)
  {
    classPath = std::string(parent->getName()) + "." + classPath;  // NOLINT
    parent = parent->getParents().empty() ? nullptr : parent->getParents().front().type();
  }

  return classPath;
}

[[nodiscard]] std::vector<Arg> prependArgs(const std::vector<Arg>& currentArgs, const std::vector<Arg>& argsToPrepend)
{
  if (argsToPrepend.empty())
  {
    return currentArgs;
  }

  auto result = argsToPrepend;
  result.insert(result.end(), currentArgs.begin(), currentArgs.end());
  return result;
}

std::optional<TransportMode> getTransportMode(const std::string& hlaTransport)
{
  if (hlaTransport == "HLAbestEffort")
  {
    return TransportMode::multicast;
  }

  if (hlaTransport == "HLAreliable")
  {
    return TransportMode::confirmed;
  }

  return std::nullopt;
}

std::optional<TransportMode> getMethodTransportMode(const std::string& hlaTransport)
{
  if (hlaTransport == "HLAbestEffort")
  {
    return TransportMode::unicast;
  }

  if (hlaTransport == "HLAreliable")
  {
    return TransportMode::confirmed;
  }

  return std::nullopt;
}

std::optional<TransportMode> getEventTransportMode(const std::string& hlaTransport)
{
  if (hlaTransport == "HLAbestEffort")
  {
    return TransportMode::multicast;
  }

  if (hlaTransport == "HLAreliable")
  {
    return TransportMode::confirmed;
  }

  return std::nullopt;
}

void collectConstructorArgs(std::vector<Arg>& list, const ClassType& type)
{
  auto allProps = type.getProperties(ClassType::SearchMode::includeParents);
  for (const auto& prop: allProps)
  {
    if (prop->getCategory() == PropertyCategory::staticRW)
    {
      list.emplace_back(std::string(prop->getName()), std::string(prop->getDescription()), prop->getType());
    }
  }
}

void collectConstructorArgs(std::vector<Arg>& list, const ClassSpec& spec)
{
  // collect parents args recursively
  for (const auto& parent: spec.parents)
  {
    collectConstructorArgs(list, *parent);
  }

  // collect class args
  for (const auto& prop: spec.properties)
  {
    if (prop.category == PropertyCategory::staticRW)
    {
      list.emplace_back(prop.name, prop.description, prop.type);
    }
  }
}

}  // namespace sen::lang::fom
