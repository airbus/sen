// === type.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/type.h"

// sen
#include "sen/core/base/result.h"

// std
#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace sen
{

Type::Type(MemberHash hash) noexcept: hash_ {hash} {}

MemberHash Type::getHash() const noexcept { return hash_; }

sen::Result<void, std::string> Type::validateLowerCaseName(std::string_view name)
{
  if (name.empty())
  {
    return Err(std::string("empty name"));
  }

  const auto firstChar = static_cast<unsigned char>(name.front());

  if (std::isdigit(firstChar) != 0)
  {
    return Err(std::string("not allowed to start with digits"));
  }

  if (std::isupper(firstChar) != 0)
  {
    return Err(std::string("not allowed to start with uppercase letter"));
  }

  if (firstChar == '_')
  {
    return Err(std::string("not allowed to start with _"));
  }

  if (firstChar == '.')
  {
    return Err(std::string("not allowed to start with ."));
  }

  auto wrongCharItr = std::find_if(                                     // NOSONAR
    name.begin(),                                                       // NOSONAR
    name.end(),                                                         // NOSONAR
    [](unsigned char c) { return c != '.' && std::isalnum(c) == 0; });  // NOSONAR

  if (wrongCharItr != name.end())
  {
    std::string err;
    err.append("name must only contain alpha-numeric characters but '");
    err.push_back(*wrongCharItr);
    err.append("' was found in '");
    err.append(name);
    err.append("'");
    return Err(err);
  }

  return Ok();
}

sen::Result<void, std::string> Type::validateTypeName(std::string_view name)
{
  if (name.empty())
  {
    return Err(std::string("empty name"));
  }

  const auto firstChar = static_cast<unsigned char>(name.front());

  if (std::isdigit(firstChar) != 0)
  {
    return Err(std::string("not allowed to start with digits"));
  }

  if (std::islower(firstChar) != 0)
  {
    return Err(std::string("not allowed to start with lowercase letter"));
  }

  if (firstChar == '_')
  {
    return Err(std::string("not allowed to start with _"));
  }

  if (firstChar == '.')
  {
    return Err(std::string("not allowed to start with ."));
  }

  auto wrongCharItr = std::find_if(                                     // NOSONAR
    name.begin(),                                                       // NOSONAR
    name.end(),                                                         // NOSONAR
    [](unsigned char c) { return c != '.' && std::isalnum(c) == 0; });  // NOSONAR

  if (wrongCharItr != name.end())
  {
    std::string err;
    err.append("name must only contain alpha-numeric characters but '");
    err.push_back(*wrongCharItr);
    err.append("' was found in '");
    err.append(name);
    err.append("'");
    return Err(err);
  }

  return Ok();
}

}  // namespace sen
