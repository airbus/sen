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
  logicalNot,
  negate,
};

/// Unary operation.
struct StlUnaryExpr
{
  StlUnaryOperator op;
  std::shared_ptr<StlExpr> expr;
};

/// Kinds of binary operations.
enum class StlBinaryOperator
{
  add,
  subtract,
  multiply,
  divide,
  lt,
  le,
  gt,
  ge,
  eq,
  ne,
};

/// Binary expression.
struct StlBinaryExpr
{
  std::shared_ptr<StlExpr> lhs;
  StlBinaryOperator op;
  std::shared_ptr<StlExpr> rhs;
};

/// Literals.
struct StlLiteralExpr
{
  StlToken value;
};

/// Groups.
struct StlGroupingExpr
{
  std::shared_ptr<StlExpr> expr;
};

/// Logic.
enum class StlLogicalOperator
{
  logicalAnd,
  logicalOr,
};

/// Binary logical operation.
struct StlLogicalExpr
{
  std::shared_ptr<StlExpr> lhs;
  StlLogicalOperator op;
  std::shared_ptr<StlExpr> rhs;
};

/// A variable.
struct StlVariableExpr
{
  std::vector<StlToken> path;
  std::string name;
};

/// A BETWEEN expression.
struct StlBetweenExpr
{
  std::shared_ptr<StlExpr> lhs;
  std::shared_ptr<StlExpr> min;
  std::shared_ptr<StlExpr> max;
  bool notBetween = false;
};

/// A IN expression.
struct StlInExpr
{
  std::shared_ptr<StlExpr> lhs;
  std::vector<std::shared_ptr<StlExpr>> set;
  bool notIn = false;
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

/// Wrapper for expression values.
struct StlExpr
{
  StlExprVal value;
};

/// @}

}  // namespace sen::lang

#endif  // SEN_CORE_LANG_STL_EXPRESSION_H
