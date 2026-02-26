// === util.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "util.h"

// sen
#include "sen/core/lang/stl_resolver.h"

// std
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>

// inja
#include <inja/inja.hpp>  // NOLINT(misc-include-cleaner)

namespace
{

nlohmann::json readJsonFile(const std::filesystem::path& path)
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
