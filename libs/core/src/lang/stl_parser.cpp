// === stl_parser.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/lang/stl_parser.h"

// sen
#include "sen/core/lang/error_reporter.h"
#include "sen/core/lang/stl_expression.h"
#include "sen/core/lang/stl_statement.h"
#include "sen/core/lang/stl_token.h"

// std
#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace sen::lang
{

StlParser::StlParser(const StlTokenList& tokens): tokens_(tokens) {}

std::vector<StlStatement> StlParser::parse()
{
  std::vector<StlStatement> statements;
  while (!isAtEnd())
  {
    statements.push_back(declaration());
  }

  return statements;
}

StlStatement StlParser::declaration()
{
  // allow comments before declarations
  // and use them as descriptions.
  previousComment_ = {};
  while (check(StlTokenType::comment))
  {
    previousComment_.push_back(consume(StlTokenType::comment, {"expecting comment"}));
  }

  if (match({StlTokenType::keywordImport}))
  {
    return importDeclaration();
  }

  if (match({StlTokenType::keywordPackage}))
  {
    return packageDeclaration();
  }

  if (match({StlTokenType::keywordStruct}))
  {
    return structDeclaration();
  }

  if (match({StlTokenType::keywordEnum}))
  {
    return enumDeclaration();
  }

  if (match({StlTokenType::keywordVariant}))
  {
    return variantDeclaration();
  }

  if (match({StlTokenType::keywordSequence}))
  {
    return sequenceStatement();
  }

  if (match({StlTokenType::keywordArray}))
  {
    return arrayStatement();
  }

  if (match({StlTokenType::keywordQuantity}))
  {
    return quantityStatement();
  }

  if (match({StlTokenType::keywordAlias}))
  {
    return aliasStatement();
  }

  if (match({StlTokenType::keywordOptional}))
  {
    return optionalTypeStatement();
  }

  if (match({StlTokenType::keywordClass}))
  {
    return classStatement(false);
  }

  if (match({StlTokenType::keywordAbstract}))
  {
    std::ignore = consume(StlTokenType::keywordClass, {"Expected class keyword"});
    return classStatement(true);
  }

  if (match({StlTokenType::keywordInterface}))
  {
    return interfaceStatement();
  }

  throw error(peek(), "Expected statement");
}

StlImportStatement StlParser::importDeclaration()
{
  // import -> IMPORT STRING

  StlImportStatement statement;
  statement.fileName = consume(StlTokenType::string, {"Expected file name to import"});
  return statement;
}

StlPackageStatement StlParser::packageDeclaration()
{
  // package -> PACKAGE identifier ('.' identifier)* ';'

  StlPackageStatement statement;
  statement.path.push_back(consume(StlTokenType::identifier, {"Expected package identifier"}));

  while (match({StlTokenType::dot}))
  {
    statement.path.push_back(consume(StlTokenType::identifier, {"Expected package identifier"}));
  }

  std::ignore = consume(StlTokenType::semicolon, {"Expected ; after package statement"});
  return statement;
}

StlTypeNameStatement StlParser::typeNameStatement(const std::vector<std::string>& messages)
{
  // typename -> identifier [(identifier '.')+ identifier ]

  StlTypeNameStatement statement;

  statement.path.push_back(consume(StlTokenType::identifier, messages));
  statement.qualifiedName.append(statement.path.back().lexeme);

  while (match({StlTokenType::dot}))
  {
    statement.path.push_back(consume(StlTokenType::identifier, messages));
    statement.qualifiedName.append(".");
    statement.qualifiedName.append(statement.path.back().lexeme);
  }

  return statement;
}

StlStructStatement StlParser::structDeclaration()
{
  // struct -> STRUCT identifier [':' identifier] ';'
  //         | STRUCT identifier [':' identifier] '{' struct_field* '}'

  StlStructStatement statement;
  statement.description = previousComment_;
  statement.identifier = consume(StlTokenType::identifier, {"Expected struct name"});
  checkNotReserved(statement.identifier);

  const auto& typeName = statement.identifier.lexeme;

  std::optional<StlToken> parentStructName;
  if (match({StlTokenType::colon}))
  {
    statement.parentStructName = typeNameStatement({"Expected parent struct name for ", typeName});
  }

  // if we have ';' then this is an empty struct
  if (match({StlTokenType::semicolon}))
  {
    return statement;
  }

  std::ignore = consume(StlTokenType::leftBrace, {"Expected '{' before struct body for ", typeName});

  while (!check(StlTokenType::rightBrace) && !isAtEnd())
  {
    statement.fields.push_back(structFieldStatement());
  }

  std::ignore = consume(StlTokenType::rightBrace, {"Expected '}' after struct body for ", typeName});
  return statement;
}

StlStructFieldStatement StlParser::structFieldStatement()
{
  // struct_field -> identifier: identifier ',' [comment]

  StlStructFieldStatement statement;

  // maybe we have a comment before the field
  while (check(StlTokenType::comment))
  {
    auto description = consume(StlTokenType::comment, {"expecting comment"});
    statement.description.push_back(description);
  }

  statement.identifier = consume(StlTokenType::identifier, {"Expected struct field name"});
  checkNotReserved(statement.identifier);

  std::ignore = consume(StlTokenType::colon, {"Expected : after struct field ", statement.identifier.lexeme});
  statement.typeName = typeNameStatement({"Expected struct field type"});

  // If next one is a comment and not a comma, we should be at the end of the structure
  if (const auto& prev = previous();
      check(StlTokenType::comment) && !containsNewLine(prev.loc.offset, peek().loc.offset))
  {
    statement.description.push_back(consume(StlTokenType::comment, {"Expecting inline comment."}));

    if (!check(StlTokenType::rightBrace))
    {
      throw error(peek(), "Expecting } to end struct definition");
    }

    return statement;
  }

  // Next one is closing brace, so we might be at the end of the structure
  if (check(StlTokenType::rightBrace) || isAtEnd())
  {
    return statement;
  }

  // otherwise we would expect a comma
  std::ignore = consume(StlTokenType::comma, {"Expected , after struct field ", statement.identifier.lexeme});

  // and maybe a comment (in the same line)
  if (const auto& comma = previous();
      check(StlTokenType::comment) && !containsNewLine(comma.loc.offset, peek().loc.offset))
  {
    statement.description.push_back(consume(StlTokenType::comment, {"Expecting inline comment."}));
  }

  return statement;
}

StlEnumStatement StlParser::enumDeclaration()
{
  // enum -> ENUM identifier '{' (enumerator (',' enumerator)*)* '}'

  StlEnumStatement statement;
  statement.description = previousComment_;
  statement.identifier = consume(StlTokenType::identifier, {"Expected enum name"});
  checkNotReserved(statement.identifier);

  const auto& typeName = statement.identifier.lexeme;

  std::ignore = consume(StlTokenType::colon, {"Expected ':' after enum name for ", typeName});
  statement.storageTypeName = typeNameStatement({"Expected enum storage type name for ", typeName});
  std::ignore = consume(StlTokenType::leftBrace, {"Expected '{' before enum body for ", typeName});

  while (!check(StlTokenType::rightBrace) && !isAtEnd())
  {
    statement.enumerators.push_back(enumeratorDeclaration());

    // If next one is a comment and not a comma, we should be at the end of the enumeration
    if (const auto& prev = previous();
        check(StlTokenType::comment) && !containsNewLine(prev.loc.offset, peek().loc.offset))
    {
      statement.enumerators.back().description.push_back(consume(StlTokenType::comment, {"Expecting inline comment."}));
      break;
    }

    // Next one is closing brace, so we might be at the end of the enumeration
    if (check(StlTokenType::rightBrace))
    {
      break;
    }

    // otherwise we expect a comma and wait for the next element
    std::ignore = consume(StlTokenType::comma, {"Expected ',' after enumerator for ", typeName});

    // and maybe a comment (in the same line)
    if (const auto& comma = previous();
        check(StlTokenType::comment) && !containsNewLine(comma.loc.offset, peek().loc.offset))
    {
      statement.enumerators.back().description.push_back(consume(StlTokenType::comment, {"Expecting inline comment."}));
    }
  }

  std::ignore = consume(StlTokenType::rightBrace, {"Expected '}' after enum body for ", typeName});
  return statement;
}

StlEnumeratorStatement StlParser::enumeratorDeclaration()
{
  // enumerator -> identifier

  StlEnumeratorStatement statement;

  // maybe we have a comment before the enumerator
  while (check(StlTokenType::comment))
  {
    auto description = consume(StlTokenType::comment, {"expecting comment"});
    statement.description.push_back(description);
  }

  statement.identifier = consume(StlTokenType::identifier, {"Expected enum name"});

  checkNotReserved(statement.identifier);
  return statement;
}

void StlParser::checkNotReserved(const StlToken& token)
{
  static std::set<std::string_view> reservedWords = {"alignas",
                                                     "alignof",
                                                     "and",
                                                     "and_eq",
                                                     "asm",
                                                     "atomic_cancel",
                                                     "atomic_commit",
                                                     "atomic_noexcept",
                                                     "auto",
                                                     "bitand",
                                                     "bitor",
                                                     "bool",
                                                     "break",
                                                     "case",
                                                     "catch",
                                                     "char",
                                                     "char8_t",
                                                     "char16_t",
                                                     "char32_t",
                                                     "class",
                                                     "compl",
                                                     "concept",
                                                     "const",
                                                     "consteval",
                                                     "constexpr",
                                                     "constinit",
                                                     "const_cast",
                                                     "continue",
                                                     "contract_assert",
                                                     "co_await",
                                                     "co_return",
                                                     "co_yield",
                                                     "decltype",
                                                     "default",
                                                     "delete",
                                                     "do",
                                                     "double",
                                                     "dynamic_cast",
                                                     "else",
                                                     "enum",
                                                     "explicit",
                                                     "export",
                                                     "extern",
                                                     "false",
                                                     "float",
                                                     "for",
                                                     "friend",
                                                     "goto",
                                                     "if",
                                                     "inline",
                                                     "int",
                                                     "long",
                                                     "mutable",
                                                     "namespace",
                                                     "new",
                                                     "noexcept",
                                                     "not",
                                                     "not_eq",
                                                     "nullptr",
                                                     "operator",
                                                     "or",
                                                     "or_eq",
                                                     "private",
                                                     "protected",
                                                     "public",
                                                     "reflexpr",
                                                     "register",
                                                     "reinterpret_cast",
                                                     "requires",
                                                     "return",
                                                     "short",
                                                     "signed",
                                                     "sizeof",
                                                     "static",
                                                     "static_assert",
                                                     "static_cast",
                                                     "struct",
                                                     "switch",
                                                     "synchronized",
                                                     "template",
                                                     "this",
                                                     "thread_local",
                                                     "throw",
                                                     "true",
                                                     "try",
                                                     "typedef",
                                                     "typeid",
                                                     "typename",
                                                     "union",
                                                     "unsigned",
                                                     "using",
                                                     "virtual",
                                                     "void",
                                                     "volatile",
                                                     "wchar_t",
                                                     "while",
                                                     "xor",
                                                     "xor_eq",
                                                     "final",
                                                     "override",
                                                     "transaction_safe",
                                                     "transaction_safe_dynamic",
                                                     "import",
                                                     "pre",
                                                     "post",
                                                     "trivially_relocatable_if_eligible",
                                                     "replaceable_if_eligible",
                                                     "_Pragma"};

  if (reservedWords.count(token.lexeme) != 0)
  {
    throw error(token, "usage of reserved word");
  }
}

StlVariantStatement StlParser::variantDeclaration()
{
  // variant -> VARIANT identifier '{' variant_element* '}'

  StlVariantStatement statement;
  statement.description = previousComment_;
  statement.identifier = consume(StlTokenType::identifier, {"Expected variant name"});
  const auto& typeName = statement.identifier.lexeme;

  std::ignore = consume(StlTokenType::leftBrace, {"Expected '{' before variant body for ", typeName});

  while (!check(StlTokenType::rightBrace) && !isAtEnd())
  {
    statement.elements.push_back(variantElementDeclaration());
  }

  std::ignore = consume(StlTokenType::rightBrace, {"Expected '}' after variant body for ", typeName});

  return statement;
}

StlVariantElement StlParser::variantElementDeclaration()
{
  // variant_element -> identifier ',' [comment]

  StlVariantElement statement;

  // maybe we have a comment before the variant element
  while (check(StlTokenType::comment))
  {
    auto description = consume(StlTokenType::comment, {"expecting comment"});
    statement.description.push_back(description);
  }

  statement.typeName = typeNameStatement({"Expected type name for variant"});
  const auto& qualName = statement.typeName.qualifiedName;

  // If next one is a comment and not a comma, we should be at the end of the variant
  if (const auto& prev = previous();
      check(StlTokenType::comment) && !containsNewLine(prev.loc.offset, peek().loc.offset))
  {
    statement.description.push_back(consume(StlTokenType::comment, {"Expecting inline comment."}));

    if (!check(StlTokenType::rightBrace))
    {
      throw error(peek(), "Expecting } to end variant definition");
    }

    return statement;
  }

  // Next one is closing brace, so we might be at the end of the variant
  if (check(StlTokenType::rightBrace) || isAtEnd())
  {
    return statement;
  }

  // otherwise we would expect a comma
  std::ignore = consume(StlTokenType::comma, {"Expected , after variant element ", qualName});

  // and maybe a comment (in the same line)
  if (const auto& comma = previous();
      check(StlTokenType::comment) && !containsNewLine(comma.loc.offset, peek().loc.offset))
  {
    statement.description.push_back(consume(StlTokenType::comment, {"Expecting inline comment."}));
  }

  return statement;
}

StlSequenceStatement StlParser::sequenceStatement()
{
  // sequence -> '<' identifier [',' identifier] '>' identifier ';'

  StlSequenceStatement statement;
  statement.description = previousComment_;

  std::ignore = consume(StlTokenType::less, {"Expected < to start sequence definition "});
  statement.elementTypeName = typeNameStatement({"Expected element type name for sequence "});

  if (match({StlTokenType::comma}))
  {
    statement.maxSize = consume(StlTokenType::integral, {"Expected maximum size for sequence"});
  }

  std::ignore = consume(StlTokenType::greater, {"Expected > to end sequence definition "});
  statement.identifier = consume(StlTokenType::identifier, {"Expected type name"});
  checkNotReserved(statement.identifier);

  statement.attributes = maybeAttributeList(statement.identifier.lexeme);

  std::ignore = consume(StlTokenType::semicolon, {"Expected ; after sequence statement"});

  if (const auto& semicolon = previous();
      check(StlTokenType::comment) && !containsNewLine(semicolon.loc.offset, peek().loc.offset))
  {
    statement.description.push_back(consume(StlTokenType::comment, {"Expecting inline comment."}));
  }

  return statement;
}

StlArrayStatement StlParser::arrayStatement()
{
  // array -> '<' identifier ',' identifier '>' identifier ';'

  StlArrayStatement statement;
  statement.description = previousComment_;

  std::ignore = consume(StlTokenType::less, {"Expected < to start array definition "});
  statement.elementTypeName = typeNameStatement({"Expected element type name for array "});

  std::ignore = consume(StlTokenType::comma, {"Expected ',' after array element type"});

  statement.size = consume(StlTokenType::integral, {"Expected size for array"});
  std::ignore = consume(StlTokenType::greater, {"Expected > to end array definition "});

  statement.identifier = consume(StlTokenType::identifier, {"Expected type name"});
  checkNotReserved(statement.identifier);

  statement.attributes = maybeAttributeList(statement.identifier.lexeme);

  std::ignore = consume(StlTokenType::semicolon, {"Expected ; after array statement"});

  if (const auto& semicolon = previous();
      check(StlTokenType::comment) && !containsNewLine(semicolon.loc.offset, peek().loc.offset))
  {
    statement.description.push_back(consume(StlTokenType::comment, {"Expecting inline comment."}));
  }

  return statement;
}

StlQuantityStatement StlParser::quantityStatement()
{
  // quantity -> '<' identifier ',' identifier '>' identifier [attribute_list] ';'

  StlQuantityStatement statement;
  statement.description = previousComment_;

  std::ignore = consume(StlTokenType::less, {"Expected < to start quantity definition "});
  statement.valueTypeName = typeNameStatement({"Expected value type name for quantity "});
  std::ignore = consume(StlTokenType::comma, {"Expected , after value type for quantity "});
  statement.unitName = consume(StlTokenType::identifier, {"Expected unit name for quantity "});
  std::ignore = consume(StlTokenType::greater, {"Expected > to end quantity definition "});

  statement.identifier = consume(StlTokenType::identifier, {"Expected type name"});
  checkNotReserved(statement.identifier);

  statement.attributes = maybeAttributeList(statement.identifier.lexeme);

  std::ignore = consume(StlTokenType::semicolon, {"Expected ; after quantity statement"});

  if (const auto& semicolon = previous();
      check(StlTokenType::comment) && !containsNewLine(semicolon.loc.offset, peek().loc.offset))
  {
    statement.description.push_back(consume(StlTokenType::comment, {"Expecting inline comment."}));
  }

  return statement;
}

StlTypeAliasStatement StlParser::aliasStatement()
{
  // alias -> identifier identifier ';'
  StlTypeAliasStatement statement;
  statement.description = previousComment_;

  statement.identifier = consume(StlTokenType::identifier, {"Expected type name for alias "});
  checkNotReserved(statement.identifier);

  statement.typeName = typeNameStatement({"Expected type for alias "});
  std::ignore = consume(StlTokenType::semicolon, {"Expected ; after alias statement"});

  if (const auto& semicolon = previous();
      check(StlTokenType::comment) && !containsNewLine(semicolon.loc.offset, peek().loc.offset))
  {
    statement.description.push_back(consume(StlTokenType::comment, {"Expecting inline comment."}));
  }

  return statement;
}

StlOptionalTypeStatement StlParser::optionalTypeStatement()
{
  // optional -> '<' identifier '>' identifier ';'
  StlOptionalTypeStatement statement;
  statement.description = previousComment_;

  std::ignore = consume(StlTokenType::less, {"Expected < to start optional definition "});
  statement.typeName = typeNameStatement({"Expected type name for optional "});
  std::ignore = consume(StlTokenType::greater, {"Expected > to end optional definition "});

  statement.identifier = consume(StlTokenType::identifier, {"Expected type name"});
  checkNotReserved(statement.identifier);

  std::ignore = consume(StlTokenType::semicolon, {"Expected ; after optional statement"});

  if (const auto& semicolon = previous();
      check(StlTokenType::comment) && !containsNewLine(semicolon.loc.offset, peek().loc.offset))
  {
    statement.description.push_back(consume(StlTokenType::comment, {"Expecting inline comment."}));
  }

  return statement;
}

StlInterfaceStatement StlParser::interfaceStatement()
{
  // interface -> INTERFACE identifier '{' class_member* '}'
  // class_member -> property | function | event

  StlInterfaceStatement statement;
  statement.identifier = consume(StlTokenType::identifier, {"Expected type name"});
  checkNotReserved(statement.identifier);

  statement.description = previousComment_;

  const auto& className = statement.identifier.lexeme;
  checkNotReserved(statement.identifier);

  std::ignore = consume(StlTokenType::leftBrace, {"Expected '{' before interface body for ", className});

  classOrInterfaceMembers(statement);

  std::ignore = consume(StlTokenType::rightBrace, {"Expected '}' after interface body for ", className});
  return statement;
}

StlClassStatement StlParser::classStatement(bool isAbstract)
{
  // class -> [ABSTRACT] CLASS identifier [parent_list] '{' class_member* '}'
  // class_member -> property | function | event

  StlClassStatement statement;
  statement.isAbstract = isAbstract;
  statement.members.identifier = consume(StlTokenType::identifier, {"Expected type name"});
  statement.members.description = previousComment_;

  const auto& className = statement.members.identifier.lexeme;
  classParents(statement);

  std::ignore = consume(StlTokenType::leftBrace, {"Expected '{' before class body for ", className});

  classOrInterfaceMembers(statement.members);

  std::ignore = consume(StlTokenType::rightBrace, {"Expected '}' after class body for ", className});
  return statement;
}

void StlParser::classParents(StlClassStatement& statement)
{
  // parent_list -> : [extends identifier] (implements identifier)*

  if (!match({StlTokenType::colon}))
  {
    return;
  }

  const auto& className = statement.members.identifier.lexeme;

  while (!isAtEnd() && !check(StlTokenType::leftBrace))
  {
    if (match({StlTokenType::keywordExtends}))
    {
      if (!statement.extends.qualifiedName.empty())
      {
        throw error(previous(), "Cannot inherit from more than one class. Consider using interfaces.");
      }

      statement.extends = typeNameStatement({"Expected parent class name for ", className});
    }
    else if (match({StlTokenType::keywordImplements}))
    {
      statement.implements.push_back(typeNameStatement({"Expected parent interface for ", className}));
    }
    else
    {
      std::string err = "Expected parent for class ";
      err.append(className);
      throw error(peek(), err);
    }

    // allow optional commas
    std::ignore = match({StlTokenType::comma});
  }
}

void StlParser::classOrInterfaceMembers(StlInterfaceStatement& statement)
{
  while (!check(StlTokenType::rightBrace) && !isAtEnd())
  {
    // allow comments before members
    // and use them as descriptions.
    previousComment_ = {};
    while (check(StlTokenType::comment))
    {
      previousComment_.push_back(consume(StlTokenType::comment, {"expecting comment"}));
    }

    // allow finishing by only setting the previous comment
    if (!previousComment_.empty() && (check(StlTokenType::rightBrace) || isAtEnd()))
    {
      break;
    }

    if (match({StlTokenType::keywordVar}))
    {
      statement.properties.push_back(varStatement());
    }
    else if (match({StlTokenType::keywordFunction}))
    {
      statement.functions.push_back(functionStatement());
    }
    else if (match({StlTokenType::keywordEvent}))
    {
      statement.events.push_back(eventStatement());
    }
    else
    {
      throw error(peek(), "Expected class member or '}'");
    }
  }
}

StlVarStatement StlParser::varStatement()
{
  // var -> VAR identifier ':' identifier attribute_list [= literal] ';'

  StlVarStatement statement;
  statement.identifier = consume(StlTokenType::identifier, {"Expected var name"});
  checkNotReserved(statement.identifier);

  std::ignore = consume(StlTokenType::colon, {"Expected : after var ", statement.identifier.lexeme});
  statement.typeName = typeNameStatement({"Expected var type"});

  if (check(StlTokenType::equal))
  {
    std::ignore = consume(StlTokenType::equal, {});
    statement.defaultValue = literal();  // or identifier for enums
  }

  if (auto attributes = maybeAttributeList(statement.identifier.lexeme); attributes)
  {
    statement.attributes = attributes.value();
  }

  std::ignore = consume(StlTokenType::semicolon, {"Expected ; after var ", statement.identifier.lexeme});

  statement.description = previousComment_;

  if (const auto& semicolon = previous();
      check(StlTokenType::comment) && !containsNewLine(semicolon.loc.offset, peek().loc.offset))
  {
    statement.description.push_back(consume(StlTokenType::comment, {"Expecting inline comment."}));
  }

  return statement;
}

StlFunctionStatement StlParser::functionStatement()
{
  // function -> FUNCTION identifier '(' arg_list ')' ['->' identifier] [attribute_list] ';'
  // arg_list -> (arg (, arg)* )*

  StlFunctionStatement statement;

  statement.identifier = consume(StlTokenType::identifier, {"Expected function name"});
  checkNotReserved(statement.identifier);

  std::ignore =
    consume(StlTokenType::leftParen, {"Expected ( to start args for function ", statement.identifier.lexeme});

  // arguments
  while (!check(StlTokenType::rightParen) && !isAtEnd())
  {
    statement.arguments.push_back(argStatement());

    if (!match({StlTokenType::comma}))
    {
      break;
    }
  }

  std::ignore =
    consume(StlTokenType::rightParen, {"Expected ) to close args for function ", statement.identifier.lexeme});

  if (match({StlTokenType::minus}))
  {
    std::ignore =
      consume(StlTokenType::greater, {"expecting -> for return type of function ", statement.identifier.lexeme});

    statement.returnTypeName = typeNameStatement({"Expected function return type"});
  }
  else
  {
    statement.returnTypeName.qualifiedName = "void";
    statement.returnTypeName.path.clear();
  }

  if (auto attributes = maybeAttributeList(statement.identifier.lexeme); attributes.has_value())
  {
    statement.attributes = *attributes;
  }

  std::ignore = consume(StlTokenType::semicolon, {"Expected ; after function ", statement.identifier.lexeme});

  validateComments(statement.identifier, statement.arguments);

  statement.description = previousComment_;

  if (const auto& semicolon = previous();
      check(StlTokenType::comment) && !containsNewLine(semicolon.loc.offset, peek().loc.offset))
  {
    statement.description.push_back(consume(StlTokenType::comment, {"Expecting inline comment."}));
  }

  return statement;
}

StlEventStatement StlParser::eventStatement()
{
  // event -> EVENT identifier '(' arg_list ')' attribute_list ';'
  // arg_list -> (arg (, arg)* )*

  StlEventStatement statement;

  statement.identifier = consume(StlTokenType::identifier, {"Expected event name"});
  checkNotReserved(statement.identifier);

  std::ignore = consume(StlTokenType::leftParen, {"Expected ( to start args for event ", statement.identifier.lexeme});

  // arguments
  while (!check(StlTokenType::rightParen) && !isAtEnd())
  {
    statement.arguments.push_back(argStatement());

    if (!match({StlTokenType::comma}))
    {
      break;
    }
  }

  std::ignore = consume(StlTokenType::rightParen, {"Expected ) to close args for event ", statement.identifier.lexeme});

  if (auto attributes = maybeAttributeList(statement.identifier.lexeme); attributes)
  {
    statement.attributes = attributes.value();
  }

  std::ignore = consume(StlTokenType::semicolon, {"Expected ; after event ", statement.identifier.lexeme});

  validateComments(statement.identifier, statement.arguments);

  statement.description = previousComment_;

  if (const auto& semicolon = previous();
      check(StlTokenType::comment) && !containsNewLine(semicolon.loc.offset, peek().loc.offset))
  {
    statement.description.push_back(consume(StlTokenType::comment, {"Expecting inline comment."}));
  }

  return statement;
}

StlArgStatement StlParser::argStatement()
{
  // arg -> identifier ':' identifier
  StlArgStatement statement;

  statement.identifier = consume(StlTokenType::identifier, {"Expected argument name"});
  checkNotReserved(statement.identifier);

  std::ignore = consume(StlTokenType::colon, {"Expected ':' after argument ", statement.identifier.lexeme});
  statement.typeName = typeNameStatement({"Expected argument type"});

  // accept comments after args
  if (check(StlTokenType::comment))
  {
    const auto& description = consume(StlTokenType::comment, {"expecting comment"});
    statement.description.push_back(description);
  }

  return statement;
}

std::optional<StlAttributeList> StlParser::maybeAttributeList(const std::string& subjectName)
{
  if (peek().type == StlTokenType::leftBracket)
  {
    return attributeList(subjectName);
  }
  return std::nullopt;
}

StlAttributeList StlParser::attributeList(const std::string& subjectName)
{
  // attribute_list -> '[' [attribute (',' attribute)* ']'
  // attribute -> identifier | identifier ':' (literal | identifier)

  std::ignore = consume(StlTokenType::leftBracket, {"Expected '[' to start attribute list for ", subjectName});

  StlAttributeList attributes;
  while (!check(StlTokenType::rightBracket) && !isAtEnd())
  {
    StlAttribute attr;
    attr.name = consume(StlTokenType::identifier, {"Expected attribute name"});

    if (check(StlTokenType::colon))
    {
      std::ignore = consume(StlTokenType::colon, {"Expected ':' after attribute name ", attr.name.lexeme});

      if (check(StlTokenType::identifier))
      {
        attr.value = consume(StlTokenType::identifier, {"Expected attribute value"});
      }
      else
      {
        attr.value = literal();
      }
    }

    attributes.push_back(std::move(attr));

    if (!match({StlTokenType::comma}))
    {
      break;
    }
  }

  std::ignore = consume(StlTokenType::rightBracket, {"Expected ']' to end attribute list for ", subjectName});

  return attributes;
}

StlToken StlParser::literal()
{
  // literal -> INTEGRAL | REAL | STRING | "true" | "false";

  if (match({StlTokenType::integral,
             StlTokenType::real,
             StlTokenType::string,
             StlTokenType::keywordTrue,
             StlTokenType::keywordFalse}))
  {
    return previous();
  }

  throw error(peek(), "Expecting literal");
}

QueryStatement StlParser::parseQuery()
{
  // query -> 'SELECT' ('*' | identifier) 'FROM' busname 'WHERE' expression

  QueryStatement statement = {};

  std::ignore = consume(StlTokenType::keywordSelect, {"Expected SELECT keyword in query"});

  // * means any class, and we leave an empty type statement
  if (!match({StlTokenType::star}))
  {
    statement.type = typeNameStatement({"Expecting class name for SELECT statement"});
  }

  std::ignore = consume(StlTokenType::keywordFrom, {"Expected FROM keyword in query"});
  statement.bus = busNameStatement();

  if (!isAtEnd())
  {
    std::ignore = consume(StlTokenType::keywordWhere, {"Expected WHERE keyword in query"});
    statement.condition = expression();
  }

  return statement;
}

BusNameStatement StlParser::busNameStatement()
{
  // busname -> identifier.identifier | '*'
  BusNameStatement statement;

  if (!match({StlTokenType::star}))
  {
    statement.session = consume(StlTokenType::identifier, {"Expecting session name"});
    std::ignore = consume(StlTokenType::dot, {"Expecting '.' after session name"});
    statement.bus = consume(StlTokenType::identifier, {"Expecting bus name"});
  }

  return statement;
}

std::shared_ptr<StlExpr> StlParser::expression() { return logicOr(); }

std::shared_ptr<StlExpr> StlParser::logicOr()
{
  auto expr = logicAnd();

  while (match({StlTokenType::keywordOr}))
  {
    auto rhs = logicAnd();
    expr = std::make_shared<StlExpr>(StlExpr {StlExprVal {StlLogicalExpr {expr, StlLogicalOperator::logicalOr, rhs}}});
  }

  return expr;
}

std::shared_ptr<StlExpr> StlParser::logicAnd()
{
  auto expr = equality();

  while (match({StlTokenType::keywordAnd}))
  {
    auto rhs = equality();
    expr = std::make_shared<StlExpr>(StlExpr {StlExprVal {StlLogicalExpr {expr, StlLogicalOperator::logicalAnd, rhs}}});
  }

  return expr;
}

std::shared_ptr<StlExpr> StlParser::equality()
{
  auto expr = comparison();

  while (match({StlTokenType::bangEqual, StlTokenType::equal}))
  {
    auto op = previous().type == StlTokenType::equal ? StlBinaryOperator::eq : StlBinaryOperator::ne;
    auto rhs = comparison();
    expr = std::make_shared<StlExpr>(StlExpr {StlExprVal {StlBinaryExpr {expr, op, rhs}}});
  }

  return expr;
}  // namespace sen::lang

std::shared_ptr<StlExpr> StlParser::comparison()
{
  auto expr = term();

  // handle possible NOT before 'BETWEEN' or 'IN' expressions
  bool isNotInBetween = false;
  if (peek().type == StlTokenType::keywordNot &&
      (peekNext().type == StlTokenType::keywordBetween || peekNext().type == StlTokenType::keywordIn))
  {
    std::ignore = advance();
    isNotInBetween = true;
  }

  if (match({StlTokenType::keywordBetween}))
  {
    auto min = term();
    std::ignore =
      consume(StlTokenType::keywordAnd, {"Expecting 'AND' after the minimum term of the BETWEEN expression"});
    auto max = term();

    if (std::holds_alternative<StlLiteralExpr>(min->value) && std::holds_alternative<StlLiteralExpr>(max->value))
    {
      auto& minValue = std::get<StlLiteralExpr>(min->value);
      auto& maxValue = std::get<StlLiteralExpr>(max->value);

      // if min and max are numbers, check that 'min' value is not greater than 'max' value
      if ((minValue.value.type == StlTokenType::real || minValue.value.type == StlTokenType::integral) &&
          (maxValue.value.type == StlTokenType::real || maxValue.value.type == StlTokenType::integral))
      {
        if (std::stod(minValue.value.lexeme) > std::stod(maxValue.value.lexeme))
        {
          std::string err("'BETWEEN' statement requires min value '");
          err.append(minValue.value.lexeme);
          err.append("' to be less or equal than max value '");
          err.append(maxValue.value.lexeme);
          err.append("'");
          throw error(peek(), err);
        }
      }
    }

    expr = std::make_shared<StlExpr>(StlExpr {StlExprVal {StlBetweenExpr {expr, min, max, isNotInBetween}}});
  }
  else if (match({StlTokenType::keywordIn}))
  {
    std::ignore = consume(StlTokenType::leftParen, {"Expecting '(' after the IN keyword"});

    StlInExpr inExpr;
    inExpr.notIn = isNotInBetween;
    inExpr.lhs = expr;
    while (true)
    {
      inExpr.set.push_back(term());
      if (match({StlTokenType::rightParen}))
      {
        break;
      }
      std::ignore = consume(StlTokenType::comma, {"Expecting ',' or ')' to continue with IN expression"});
    }
    expr = std::make_shared<StlExpr>(StlExpr {StlExprVal {std::move(inExpr)}});
  }
  else
  {
    while (match({StlTokenType::greater, StlTokenType::greaterEqual, StlTokenType::less, StlTokenType::lessEqual}))
    {
      StlBinaryOperator op = {};

      switch (previous().type)
      {
        case StlTokenType::greater:
          op = StlBinaryOperator::gt;
          break;

        case StlTokenType::greaterEqual:
          op = StlBinaryOperator::ge;
          break;

        case StlTokenType::less:
          op = StlBinaryOperator::lt;
          break;

        case StlTokenType::lessEqual:
          op = StlBinaryOperator::le;
          break;

        default:
          break;
      }

      auto rhs = term();
      expr = std::make_shared<StlExpr>(StlExpr {StlExprVal {StlBinaryExpr {expr, op, rhs}}});
    }
  }

  return expr;
}

std::shared_ptr<StlExpr> StlParser::term()
{
  auto expr = factor();

  while (match({StlTokenType::minus, StlTokenType::plus}))
  {
    auto op = previous().type == StlTokenType::minus ? StlBinaryOperator::subtract : StlBinaryOperator::add;
    auto rhs = factor();
    expr = std::make_shared<StlExpr>(StlExpr {StlExprVal {StlBinaryExpr {expr, op, rhs}}});
  }

  return expr;
}

std::shared_ptr<StlExpr> StlParser::factor()
{
  auto expr = unary();

  while (match({StlTokenType::slash, StlTokenType::star}))
  {
    auto op = previous().type == StlTokenType::slash ? StlBinaryOperator::divide : StlBinaryOperator::multiply;
    auto rhs = unary();
    expr = std::make_shared<StlExpr>(StlExpr {StlExprVal {StlBinaryExpr {expr, op, rhs}}});
  }

  return expr;
}

std::shared_ptr<StlExpr> StlParser::unary()
{
  if (match({StlTokenType::bang, StlTokenType::minus, StlTokenType::keywordNot}))
  {
    auto op = previous().type == StlTokenType::minus ? StlUnaryOperator::negate : StlUnaryOperator::logicalNot;
    auto rhs = unary();
    return std::make_shared<StlExpr>(StlExpr {StlExprVal {StlUnaryExpr {op, rhs}}});
  }

  return primary();
}

std::shared_ptr<StlExpr> StlParser::primary()
{
  if (match({StlTokenType::integral,
             StlTokenType::real,
             StlTokenType::string,
             StlTokenType::keywordTrue,
             StlTokenType::keywordFalse}))
  {
    return std::make_shared<StlExpr>(StlExpr {StlExprVal {StlLiteralExpr {previous()}}});
  }

  if (check(StlTokenType::identifier))
  {
    return variable();
  }

  if (match({StlTokenType::leftParen}))
  {
    auto expr = expression();
    std::ignore = consume(StlTokenType::rightParen, {"expecting ')' after expression."});
    return std::make_shared<StlExpr>(StlExpr {StlExprVal {StlGroupingExpr {expr}}});
  }

  throw error(peek(), "Expecting primary expression");
}

std::shared_ptr<StlExpr> StlParser::variable()
{
  // variable -> identifier ['.' identifier]*

  StlVariableExpr expr = {};

  expr.path.push_back(consume(StlTokenType::identifier, {"Expecting identifier for variable name"}));
  expr.name.append(expr.path.back().lexeme);

  while (match({StlTokenType::dot}))
  {
    expr.path.push_back(consume(StlTokenType::identifier, {"Expecting identifier for variable name"}));
    expr.name.append(".");
    expr.name.append(expr.path.back().lexeme);
  }

  return std::make_shared<StlExpr>(StlExpr {StlExprVal {expr}});
}

bool StlParser::match(const std::vector<StlTokenType>& types)
{
  for (const auto& type: types)  // NOSONAR
  {
    if (check(type))
    {
      std::ignore = advance();
      return true;
    }
  }

  return false;
}

bool StlParser::check(StlTokenType type) const
{
  if (isAtEnd())
  {
    return false;
  }

  return peek().type == type;
}

const StlToken& StlParser::peek() const { return tokens_.at(current_); }

StlToken StlParser::peekNext() const
{
  if (current_ + 1U >= tokens_.size())
  {
    return {};
  }

  return tokens_.at(current_ + 1);
}

const StlToken& StlParser::previous() const { return tokens_.at(current_ - 1U); }

bool StlParser::isAtEnd() const noexcept { return peek().type == StlTokenType::endOfFile; }

const StlToken& StlParser::advance()
{
  if (!isAtEnd())
  {
    current_++;
  }

  return previous();
}

const StlToken& StlParser::consume(StlTokenType type, const std::vector<std::string>& message)
{
  if (check(type))
  {
    return advance();
  }

  std::string err;
  for (const auto& item: message)
  {
    err.append(item);
  }

  throw error(peek(), err);
}

StlParser::ParseError StlParser::error(const StlToken& token, const std::string& message) const
{
  ErrorReporter::report(token.loc, message, token.lexeme.size());
  return ParseError(message);
}

void StlParser::validateComments(const StlToken& functionOrEventIdentifier,
                                 const std::vector<StlArgStatement>& arguments)
{
  std::set<std::string> argsNames;

  for (const auto& arg: arguments)
  {
    argsNames.insert(arg.identifier.lexeme);
  }

  std::vector<StlToken> filteredComments;
  std::set<std::string> documentedParams;
  bool endOfDescription = false;

  for (const auto& commentToken: previousComment_)
  {
    std::string commentText = std::string(commentToken.lexeme);

    std::regex paramRegex("@param\\s+([a-zA-Z_][a-zA-z0-9_]*)");

    std::smatch paramMatch;

    if (std::regex_search(commentText, paramMatch, paramRegex))
    {
      endOfDescription = true;
      std::string paramName = paramMatch[1].str();

      if (documentedParams.count(paramName) > 0)
      {
        throw error(functionOrEventIdentifier, "Parameter '" + paramName + "' is documented more than once.");
      }

      documentedParams.insert(paramName);
      continue;
    }

    if (!endOfDescription)
    {
      filteredComments.push_back(commentToken);
    }
  }

  for (const auto& documentedParam: documentedParams)
  {
    if (argsNames.find(documentedParam) == argsNames.end())
    {
      throw error(functionOrEventIdentifier,
                  "Documented parameter '" + documentedParam + "' was not found in the argument list.");
    }
  }

  previousComment_ = filteredComments;
}

bool StlParser::containsNewLine(size_t startOffset, size_t endOffset) const
{
  std::string_view source = std::string_view(peek().loc.src).substr(startOffset, endOffset - startOffset);
  return std::any_of(source.begin(), source.end(), [](char c) { return c == '\n'; });
}

}  // namespace sen::lang
