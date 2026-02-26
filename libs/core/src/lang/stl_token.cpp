// === stl_token.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/lang/stl_token.h"

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/meta/var.h"

// std
#include <cstdint>
#include <string>

namespace sen::lang
{

std::string toString(const StlToken& token)
{
  switch (token.type)  // NOSONAR
  {
    case StlTokenType::leftParen:
      return "leftParen";
    case StlTokenType::rightParen:
      return "rightParen";
    case StlTokenType::leftBrace:
      return "leftBrace";
    case StlTokenType::rightBrace:
      return "rightBrace";
    case StlTokenType::leftBracket:
      return "leftBracket";
    case StlTokenType::rightBracket:
      return "rightBracket";
    case StlTokenType::comma:
      return "comma";
    case StlTokenType::dot:
      return "dot";
    case StlTokenType::minus:
      return "minus";
    case StlTokenType::plus:
      return "plus";
    case StlTokenType::colon:
      return "colon";
    case StlTokenType::semicolon:
      return "semicolon";
    case StlTokenType::slash:
      return "slash";
    case StlTokenType::star:
      return "star";
    case StlTokenType::at:
      return "at";
    case StlTokenType::bang:
      return "bang";
    case StlTokenType::bangEqual:
      return "bangEqual";
    case StlTokenType::equal:
      return "equal";
    case StlTokenType::equalEqual:
      return "equalEqual";
    case StlTokenType::greater:
      return "greater";
    case StlTokenType::greaterEqual:
      return "greaterEqual";
    case StlTokenType::less:
      return "less";
    case StlTokenType::lessEqual:
      return "lessEqual";
    case StlTokenType::identifier:
      return std::string("identifier('") + token.lexeme + "')";
    case StlTokenType::string:
      return std::string("string('") + token.lexeme + "')";
    case StlTokenType::real:
      return std::string("real(") + std::to_string(getCopyAs<float64_t>(token.value)) + ")";
    case StlTokenType::integral:
      return std::string("integral(") + std::to_string(getCopyAs<int64_t>(token.value)) + ")";
    case StlTokenType::keywordAnd:
      return "keywordAnd";
    case StlTokenType::keywordAbstract:
      return "keywordAbstract";
    case StlTokenType::keywordClass:
      return "keywordClass";
    case StlTokenType::keywordInterface:
      return "keywordInterface";
    case StlTokenType::keywordExtends:
      return "keywordExtends";
    case StlTokenType::keywordImplements:
      return "keywordImplements";
    case StlTokenType::keywordStruct:
      return "keywordStruct";
    case StlTokenType::keywordVariant:
      return "keywordVariant";
    case StlTokenType::keywordFalse:
      return "keywordFalse";
    case StlTokenType::keywordFunction:
      return "keywordFunction";
    case StlTokenType::keywordEvent:
      return "keywordEvent";
    case StlTokenType::keywordVar:
      return "keywordVar";
    case StlTokenType::keywordImport:
      return "keywordImport";
    case StlTokenType::keywordOptional:
      return "keywordOptional";
    case StlTokenType::keywordPackage:
      return "keywordPackage";
    case StlTokenType::keywordOr:
      return "keywordOr";
    case StlTokenType::keywordNot:
      return "keywordNot";
    case StlTokenType::keywordTrue:
      return "keywordTrue";
    case StlTokenType::keywordEnum:
      return "keywordEnum";
    case StlTokenType::keywordSequence:
      return "keywordSequence";
    case StlTokenType::keywordArray:
      return "keywordArray";
    case StlTokenType::keywordQuantity:
      return "keywordQuantity";
    case StlTokenType::keywordAlias:
      return "keywordAlias";
    case StlTokenType::keywordSelect:
      return "keywordSelect";
    case StlTokenType::keywordWhere:
      return "keywordWhere";
    case StlTokenType::keywordFrom:
      return "keywordFrom";
    case StlTokenType::keywordIn:
      return "keywordIn";
    case StlTokenType::keywordBetween:
      return "keywordBetween";
    case StlTokenType::comment:
      return std::string("comment('") + token.lexeme + "')";
    case StlTokenType::endOfFile:
      return "endOfFile";
  }

  return {};
}

std::string toString(StlTokenType type)
{
  StlToken token;
  token.type = type;
  return toString(token);
}

}  // namespace sen::lang
