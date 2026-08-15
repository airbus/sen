// === util.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_GEN_SRC_COMMON_UTIL_H
#define SEN_LIBS_GEN_SRC_COMMON_UTIL_H

// sen
#include "sen/core/lang/stl_resolver.h"
#include "sen/core/meta/custom_type.h"

// inja
#include <inja/environment.hpp>

// std
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace sen::gen::detail
{

namespace impl
{
constexpr std::size_t maxCommentLineLength = 80U;
}  // namespace impl

/// Registers inja helper callbacks used by generator templates.
void configureEnv(inja::Environment& env);

/// Splits a string into substrings by a delimiter, skipping empty results.
[[nodiscard]] std::vector<std::string> tokenize(const std::string& str, const char delim);

/// Appends a prefix to the beginning of a given string
[[nodiscard]] std::string prepend(const std::string& prefix, const std::string str);

/// Appends a suffix to the end of a given string
[[nodiscard]] std::string append(const std::string& lhs, const std::string rhs);

/// Transform a string to uppercase
[[nodiscard]] std::string capitalize(const std::string& str);

/// Transform a string_view to uppercase
[[nodiscard]] std::string capitalize(std::string_view str);

/// Computes C++ namespace from a string package
[[nodiscard]] std::string computeCppNamespace(const std::vector<std::string>& package);

/// Computes C++ namespace from a type set
[[nodiscard]] std::string computeCppNamespace(const sen::lang::TypeSet& set);

/// Computes C++ namespace from a custom type
[[nodiscard]] std::string computeCppNamespace(const sen::CustomType& type);

/// Walks the given context's type sets and their imports depth-first, returning unique sets in
/// insertion order. Insertion order (rather than pointer order) keeps generator output stable
/// across builds for generators that emit a single consolidated file or rely on iteration order
/// for dependency layering.
[[nodiscard]] std::vector<const sen::lang::TypeSet*> collectAllTypeSets(const sen::lang::TypeSetContext& typeSets);

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

}  // namespace sen::gen::detail

#endif  // SEN_LIBS_GEN_SRC_COMMON_UTIL_H
