// === stl_statement.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_LANG_STL_STATEMENT_H
#define SEN_CORE_LANG_STL_STATEMENT_H

// sen
#include "sen/core/lang/stl_expression.h"
#include "sen/core/lang/stl_token.h"

// std
#include <memory>
#include <optional>

namespace sen::lang
{

/// \addtogroup lang
/// @{

struct StlImportStatement
{
  StlToken fileName;
};

struct StlPackageStatement
{
  std::vector<StlToken> path;
};

struct StlAttribute
{
  StlToken name;
  StlToken value;
};

using StlAttributeList = std::vector<StlAttribute>;

struct StlTypeNameStatement
{
  std::vector<StlToken> path;
  std::string qualifiedName;
};

struct StlStructFieldStatement
{
  StlTypeNameStatement typeName;
  StlToken identifier;
  std::vector<StlToken> description;
};

struct StlStructStatement
{
  StlToken identifier;
  std::vector<StlToken> description;
  std::optional<StlTypeNameStatement> parentStructName;
  std::vector<StlStructFieldStatement> fields;
};

struct StlEnumeratorStatement
{
  StlToken identifier;
  std::vector<StlToken> description;
  StlAttributeList attributes;
};

struct StlEnumStatement
{
  StlToken identifier;
  std::vector<StlToken> description;
  StlTypeNameStatement storageTypeName;
  std::vector<StlEnumeratorStatement> enumerators;
};

struct StlVariantElement
{
  StlTypeNameStatement typeName;
  std::vector<StlToken> description;
};

struct StlVariantStatement
{
  StlToken identifier;
  std::vector<StlToken> description;
  std::vector<StlVariantElement> elements;
};

struct StlQuantityStatement
{
  StlToken identifier;
  std::vector<StlToken> description;
  StlTypeNameStatement valueTypeName;
  StlToken unitName;
  std::optional<StlAttributeList> attributes;
};

struct StlSequenceStatement
{
  StlToken identifier;
  std::vector<StlToken> description;
  StlTypeNameStatement elementTypeName;
  std::optional<StlToken> maxSize;
  std::optional<StlAttributeList> attributes;
};

struct StlArrayStatement
{
  StlToken identifier;
  std::vector<StlToken> description;
  StlTypeNameStatement elementTypeName;
  StlToken size;
  std::optional<StlAttributeList> attributes;
};

struct StlTypeAliasStatement
{
  StlToken identifier;
  StlTypeNameStatement typeName;
  std::vector<StlToken> description;
};

struct StlOptionalTypeStatement
{
  StlToken identifier;
  StlTypeNameStatement typeName;
  std::vector<StlToken> description;
};

struct StlArgStatement
{
  StlToken identifier;
  StlTypeNameStatement typeName;
  std::vector<StlToken> description;
};

struct StlVarStatement
{
  StlToken identifier;
  StlToken defaultValue;
  std::vector<StlToken> description;
  StlAttributeList attributes;
  StlTypeNameStatement typeName;
};

struct StlFunctionStatement
{
  StlToken identifier;
  std::vector<StlToken> description;
  StlAttributeList attributes;
  StlTypeNameStatement returnTypeName;
  std::vector<StlArgStatement> arguments;
};

struct StlEventStatement
{
  StlToken identifier;
  std::vector<StlToken> description;
  StlAttributeList attributes;
  std::vector<StlArgStatement> arguments;
};

struct StlInterfaceStatement
{
  StlToken identifier;
  std::vector<StlToken> description;
  std::vector<StlVarStatement> properties;
  std::vector<StlFunctionStatement> functions;
  std::vector<StlEventStatement> events;
};

struct StlClassStatement
{
  bool isAbstract = false;
  StlInterfaceStatement members;
  StlTypeNameStatement extends;
  std::vector<StlTypeNameStatement> implements;
};

using StlStatement = std::variant<std::monostate,
                                  StlImportStatement,
                                  StlPackageStatement,
                                  StlStructStatement,
                                  StlEnumStatement,
                                  StlVariantStatement,
                                  StlSequenceStatement,
                                  StlArrayStatement,
                                  StlQuantityStatement,
                                  StlTypeAliasStatement,
                                  StlOptionalTypeStatement,
                                  StlClassStatement,
                                  StlInterfaceStatement>;

struct BusNameStatement
{
  StlToken session;
  StlToken bus;
};

struct QueryStatement
{
  std::optional<StlTypeNameStatement> type;
  BusNameStatement bus;
  std::shared_ptr<StlExpr> condition;
};

/// @}

}  // namespace sen::lang

#endif  // SEN_CORE_LANG_STL_STATEMENT_H
