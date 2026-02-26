// === util.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "util.h"

// sen
#include "sen/core/base/hash32.h"

// inja
#include <inja/environment.hpp>
#include <inja/json.hpp>

// std
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>

void writeFile(std::filesystem::path path,
               const unsigned* templateData,
               unsigned templateDataSize,
               const inja::json& packageData)
{
  const auto templateStr = sen::decompressSymbolToString(templateData, templateDataSize);

  std::ofstream file(path);
  inja::render_to(file, templateStr, packageData);
}

std::string toSnakeCase(const std::string& str)
{
  std::string result;

  for (std::size_t i = 0; i < str.size(); ++i)
  {
    if (std::isupper(str[i]) != 0)
    {
      if (i != 0)
      {
        result.append(1U, '_');
      }
      result.append(1U, static_cast<char>(std::tolower(str[i])));
    }
    else
    {
      result.append(1U, str[i]);
    }
  }
  return result;
}

bool isUpperCamelCase(const std::string& str)
{
  if (str.empty())
  {
    return true;
  }

  if (std::islower(str.at(0U)) != 0)
  {
    return false;
  }

  // no spaces
  if (str.find_first_of(' ') != std::string::npos)
  {
    return false;
  }

  // no underscores
  return str.find_first_of('_') == std::string::npos;
}
