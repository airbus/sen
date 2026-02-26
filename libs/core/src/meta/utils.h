// === utils.h =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_CORE_SRC_META_UTILS_H
#define SEN_LIBS_CORE_SRC_META_UTILS_H

#include "sen/core/base/assert.h"
#include "sen/core/meta/callable.h"

// std
#include <string>

namespace sen::impl
{

/// Helper to capitalize a string
inline std::string capitalize(const std::string& str) noexcept
{
  SEN_EXPECT(!str.empty());

  std::string result = str;
  result.at(0) = static_cast<char>(std::toupper(str.at(0U)));
  return result;
}

/// Type safe version of std::islower
inline bool isLower(char ch) noexcept { return std::islower(static_cast<unsigned char>(ch)) != 0; }

/// Type safe version of std::isdigit
inline bool isDigit(char ch) noexcept { return std::isdigit(static_cast<unsigned char>(ch)) != 0; }

/// Throws if an identifier is not correct
inline void checkIdentifier(std::string_view name, bool isQualifiedName = false)
{

  // names shall not be empty
  if (name.empty())
  {
    throwRuntimeError("identifier is empty");
  }

  // identifiers must not start with digits
  if (impl::isDigit(name.at(0U)))
  {
    throwRuntimeError("identifiers shall not start with digits");
  }

  // identifiers must not contain spaces
  if (name.find_first_of(' ') != std::string::npos)
  {
    throwRuntimeError("identifiers shall not contain spaces");
  }

  // identifiers must not contain invalid characters
  if (auto pos = name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_");
      pos != std::string::npos)
  {
    // do not throw error if dot character is found in qualified name identifier
    if (!isQualifiedName || name[pos] != '.')
    {
      std::string err;
      err.append("invalid character '");
      err.append(1, name[pos]);
      err.append("'");
      err.append(" in identifier: ");
      err.append(name);
      throwRuntimeError(err);
    }
  }
}

/// Throws if a member name is not correct
inline void checkMemberName(const std::string& name)
{
  checkIdentifier(name);

  // member names must start with lower case
  if (!impl::isLower(name.at(0U)))
  {
    throwRuntimeError("member name does not start with lower case");
  }
}

/// Throws if a user-defined type name is not correct
inline void checkUserTypeName(const std::string& name)
{
  checkIdentifier(name);

  // type names shall start with capital leter
  if (impl::isLower(name.at(0U)))
  {
    throwRuntimeError("user-defined type name does not start with upper case");
  }
}

}  // namespace sen::impl

#endif  // SEN_LIBS_CORE_SRC_META_UTILS_H
