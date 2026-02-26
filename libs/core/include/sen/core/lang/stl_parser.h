// === stl_parser.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_LANG_STL_PARSER_H
#define SEN_CORE_LANG_STL_PARSER_H

// sen
#include "sen/core/lang/stl_expression.h"
#include "sen/core/lang/stl_statement.h"
#include "sen/core/lang/stl_token.h"

// std
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace sen::lang
{

/// \addtogroup lang
/// @{

/// Parses STL types and queries out of a list of tokens.
class StlParser
{
public:
  /// Stores the tokens for eventual parsing.
  explicit StlParser(const StlTokenList& tokens);

  struct ParseError: public std::runtime_error
  {
    explicit ParseError(const std::string& msg): std::runtime_error(msg) {}
  };

  /// Parse the tokens given in the constructor into a set of statements.
  [[nodiscard]] std::vector<StlStatement> parse();

  /// Parse the tokens as a query statement.
  [[nodiscard]] QueryStatement parseQuery();

private:  // declarations
  [[nodiscard]] StlStatement declaration();
  [[nodiscard]] StlImportStatement importDeclaration();
  [[nodiscard]] StlPackageStatement packageDeclaration();
  [[nodiscard]] StlTypeNameStatement typeNameStatement(const std::vector<std::string>& messages);
  [[nodiscard]] StlStructStatement structDeclaration();
  [[nodiscard]] StlStructFieldStatement structFieldStatement();
  [[nodiscard]] StlEnumStatement enumDeclaration();
  [[nodiscard]] StlEnumeratorStatement enumeratorDeclaration();
  [[nodiscard]] StlVariantStatement variantDeclaration();
  [[nodiscard]] StlVariantElement variantElementDeclaration();
  [[nodiscard]] StlSequenceStatement sequenceStatement();
  [[nodiscard]] StlArrayStatement arrayStatement();
  [[nodiscard]] StlQuantityStatement quantityStatement();
  [[nodiscard]] StlTypeAliasStatement aliasStatement();
  [[nodiscard]] StlOptionalTypeStatement optionalTypeStatement();
  [[nodiscard]] StlClassStatement classStatement(bool isAbstract);
  [[nodiscard]] StlInterfaceStatement interfaceStatement();
  [[nodiscard]] StlVarStatement varStatement();
  [[nodiscard]] StlFunctionStatement functionStatement();
  [[nodiscard]] StlEventStatement eventStatement();
  [[nodiscard]] StlArgStatement argStatement();
  [[nodiscard]] std::optional<StlAttributeList> maybeAttributeList(const std::string& subjectName);
  [[nodiscard]] StlAttributeList attributeList(const std::string& subjectName);
  [[nodiscard]] BusNameStatement busNameStatement();
  [[nodiscard]] std::shared_ptr<StlExpr> expression();
  [[nodiscard]] std::shared_ptr<StlExpr> logicOr();
  [[nodiscard]] std::shared_ptr<StlExpr> logicAnd();
  [[nodiscard]] std::shared_ptr<StlExpr> equality();
  [[nodiscard]] std::shared_ptr<StlExpr> comparison();
  [[nodiscard]] std::shared_ptr<StlExpr> term();
  [[nodiscard]] std::shared_ptr<StlExpr> factor();
  [[nodiscard]] std::shared_ptr<StlExpr> unary();
  [[nodiscard]] std::shared_ptr<StlExpr> primary();
  [[nodiscard]] std::shared_ptr<StlExpr> variable();

private:  // expressions
  [[nodiscard]] StlToken literal();

private:  // utility
  [[nodiscard]] bool match(const std::vector<StlTokenType>& types);
  [[nodiscard]] bool check(StlTokenType type) const;
  [[nodiscard]] const StlToken& peek() const;
  [[nodiscard]] StlToken peekNext() const;
  [[nodiscard]] const StlToken& previous() const;
  [[nodiscard]] bool isAtEnd() const noexcept;
  [[nodiscard]] const StlToken& advance();
  [[nodiscard]] const StlToken& consume(StlTokenType type, const std::vector<std::string>& message);
  [[nodiscard]] ParseError error(const StlToken& token, const std::string& message) const;
  void checkNotReserved(const StlToken& token);

private:  // others
  void classOrInterfaceMembers(StlInterfaceStatement& statement);
  void classParents(StlClassStatement& statement);
  void validateComments(const StlToken& functionOrEventIdentifier, const std::vector<StlArgStatement>& arguments);
  [[nodiscard]] bool containsNewLine(size_t startOffset, size_t endOffset) const;

private:
  const StlTokenList& tokens_;
  std::size_t current_ = 0U;
  std::vector<StlToken> previousComment_;
};

/// @}

}  // namespace sen::lang

#endif  // SEN_CORE_LANG_STL_PARSER_H
