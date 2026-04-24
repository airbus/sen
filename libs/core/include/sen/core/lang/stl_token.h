// === stl_token.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_LANG_STL_TOKEN_H
#define SEN_CORE_LANG_STL_TOKEN_H

// sen
#include "sen/core/lang/code_location.h"
#include "sen/core/meta/var.h"

// std
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace sen::lang
{

/// \addtogroup lang
/// @{

/// Supported tokens.
enum class StlTokenType : uint8_t
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

  // in case of error
  unknown,
};

class StlToken
{
public:
  StlToken() = default;
  StlToken(const StlTokenType type, std::string lexeme, Var value, CodeLocation codeLocation)
    : type_ {type}, lexeme_ {std::move(lexeme)}, value_ {std::move(value)}, codeLocation_ {std::move(codeLocation)}
  {
  }

public:
  [[nodiscard]] StlTokenType type() const { return type_; }

  [[nodiscard]] std::string lexeme() const { return lexeme_; }

  [[nodiscard]] Var value() const { return value_; }

  [[nodiscard]] CodeLocation codeLocation() const { return codeLocation_; }

private:
  StlTokenType type_ {StlTokenType::unknown};
  std::string lexeme_;
  Var value_;
  CodeLocation codeLocation_ {};
};

std::string toString(const StlToken& token);

std::string toString(StlTokenType type);

StlTokenType fromString(const std::string& stlTokenType);

using StlTokenList = std::vector<StlToken>;

/// @}

}  // namespace sen::lang

#endif  // SEN_CORE_LANG_STL_TOKEN_H
