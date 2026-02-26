// === alias_type.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/alias_type.h"

// implementation
#include "utils.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/type.h"

// std
#include <memory>
#include <string_view>
#include <tuple>
#include <utility>

namespace sen
{
//--------------------------------------------------------------------------------------------------------------
// AliasType
//--------------------------------------------------------------------------------------------------------------

AliasType::AliasType(AliasSpec spec, Private notUsable): CustomType(impl::hash<AliasSpec>()(spec)), spec_ {spec}
{
  std::ignore = notUsable;
}

TypeHandle<AliasType> AliasType::make(AliasSpec spec)
{
  impl::checkUserTypeName(spec.name);
  impl::checkIdentifier(spec.qualifiedName, true);

  return TypeHandle<AliasType> {std::move(spec), Private {}};
}

ConstTypeHandle<> AliasType::getAliasedType() const noexcept { return spec_.aliasedType; }

std::string_view AliasType::getQualifiedName() const noexcept { return spec_.qualifiedName; }

std::string_view AliasType::getName() const noexcept { return spec_.name; }

std::string_view AliasType::getDescription() const noexcept { return spec_.description; }

bool AliasType::equals(const Type& other) const noexcept
{
  return &other == this || (other.isAliasType() && spec_ == other.asAliasType()->spec_);
}

bool AliasType::isBounded() const noexcept { return spec_.aliasedType->isBounded(); }

}  // namespace sen
