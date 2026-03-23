// === stl_expression.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_LANG_STL_EXPRESSION_H
#define SEN_CORE_LANG_STL_EXPRESSION_H

#include "sen/core/lang/stl_token.h"

namespace sen::lang
{

/// \addtogroup lang
/// @{

struct StlExpr;

/// An operator on a single term.
enum class StlUnaryOperator
{
  logicalNot,  ///< Logical negation (`!`).
  negate,      ///< Arithmetic negation (`-`).
};

/// Unary operation.
struct StlUnaryExpr
{
  StlUnaryOperator op;            ///< The unary operator to apply.
  std::shared_ptr<StlExpr> expr;  ///< The operand expression.
};

/// Kinds of binary operations.
enum class StlBinaryOperator
{
  add,       ///< Addition (`+`).
  subtract,  ///< Subtraction (`-`).
  multiply,  ///< Multiplication (`*`).
  divide,    ///< Division (`/`).
  lt,        ///< Less-than (`<`).
  le,        ///< Less-than-or-equal (`<=`).
  gt,        ///< Greater-than (`>`).
  ge,        ///< Greater-than-or-equal (`>=`).
  eq,        ///< Equality (`==`).
  ne,        ///< Inequality (`!=`).
};

/// Binary expression.
struct StlBinaryExpr
{
  std::shared_ptr<StlExpr> lhs;  ///< Left-hand side operand.
  StlBinaryOperator op;          ///< The binary operator.
  std::shared_ptr<StlExpr> rhs;  ///< Right-hand side operand.
};

/// Literals.
struct StlLiteralExpr
{
  StlToken value;  ///< Token holding the literal value (number, string, etc.).
};

/// Groups.
struct StlGroupingExpr
{
  std::shared_ptr<StlExpr> expr;  ///< The parenthesised inner expression.
};

/// Logic.
enum class StlLogicalOperator
{
  logicalAnd,  ///< Short-circuit logical AND (`AND` / `&&`).
  logicalOr,   ///< Short-circuit logical OR (`OR` / `||`).
};

/// Binary logical operation.
struct StlLogicalExpr
{
  std::shared_ptr<StlExpr> lhs;  ///< Left-hand side operand.
  StlLogicalOperator op;         ///< The logical operator.
  std::shared_ptr<StlExpr> rhs;  ///< Right-hand side operand.
};

/// A variable.
struct StlVariableExpr
{
  std::vector<StlToken> path;  ///< Qualifier tokens preceding the final name (e.g. namespace segments).
  std::string name;            ///< Fully-qualified variable name as a string.
};

/// A BETWEEN expression.
struct StlBetweenExpr
{
  std::shared_ptr<StlExpr> lhs;  ///< The value being tested.
  std::shared_ptr<StlExpr> min;  ///< Lower bound of the range (inclusive).
  std::shared_ptr<StlExpr> max;  ///< Upper bound of the range (inclusive).
  bool notBetween = false;       ///< If `true`, the semantics are inverted (`NOT BETWEEN`).
};

/// A IN expression.
struct StlInExpr
{
  std::shared_ptr<StlExpr> lhs;               ///< The value being tested for membership.
  std::vector<std::shared_ptr<StlExpr>> set;  ///< The candidate set to test against.
  bool notIn = false;                         ///< If `true`, the semantics are inverted (`NOT IN`).
};

/// A variant supporting all expressions.
using StlExprVal = std::variant<std::monostate,
                                StlLiteralExpr,
                                StlUnaryExpr,
                                StlBinaryExpr,
                                StlGroupingExpr,
                                StlLogicalExpr,
                                StlVariableExpr,
                                StlBetweenExpr,
                                StlInExpr>;

/// Wrapper for expression values; the root node type of the STL expression AST.
struct StlExpr
{
  StlExprVal value;  ///< The concrete expression held by this node.
};

/// @}

}  // namespace sen::lang

#endif  // SEN_CORE_LANG_STL_EXPRESSION_H
