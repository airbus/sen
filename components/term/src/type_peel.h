// === type_peel.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_TERM_SRC_TYPE_PEEL_H
#define SEN_COMPONENTS_TERM_SRC_TYPE_PEEL_H

// sen
#include "sen/core/meta/alias_type.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/enum_type.h"
#include "sen/core/meta/optional_type.h"
#include "sen/core/meta/quantity_type.h"
#include "sen/core/meta/sequence_type.h"
#include "sen/core/meta/struct_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/variant_type.h"

// std
#include <string_view>

namespace sen::components::term
{

/// The word term shows for a type's kind. Named here because `types` uses it in its table and the
/// completer in its annotations.
[[nodiscard]] inline std::string_view typeKindName(const Type& type)
{
  if (type.asClassType() != nullptr)
  {
    return "class";
  }
  if (type.asStructType() != nullptr)
  {
    return "struct";
  }
  if (type.asEnumType() != nullptr)
  {
    return "enum";
  }
  if (type.asSequenceType() != nullptr)
  {
    return "sequence";
  }
  if (type.asVariantType() != nullptr)
  {
    return "variant";
  }
  if (type.asQuantityType() != nullptr)
  {
    return "quantity";
  }
  if (type.asAliasType() != nullptr)
  {
    return "alias";
  }
  if (type.asOptionalType() != nullptr)
  {
    return "optional";
  }
  return "type";
}

/// Peel every alias layer, giving the type the alias ultimately names.
///
/// An alias is transparent to everything term does with a type: the editor to offer, the way to format a
/// value, whether it is a sequence. So almost every use has to peel first, because keeping the alias makes
/// `asQuantityType()` and `asOptionalType()` return null downstream.
[[nodiscard]] inline ConstTypeHandle<> peelAliases(ConstTypeHandle<> type)
{
  for (const auto* alias = type->asAliasType(); alias != nullptr; alias = type->asAliasType())
  {
    type = alias->getAliasedType();
  }
  return type;
}

/// The same, for a raw type pointer: `TypeHandle`'s pointer constructor is private, so code holding a
/// `const Type&` cannot make a handle to peel.
[[nodiscard]] inline const Type* peelAliases(const Type* type)
{
  for (const auto* alias = type->asAliasType(); alias != nullptr; alias = type->asAliasType())
  {
    type = alias->getAliasedType().type();
  }
  return type;
}

/// The same, for the optional handle a form field carries. Throws on an empty handle, as every
/// hand-written version did.
[[nodiscard]] inline ConstTypeHandle<> peelAliases(const MaybeConstTypeHandle<>& type)
{
  return peelAliases(type.value());
}

/// Peel alias and optional layers together, giving the value type underneath. Use this where a field
/// holds a value and it does not matter whether that value is optional; use `peelAliases` where the
/// optional itself matters.
[[nodiscard]] inline ConstTypeHandle<> peelToValue(ConstTypeHandle<> type)
{
  while (true)
  {
    type = peelAliases(type);
    const auto* optional = type->asOptionalType();
    if (optional == nullptr)
    {
      return type;
    }
    type = optional->getType();
  }
}

}  // namespace sen::components::term

#endif  // SEN_COMPONENTS_TERM_SRC_TYPE_PEEL_H
