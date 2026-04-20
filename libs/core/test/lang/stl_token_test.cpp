// === stl_token_test.cpp ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/lang/code_location.h"
#include "sen/core/lang/stl_token.h"
#include "sen/core/meta/var.h"

// google test
#include <gtest/gtest.h>

// std
#include <array>
#include <cstddef>
#include <string_view>

namespace
{
TEST(AnStlToken, IsInAConsistentStateWhenDefaultConstructed)
{
  // arrange + act
  const sen::lang::StlToken token {};

  // assert
  ASSERT_EQ(token.type(), sen::lang::StlTokenType::unknown);
  ASSERT_TRUE(token.lexeme().empty());
  ASSERT_TRUE(token.value().isEmpty());
  ASSERT_EQ(token.codeLocation().offset, 0);
  ASSERT_EQ(token.codeLocation().src, nullptr);
}

TEST(AnStlToken, ProvidesConsistentViewOfItsMembers)
{
  // arrange
  constexpr auto tokenType {sen::lang::StlTokenType::keywordPackage};
  constexpr std::string_view lexeme {"string"};
  const sen::Var tokenValue {"package"};
  constexpr std::string_view sourceCode {"package test.package;"};
  constexpr size_t offset {0};
  constexpr sen::lang::CodeLocation codeLocation {sourceCode.data(), offset};

  // act
  const sen::lang::StlToken token {tokenType, std::string {lexeme}, tokenValue, codeLocation};

  // assert
  ASSERT_EQ(token.type(), tokenType);
  ASSERT_EQ(token.lexeme(), lexeme);
  ASSERT_EQ(token.value(), tokenValue);
  ASSERT_EQ(token.codeLocation().src, sourceCode.data());
  ASSERT_EQ(token.codeLocation().offset, offset);
}

TEST(AnStlToken, IsConvertibleToString)
{
  // arrange
  using StlTokenType = sen::lang::StlTokenType;
  constexpr StlTokenType invalidTokenType {255};
  constexpr std::array stlTokenTypes {StlTokenType::leftParen,
                                      StlTokenType::rightParen,
                                      StlTokenType::leftBrace,
                                      StlTokenType::rightBrace,
                                      StlTokenType::leftBracket,
                                      StlTokenType::rightBracket,
                                      StlTokenType::comma,
                                      StlTokenType::dot,
                                      StlTokenType::minus,
                                      StlTokenType::plus,
                                      StlTokenType::colon,
                                      StlTokenType::semicolon,
                                      StlTokenType::slash,
                                      StlTokenType::star,
                                      StlTokenType::at,

                                      StlTokenType::bang,
                                      StlTokenType::bangEqual,
                                      StlTokenType::equal,
                                      StlTokenType::equalEqual,
                                      StlTokenType::greater,
                                      StlTokenType::greaterEqual,
                                      StlTokenType::less,
                                      StlTokenType::lessEqual,

                                      StlTokenType::identifier,
                                      StlTokenType::string,
                                      StlTokenType::real,
                                      StlTokenType::integral,

                                      StlTokenType::keywordAbstract,
                                      StlTokenType::keywordAnd,
                                      StlTokenType::keywordClass,
                                      StlTokenType::keywordInterface,
                                      StlTokenType::keywordExtends,
                                      StlTokenType::keywordImplements,
                                      StlTokenType::keywordStruct,
                                      StlTokenType::keywordVariant,
                                      StlTokenType::keywordFalse,
                                      StlTokenType::keywordFunction,
                                      StlTokenType::keywordEvent,
                                      StlTokenType::keywordVar,
                                      StlTokenType::keywordOr,
                                      StlTokenType::keywordNot,
                                      StlTokenType::keywordTrue,
                                      StlTokenType::keywordImport,
                                      StlTokenType::keywordOptional,
                                      StlTokenType::keywordPackage,
                                      StlTokenType::keywordEnum,
                                      StlTokenType::keywordSequence,
                                      StlTokenType::keywordArray,
                                      StlTokenType::keywordQuantity,
                                      StlTokenType::keywordAlias,
                                      StlTokenType::keywordSelect,
                                      StlTokenType::keywordWhere,
                                      StlTokenType::keywordFrom,
                                      StlTokenType::keywordIn,
                                      StlTokenType::keywordBetween,

                                      StlTokenType::comment,
                                      StlTokenType::endOfFile,
                                      StlTokenType::unknown};

  // act + assert
  for (const auto& tokenType: stlTokenTypes)
  {
    const auto tokenTypeAsString {sen::lang::toString(tokenType)};
    ASSERT_EQ(tokenType, sen::lang::fromString(tokenTypeAsString));
  }
  const auto tokenTypeAsString {sen::lang::toString(invalidTokenType)};
  ASSERT_EQ(StlTokenType::unknown, sen::lang::fromString(tokenTypeAsString));
}

}  // namespace
