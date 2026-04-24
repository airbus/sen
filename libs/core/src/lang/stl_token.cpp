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
#include <string_view>

namespace sen::lang
{

std::string toString(const StlToken& token)
{
  switch (token.type())
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
      return std::string("identifier('") + token.lexeme() + "')";
    case StlTokenType::string:
      return std::string("string('") + token.lexeme() + "')";
    case StlTokenType::real:
      return std::string("real(") + std::to_string(getCopyAs<float64_t>(token.value())) + ")";
    case StlTokenType::integral:
      return std::string("integral(") + std::to_string(getCopyAs<int64_t>(token.value())) + ")";
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
      return std::string("comment('") + token.lexeme() + "')";
    case StlTokenType::endOfFile:
      return "endOfFile";
    default:
      return "unknown";
  }
}

std::string toString(const StlTokenType type) { return toString(StlToken {type, {}, {}, {}}); }

// NOLINTNEXTLINE(readability-function-size)
StlTokenType fromString(const std::string& stlTokenType)
{
  auto startsWith = [](const std::string_view ref, const std::string_view cmp) -> bool
  { return ref.compare(0, cmp.length(), cmp) == 0; };

  if (stlTokenType == "leftParen")
  {
    return StlTokenType::leftParen;
  }
  if (stlTokenType == "rightParen")
  {
    return StlTokenType::rightParen;
  }
  if (stlTokenType == "leftBrace")
  {
    return StlTokenType::leftBrace;
  }
  if (stlTokenType == "rightBrace")
  {
    return StlTokenType::rightBrace;
  }
  if (stlTokenType == "leftBracket")
  {
    return StlTokenType::leftBracket;
  }
  if (stlTokenType == "rightBracket")
  {
    return StlTokenType::rightBracket;
  }
  if (stlTokenType == "comma")
  {
    return StlTokenType::comma;
  }
  if (stlTokenType == "dot")
  {
    return StlTokenType::dot;
  }
  if (stlTokenType == "minus")
  {
    return StlTokenType::minus;
  }
  if (stlTokenType == "plus")
  {
    return StlTokenType::plus;
  }
  if (stlTokenType == "colon")
  {
    return StlTokenType::colon;
  }
  if (stlTokenType == "semicolon")
  {
    return StlTokenType::semicolon;
  }
  if (stlTokenType == "slash")
  {
    return StlTokenType::slash;
  }
  if (stlTokenType == "star")
  {
    return StlTokenType::star;
  }
  if (stlTokenType == "at")
  {
    return StlTokenType::at;
  }

  if (stlTokenType == "bang")
  {
    return StlTokenType::bang;
  }
  if (stlTokenType == "bangEqual")
  {
    return StlTokenType::bangEqual;
  }
  if (stlTokenType == "equal")
  {
    return StlTokenType::equal;
  }
  if (stlTokenType == "equalEqual")
  {
    return StlTokenType::equalEqual;
  }
  if (stlTokenType == "greater")
  {
    return StlTokenType::greater;
  }
  if (stlTokenType == "greaterEqual")
  {
    return StlTokenType::greaterEqual;
  }
  if (stlTokenType == "less")
  {
    return StlTokenType::less;
  }
  if (stlTokenType == "lessEqual")
  {
    return StlTokenType::lessEqual;
  }

  if (startsWith(stlTokenType, "identifier"))
  {
    return StlTokenType::identifier;
  }
  if (startsWith(stlTokenType, "string"))
  {
    return StlTokenType::string;
  }
  if (startsWith(stlTokenType, "real"))
  {
    return StlTokenType::real;
  }
  if (startsWith(stlTokenType, "integral"))
  {
    return StlTokenType::integral;
  }

  if (stlTokenType == "keywordAbstract")
  {
    return StlTokenType::keywordAbstract;
  }
  if (stlTokenType == "keywordAnd")
  {
    return StlTokenType::keywordAnd;
  }
  if (stlTokenType == "keywordClass")
  {
    return StlTokenType::keywordClass;
  }
  if (stlTokenType == "keywordInterface")
  {
    return StlTokenType::keywordInterface;
  }
  if (stlTokenType == "keywordExtends")
  {
    return StlTokenType::keywordExtends;
  }
  if (stlTokenType == "keywordImplements")
  {
    return StlTokenType::keywordImplements;
  }
  if (stlTokenType == "keywordStruct")
  {
    return StlTokenType::keywordStruct;
  }
  if (stlTokenType == "keywordVariant")
  {
    return StlTokenType::keywordVariant;
  }
  if (stlTokenType == "keywordFalse")
  {
    return StlTokenType::keywordFalse;
  }
  if (stlTokenType == "keywordFunction")
  {
    return StlTokenType::keywordFunction;
  }
  if (stlTokenType == "keywordEvent")
  {
    return StlTokenType::keywordEvent;
  }
  if (stlTokenType == "keywordVar")
  {
    return StlTokenType::keywordVar;
  }
  if (stlTokenType == "keywordOr")
  {
    return StlTokenType::keywordOr;
  }
  if (stlTokenType == "keywordNot")
  {
    return StlTokenType::keywordNot;
  }
  if (stlTokenType == "keywordTrue")
  {
    return StlTokenType::keywordTrue;
  }
  if (stlTokenType == "keywordImport")
  {
    return StlTokenType::keywordImport;
  }
  if (stlTokenType == "keywordOptional")
  {
    return StlTokenType::keywordOptional;
  }
  if (stlTokenType == "keywordPackage")
  {
    return StlTokenType::keywordPackage;
  }
  if (stlTokenType == "keywordEnum")
  {
    return StlTokenType::keywordEnum;
  }
  if (stlTokenType == "keywordSequence")
  {
    return StlTokenType::keywordSequence;
  }
  if (stlTokenType == "keywordArray")
  {
    return StlTokenType::keywordArray;
  }
  if (stlTokenType == "keywordQuantity")
  {
    return StlTokenType::keywordQuantity;
  }
  if (stlTokenType == "keywordAlias")
  {
    return StlTokenType::keywordAlias;
  }
  if (stlTokenType == "keywordSelect")
  {
    return StlTokenType::keywordSelect;
  }
  if (stlTokenType == "keywordWhere")
  {
    return StlTokenType::keywordWhere;
  }
  if (stlTokenType == "keywordFrom")
  {
    return StlTokenType::keywordFrom;
  }
  if (stlTokenType == "keywordIn")
  {
    return StlTokenType::keywordIn;
  }
  if (stlTokenType == "keywordBetween")
  {
    return StlTokenType::keywordBetween;
  }

  if (startsWith(stlTokenType, "comment"))
  {
    return StlTokenType::comment;
  }
  if (stlTokenType == "endOfFile")
  {
    return StlTokenType::endOfFile;
  }
  if (stlTokenType == "unknown")
  {
    return StlTokenType::unknown;
  }

  return StlTokenType::unknown;
}

}  // namespace sen::lang
