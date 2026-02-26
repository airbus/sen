// === stl_scanner.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/lang/stl_scanner.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/lang/error_reporter.h"
#include "sen/core/lang/stl_token.h"
#include "sen/core/meta/var.h"

// std
#include <cstdint>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unordered_map>

namespace sen::lang
{

namespace
{
[[nodiscard]] constexpr bool isDigit(char c) noexcept { return c >= '0' && c <= '9'; }

[[nodiscard]] constexpr bool isAlpha(char c) noexcept
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c == '_');
}

[[nodiscard]] constexpr bool isAlphaNumeric(char c) noexcept { return isDigit(c) || isAlpha(c); }

}  // namespace

StlScanner::StlScanner(const std::string& program): program_(program), current_({program.c_str(), 0U}) {}

const StlTokenList& StlScanner::scanTokens()
{
  tokens_.clear();

  while (!isAtEnd())
  {
    start_ = current_.offset;
    scanToken();
  }

  const StlToken eofToken {StlTokenType::endOfFile, {}, {}, current_};
  tokens_.push_back(eofToken);
  return tokens_;
}

bool StlScanner::isAtEnd() const { return (current_.offset >= program_.size()); }

void StlScanner::scanToken()
{
  const auto c = advance();

  switch (c)
  {
      // single character lexemes
    case '(':
      addToken(StlTokenType::leftParen, {});
      break;
    case ')':
      addToken(StlTokenType::rightParen, {});
      break;
    case '{':
      addToken(StlTokenType::leftBrace, {});
      break;
    case '}':
      addToken(StlTokenType::rightBrace, {});
      break;
    case '[':
      addToken(StlTokenType::leftBracket, {});
      break;
    case ']':
      addToken(StlTokenType::rightBracket, {});
      break;
    case ',':
      addToken(StlTokenType::comma, {});
      break;
    case '.':
      addToken(StlTokenType::dot, {});
      break;
    case '-':
    {
      if (isDigit(peek()))
      {
        number(Sign::negative);
      }
      else
      {
        addToken(StlTokenType::minus, {});
      }
    }
    break;
    case '+':
      addToken(StlTokenType::plus, {});
      break;
    case ';':
      addToken(StlTokenType::semicolon, {});
      break;
    case ':':
      addToken(StlTokenType::colon, {});
      break;
    case '*':
      addToken(StlTokenType::star, {});
      break;
    case '@':
      addToken(StlTokenType::at, {});
      break;

      // two character lexemes
    case '!':
    {
      if (match('='))
      {
        addToken(StlTokenType::bangEqual, {});
      }
      else
      {
        addToken(StlTokenType::bang, {});
      }
      break;
    }
    case '=':
    {
      if (match('='))
      {
        addToken(StlTokenType::equalEqual, {});
      }
      else
      {
        addToken(StlTokenType::equal, {});
      }
      break;
    }
    case '<':
    {
      if (match('='))
      {
        addToken(StlTokenType::lessEqual, {});
      }
      else
      {
        addToken(StlTokenType::less, {});
      }
      break;
    }
    case '>':
    {
      if (match('='))
      {
        addToken(StlTokenType::greaterEqual, {});
      }
      else
      {
        addToken(StlTokenType::greater, {});
      }
      break;
    }

    // longer lexemes
    case '/':
    {
      if (match('/'))
      {
        // comments starting with '--' or with ' --' are considered rulers/separators and ignored
        if ((match('-') && peek() == '-') || (match(' ') && peek() == '-' && peekNext() == '-'))
        {
          ruler();
        }
        else
        {
          comment();
        }
      }
      else
      {
        addToken(StlTokenType::slash, {});
      }
      break;
    }

    // whitespaces
    case ' ':
      [[fallthrough]];
    case '\r':
      [[fallthrough]];
    case '\t':
      [[fallthrough]];
    case '\n':
      break;

    case '"':
      string('"');
      break;

    case '\'':
      string('\'');
      break;

    default:
    {
      if (isDigit(c))
      {
        number(Sign::positive);
      }
      else if (isAlpha(c))
      {
        identifier();
      }
      else
      {
        error("unexpected character");
      }
    }
    break;
  }
}

char StlScanner::advance()
{
  auto result = program_.at(current_.offset);
  current_.offset++;
  return result;
}

void StlScanner::addToken(StlTokenType type, const Var& value)
{
  using LexemeMap = std::unordered_map<StlTokenType, std::string>;

  static LexemeMap lexemeMap = {{StlTokenType::keywordFalse, "false"},
                                {StlTokenType::keywordTrue, "true"},
                                {StlTokenType::keywordFunction, "fn"},
                                {StlTokenType::keywordEvent, "event"},
                                {StlTokenType::keywordVar, "var"},
                                {StlTokenType::keywordAbstract, "abstract"},
                                {StlTokenType::keywordClass, "class"},
                                {StlTokenType::keywordInterface, "interface"},
                                {StlTokenType::keywordExtends, "extends"},
                                {StlTokenType::keywordImplements, "implements"},
                                {StlTokenType::keywordStruct, "struct"},
                                {StlTokenType::keywordVariant, "variant"},
                                {StlTokenType::keywordImport, "import"},
                                {StlTokenType::keywordPackage, "package"},
                                {StlTokenType::keywordEnum, "enum"},
                                {StlTokenType::keywordSequence, "sequence"},
                                {StlTokenType::keywordArray, "array"},
                                {StlTokenType::keywordQuantity, "quantity"},
                                {StlTokenType::keywordAlias, "alias"},
                                {StlTokenType::keywordOptional, "optional"},
                                {StlTokenType::keywordAnd, "AND"},
                                {StlTokenType::keywordOr, "OR"},
                                {StlTokenType::keywordNot, "NOT"},
                                {StlTokenType::keywordSelect, "SELECT"},
                                {StlTokenType::keywordWhere, "WHERE"},
                                {StlTokenType::keywordFrom, "FROM"},
                                {StlTokenType::keywordIn, "IN"},
                                {StlTokenType::keywordBetween, "BETWEEN"},
                                {StlTokenType::leftParen, "("},
                                {StlTokenType::rightParen, ")"},
                                {StlTokenType::leftBrace, "{"},
                                {StlTokenType::rightBrace, "}"},
                                {StlTokenType::leftBracket, "["},
                                {StlTokenType::rightBracket, "]"},
                                {StlTokenType::comma, ","},
                                {StlTokenType::dot, "."},
                                {StlTokenType::colon, ":"},
                                {StlTokenType::semicolon, ";"},
                                {StlTokenType::minus, "-"},
                                {StlTokenType::plus, "+"},
                                {StlTokenType::star, "*"},
                                {StlTokenType::at, "@"},
                                {StlTokenType::slash, "/"},
                                {StlTokenType::bang, "!"},
                                {StlTokenType::bangEqual, "!="},
                                {StlTokenType::equal, "="},
                                {StlTokenType::equalEqual, "=="},
                                {StlTokenType::less, "<"},
                                {StlTokenType::lessEqual, "<="},
                                {StlTokenType::greater, ">"},
                                {StlTokenType::greaterEqual, ">="}};

  const auto& lexeme = lexemeMap.find(type);
  if (lexeme == lexemeMap.end())
  {
    std::string err;
    err = "unknown token type ";
    err.append("'");
    err.append(toString(type));
    err.append("'");
    throw std::logic_error(err);
  }

  tokens_.emplace_back(type, lexeme->second, value, current_);
}

void StlScanner::error(const std::string& message) const { ErrorReporter::report({}, message); }

bool StlScanner::match(char expected)
{
  if (isAtEnd())
  {
    return false;
  }

  if (program_.at(current_.offset) != expected)
  {
    return false;
  }

  current_.offset++;
  return true;
}

char StlScanner::peek() const
{
  if (isAtEnd())
  {
    return '\0';
  }

  return program_.at(current_.offset);
}

char StlScanner::peekNext() const
{
  if (current_.offset + 1U >= program_.size())
  {
    return '\0';
  }

  return program_.at(current_.offset + 1U);
}

void StlScanner::string(char endingChar)
{
  while (peek() != endingChar && !isAtEnd())
  {
    std::ignore = advance();
  }

  if (isAtEnd())
  {
    error("Unterminated string.");
    return;
  }

  std::ignore = advance();  // the closing "

  // trim the surrounding quotes.
  const auto lexeme = program_.substr(start_ + 1U, current_.offset - start_ - 2U);
  tokens_.emplace_back(StlTokenType::string, lexeme, lexeme, current_);
}

void StlScanner::comment()
{
  // comments go until end of line
  while (!isAtEnd() && peek() != '\n' && peek() != '\r')
  {
    std::ignore = advance();
  }

  // trim the initial //
  const auto lexeme = program_.substr(start_ + 2U, current_.offset - start_ - 2U);
  const auto firstChar = lexeme.find_first_not_of(" \t\n\r");
  const auto trimmedLexeme = (firstChar == std::string::npos) ? "" : lexeme.substr(firstChar);
  tokens_.emplace_back(StlTokenType::comment, trimmedLexeme, Var {}, current_);
}

void StlScanner::ruler()
{
  // rulers go until end of line
  while (!isAtEnd() && peek() != '\n' && peek() != '\r')
  {
    std::ignore = advance();
  }
}

void StlScanner::number(Sign sign)
{
  // ignore all possible whitespaces before number
  while (peek() == ' ')
  {
    std::ignore = advance();
  }

  // move past all digits
  while (isDigit(peek()))
  {
    std::ignore = advance();
  }

  // look for a fractional part.
  if (peek() == '.' && isDigit(peekNext()))
  {
    std::ignore = advance();  // consume the "."

    // move past all remaining digits
    while (isDigit(peek()))
    {
      std::ignore = advance();
    }

    // take the numeric value out of the lexeme (ignoring leading spaces and signs)
    auto lexeme = formatNumber(program_.substr(start_, current_.offset - start_));

    double value = std::stod(lexeme);
    if (sign == Sign::negative)
    {
      value = -value;
      lexeme.insert(0, "-");
    }

    tokens_.emplace_back(StlTokenType::real, lexeme, value, current_);
  }
  else
  {
    // take the numeric value out of the lexeme (ignoring leading spaces and signs)
    auto lexeme = formatNumber(program_.substr(start_, current_.offset - start_));

    int64_t value = std::stoll(lexeme);
    if (sign == Sign::negative)
    {
      value = -value;
      lexeme.insert(0, "-");
    }

    tokens_.emplace_back(StlTokenType::integral, lexeme, value, current_);
  }
}

void StlScanner::identifier()
{
  // advance to the end of the identifier
  while (isAlphaNumeric(peek()))
  {
    std::ignore = advance();
  }

  tokens_.push_back(identifierToken());
}

StlToken StlScanner::identifierToken() const
{
  const auto& lexeme = program_.substr(start_, current_.offset - start_);

  if (lexeme == "true")
  {
    return {StlTokenType::keywordTrue, lexeme, true, current_};
  }

  if (lexeme == "false")
  {
    return {StlTokenType::keywordFalse, lexeme, false, current_};
  }

  using IdentifierMap = std::unordered_map<std::string, StlTokenType>;
  static IdentifierMap identifierMap = {{"AND", StlTokenType::keywordAnd},
                                        {"OR", StlTokenType::keywordOr},
                                        {"fn", StlTokenType::keywordFunction},
                                        {"event", StlTokenType::keywordEvent},
                                        {"var", StlTokenType::keywordVar},
                                        {"abstract", StlTokenType::keywordAbstract},
                                        {"class", StlTokenType::keywordClass},
                                        {"interface", StlTokenType::keywordInterface},
                                        {"extends", StlTokenType::keywordExtends},
                                        {"implements", StlTokenType::keywordImplements},
                                        {"struct", StlTokenType::keywordStruct},
                                        {"variant", StlTokenType::keywordVariant},
                                        {"optional", StlTokenType::keywordOptional},
                                        {"import", StlTokenType::keywordImport},
                                        {"package", StlTokenType::keywordPackage},
                                        {"enum", StlTokenType::keywordEnum},
                                        {"sequence", StlTokenType::keywordSequence},
                                        {"array", StlTokenType::keywordArray},
                                        {"quantity", StlTokenType::keywordQuantity},
                                        {"alias", StlTokenType::keywordAlias},
                                        {"SELECT", StlTokenType::keywordSelect},
                                        {"FROM", StlTokenType::keywordFrom},
                                        {"WHERE", StlTokenType::keywordWhere},
                                        {"BETWEEN", StlTokenType::keywordBetween},
                                        {"IN", StlTokenType::keywordIn},
                                        {"NOT", StlTokenType::keywordNot}};

  auto itr = identifierMap.find(lexeme);
  const auto tokenType = (itr == identifierMap.end()) ? StlTokenType::identifier : itr->second;
  return {tokenType, lexeme, lexeme, current_};
}

std::string StlScanner::formatNumber(const std::string& lexeme)
{
  if (auto pos = lexeme.find_first_of("1234567890"); pos != std::string::npos)
  {
    std::string newLexeme = lexeme.substr(pos);
    return newLexeme;
  }

  std::string err;
  err.append(lexeme);
  err.append(" is not a valid number");
  throwRuntimeError(err);
}

}  // namespace sen::lang
