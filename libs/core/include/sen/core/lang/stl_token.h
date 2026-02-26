// === stl_token.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_LANG_STL_TOKEN_H
#define SEN_CORE_LANG_STL_TOKEN_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/lang/code_location.h"
#include "sen/core/meta/var.h"

// std
#include <string>
#include <vector>

namespace sen::lang
{

/// \addtogroup lang
/// @{

/// Supported tokens.
enum class StlTokenType
{
  // single characters
  leftParen,
  rightParen,
  leftBrace,
  rightBrace,
  leftBracket,
  rightBracket,
  comma,
  dot,
  minus,
  plus,
  colon,
  semicolon,
  slash,
  star,
  at,

  // one or two characters
  bang,
  bangEqual,
  equal,
  equalEqual,
  greater,
  greaterEqual,
  less,
  lessEqual,

  // literals
  identifier,
  string,
  real,
  integral,

  // keywords
  keywordAbstract,
  keywordAnd,
  keywordClass,
  keywordInterface,
  keywordExtends,
  keywordImplements,
  keywordStruct,
  keywordVariant,
  keywordFalse,
  keywordFunction,
  keywordEvent,
  keywordVar,
  keywordOr,
  keywordNot,
  keywordTrue,
  keywordImport,
  keywordOptional,
  keywordPackage,
  keywordEnum,
  keywordSequence,
  keywordArray,
  keywordQuantity,
  keywordAlias,
  keywordSelect,
  keywordWhere,
  keywordFrom,
  keywordIn,
  keywordBetween,

  // others
  comment,
  endOfFile,
};

struct StlToken
{
  SEN_COPY_CONSTRUCT(StlToken) = default;
  SEN_COPY_ASSIGN(StlToken) = default;
  SEN_MOVE_CONSTRUCT(StlToken) = default;
  SEN_MOVE_ASSIGN(StlToken) = default;

  StlToken() = default;
  StlToken(StlTokenType type, std::string lexeme, Var value, CodeLocation loc)
    : type(std::move(type)), lexeme(std::move(lexeme)), value(std::move(value)), loc(std::move(loc))
  {
  }
  ~StlToken() = default;

  StlTokenType type = {};   // NOLINT(misc-non-private-member-variables-in-classes)
  std::string lexeme = {};  // NOLINT(misc-non-private-member-variables-in-classes)
  Var value = {};           // NOLINT(misc-non-private-member-variables-in-classes)
  CodeLocation loc = {};    // NOLINT(misc-non-private-member-variables-in-classes)
};

std::string toString(const StlToken& token);

std::string toString(StlTokenType type);

using StlTokenList = std::vector<StlToken>;

/// @}

}  // namespace sen::lang

#endif  // SEN_CORE_LANG_STL_TOKEN_H
