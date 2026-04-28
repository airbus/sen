// === string_case.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/lang/string_utils.h"

// sen
#include "sen/core/base/checked_conversions.h"

// std
#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace sen
{

std::string capitalizeAndRemoveSeparators(std::string_view str)
{
  std::string result;
  result.reserve(str.size());
  for (const auto c: str)
  {
    if (c != '-' && c != '_')
    {
      result.push_back(c);
    }
  }
  if (!result.empty())
  {
    result.front() = std_util::checkedConversion<char>(std::toupper(static_cast<unsigned char>(result.front())));
  }
  return result;
}

std::string snakeCaseToPascalCase(std::string_view str)
{
  std::string result;
  result.reserve(str.size());
  bool capitalizeNext = true;
  for (const auto ch: str)
  {
    if (ch == '_')
    {
      capitalizeNext = true;
      continue;
    }
    if (capitalizeNext)
    {
      result.push_back(std_util::checkedConversion<char>(std::toupper(static_cast<unsigned char>(ch))));
      capitalizeNext = false;
    }
    else
    {
      result.push_back(ch);
    }
  }
  return result;
}

std::string pascalCaseToSnakeCase(std::string_view str)
{
  std::string result;
  result.reserve(str.size() * 2U);
  for (std::size_t i = 0; i < str.size(); ++i)
  {
    const auto c = static_cast<unsigned char>(str[i]);
    if (std::isupper(c) != 0)
    {
      if (i != 0)
      {
        result.push_back('_');
      }
      result.push_back(std_util::checkedConversion<char>(std::tolower(c)));
    }
    else
    {
      result.push_back(str[i]);
    }
  }
  return result;
}

bool isCapitalizedAndNoSeparators(std::string_view str)
{
  if (str.empty())
  {
    return true;
  }
  if (std::islower(static_cast<unsigned char>(str.front())) != 0)
  {
    return false;
  }
  if (str.find_first_of(' ') != std::string_view::npos)
  {
    return false;
  }
  return str.find_first_of('_') == std::string_view::npos;
}

}  // namespace sen
