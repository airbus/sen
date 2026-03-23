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

/// AST node for an `import` directive.
struct StlImportStatement
{
  StlToken fileName;  ///< Token holding the path of the file to import.
};

/// AST node for a `package` declaration.
struct StlPackageStatement
{
  std::vector<StlToken> path;  ///< Dot-separated package name tokens (e.g. `["sen", "geometry"]`).
};

/// A single keyâ€“value attribute (e.g. `[transport = multicast]`).
struct StlAttribute
{
  StlToken name;   ///< Attribute key token.
  StlToken value;  ///< Attribute value token.
};

/// Ordered list of attributes attached to a declaration.
using StlAttributeList = std::vector<StlAttribute>;

/// AST node for a type name reference, possibly qualified.
struct StlTypeNameStatement
{
  std::vector<StlToken> path;  ///< Qualifier tokens preceding the type name (e.g. namespace segments).
  std::string qualifiedName;   ///< Fully-qualified type name assembled from @p path.
};

/// AST node for a single field inside a `struct` declaration.
struct StlStructFieldStatement
{
  StlTypeNameStatement typeName;      ///< Type of this field.
  StlToken identifier;                ///< Field name token.
  std::vector<StlToken> description;  ///< Documentation tokens extracted from the source comment.
};

/// AST node for a `struct` declaration.
struct StlStructStatement
{
  StlToken identifier;                                   ///< Struct name token.
  std::vector<StlToken> description;                     ///< Documentation tokens.
  std::optional<StlTypeNameStatement> parentStructName;  ///< Base struct name, if this struct extends another.
  std::vector<StlStructFieldStatement> fields;           ///< Declared fields in source order.
};

/// AST node for a single enumerator inside an `enum` declaration.
struct StlEnumeratorStatement
{
  StlToken identifier;                ///< Enumerator name token.
  std::vector<StlToken> description;  ///< Documentation tokens.
  StlAttributeList attributes;        ///< Attributes attached to this enumerator.
};

/// AST node for an `enum` declaration.
struct StlEnumStatement
{
  StlToken identifier;                              ///< Enum type name token.
  std::vector<StlToken> description;                ///< Documentation tokens.
  StlTypeNameStatement storageTypeName;             ///< Underlying integer storage type.
  std::vector<StlEnumeratorStatement> enumerators;  ///< Enumerator values in source order.
};

/// AST node for one alternative (element type) inside a `variant` declaration.
struct StlVariantElement
{
  StlTypeNameStatement typeName;      ///< Type of this variant alternative.
  std::vector<StlToken> description;  ///< Documentation tokens for this alternative.
};

/// AST node for a `variant` declaration.
struct StlVariantStatement
{
  StlToken identifier;                      ///< Variant type name token.
  std::vector<StlToken> description;        ///< Documentation tokens.
  std::vector<StlVariantElement> elements;  ///< Alternative types in source order.
};

/// AST node for a `quantity` declaration.
struct StlQuantityStatement
{
  StlToken identifier;                         ///< Quantity type name token.
  std::vector<StlToken> description;           ///< Documentation tokens.
  StlTypeNameStatement valueTypeName;          ///< Underlying numeric value type.
  StlToken unitName;                           ///< SI unit token (e.g. `"m"`, `"rad"`).
  std::optional<StlAttributeList> attributes;  ///< Optional attributes (e.g. range constraints).
};

/// AST node for a `sequence` (variable-length array) declaration.
struct StlSequenceStatement
{
  StlToken identifier;                         ///< Sequence type name token.
  std::vector<StlToken> description;           ///< Documentation tokens.
  StlTypeNameStatement elementTypeName;        ///< Element type of the sequence.
  std::optional<StlToken> maxSize;             ///< Optional maximum element count constraint.
  std::optional<StlAttributeList> attributes;  ///< Optional attributes.
};

/// AST node for a fixed-size `array` declaration.
struct StlArrayStatement
{
  StlToken identifier;                         ///< Array type name token.
  std::vector<StlToken> description;           ///< Documentation tokens.
  StlTypeNameStatement elementTypeName;        ///< Element type of the array.
  StlToken size;                               ///< Token holding the fixed element count.
  std::optional<StlAttributeList> attributes;  ///< Optional attributes.
};

/// AST node for a type alias (`typedef`-style) declaration.
struct StlTypeAliasStatement
{
  StlToken identifier;                ///< Alias name token.
  StlTypeNameStatement typeName;      ///< The target type being aliased.
  std::vector<StlToken> description;  ///< Documentation tokens.
};

/// AST node for an `optional<T>` type declaration.
struct StlOptionalTypeStatement
{
  StlToken identifier;                ///< Optional type name token.
  StlTypeNameStatement typeName;      ///< The wrapped element type.
  std::vector<StlToken> description;  ///< Documentation tokens.
};

/// AST node for a single argument in a function or event signature.
struct StlArgStatement
{
  StlToken identifier;                ///< Argument name token.
  StlTypeNameStatement typeName;      ///< Type of the argument.
  std::vector<StlToken> description;  ///< Documentation tokens.
};

/// AST node for a property (`var`) declaration inside a class or interface.
struct StlVarStatement
{
  StlToken identifier;                ///< Property name token.
  StlToken defaultValue;              ///< Default value token (may be empty).
  std::vector<StlToken> description;  ///< Documentation tokens.
  StlAttributeList attributes;        ///< Attributes (e.g. transport mode, category).
  StlTypeNameStatement typeName;      ///< Value type of the property.
};

/// AST node for a method (`function`) declaration inside a class or interface.
struct StlFunctionStatement
{
  StlToken identifier;                     ///< Method name token.
  std::vector<StlToken> description;       ///< Documentation tokens.
  StlAttributeList attributes;             ///< Attributes (e.g. `[local]`, `[deferred]`).
  StlTypeNameStatement returnTypeName;     ///< Return type of the method.
  std::vector<StlArgStatement> arguments;  ///< Parameter list in source order.
};

/// AST node for an `event` declaration inside a class or interface.
struct StlEventStatement
{
  StlToken identifier;                     ///< Event name token.
  std::vector<StlToken> description;       ///< Documentation tokens.
  StlAttributeList attributes;             ///< Attributes attached to the event.
  std::vector<StlArgStatement> arguments;  ///< Event argument list in source order.
};

/// AST node for an `interface` declaration (or the body of a `class`).
struct StlInterfaceStatement
{
  StlToken identifier;                          ///< Interface name token.
  std::vector<StlToken> description;            ///< Documentation tokens.
  std::vector<StlVarStatement> properties;      ///< Property declarations in source order.
  std::vector<StlFunctionStatement> functions;  ///< Method declarations in source order.
  std::vector<StlEventStatement> events;        ///< Event declarations in source order.
};

/// AST node for a `class` declaration.
struct StlClassStatement
{
  bool isAbstract = false;                       ///< `true` if the class is declared `abstract`.
  StlInterfaceStatement members;                 ///< Member declarations (properties, methods, events).
  StlTypeNameStatement extends;                  ///< Base class name (empty if none).
  std::vector<StlTypeNameStatement> implements;  ///< Interface names this class implements.
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

/// AST node for a `<session>.<bus>` bus address reference in a SELECT query.
struct BusNameStatement
{
  StlToken session;  ///< Session name token.
  StlToken bus;      ///< Bus name token.
};

/// AST node for a full `SELECT [type] FROM <bus> [WHERE condition]` query statement.
struct QueryStatement
{
  std::optional<StlTypeNameStatement> type;  ///< Type filter, or `nullopt` to select all types (`*`).
  BusNameStatement bus;                      ///< Source bus address.
  std::shared_ptr<StlExpr> condition;        ///< Optional `WHERE` filter expression; may be null.
};

/// @}

}  // namespace sen::lang

#endif  // SEN_CORE_LANG_STL_STATEMENT_H
