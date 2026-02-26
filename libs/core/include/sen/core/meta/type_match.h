// === type_match.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_TYPE_MATCH_H
#define SEN_CORE_META_TYPE_MATCH_H

// sen
#include "sen/core/base/result.h"
#include "sen/core/meta/type.h"

// std
#include <string>
#include <vector>

namespace sen
{

/// \addtogroup type_utils
/// @{

/// Kind of problem found when matching types.
enum class TypeMatchProblem
{
  compatible,    ///< Not the same, but convertable.
  incompatible,  ///< Cannot be converted.
  missing,       ///< There is missing information.
};

/// Information about a type matching issue.
struct TypeMatchIssue final
{
  TypeMatchProblem problem;
  std::string description;
};

/// Types are compatible.
struct TypesMatch final
{
  std::vector<TypeMatchIssue> issues;
};

/// Types do not match
struct TypesDontMatch final
{
  std::vector<TypeMatchIssue> issues;
};

/// The result of a type matching analysis.
using TypeMatch = Result<TypesMatch, TypesDontMatch>;

/// Analyze if a type can be converted to another.
TypeMatch canConvert(const Type& from, const Type& to);

/// @}

}  // namespace sen

#endif  // SEN_CORE_META_TYPE_MATCH_H
