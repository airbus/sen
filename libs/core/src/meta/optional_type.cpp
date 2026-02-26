// === optional_type.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/optional_type.h"

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
// OptionalType
//--------------------------------------------------------------------------------------------------------------

OptionalType::OptionalType(OptionalSpec spec, Private notUsable)
  : CustomType(impl::hash<OptionalSpec>()(spec)), spec_ {spec}
{
  std::ignore = notUsable;
}

TypeHandle<OptionalType> OptionalType::make(OptionalSpec spec)
{
  impl::checkUserTypeName(spec.name);
  impl::checkIdentifier(spec.qualifiedName, true);

  return {std::move(spec), Private {}};
}

ConstTypeHandle<> OptionalType::getType() const noexcept { return spec_.type; }

std::string_view OptionalType::getQualifiedName() const noexcept { return spec_.qualifiedName; }

std::string_view OptionalType::getName() const noexcept { return spec_.name; }

std::string_view OptionalType::getDescription() const noexcept { return spec_.description; }

bool OptionalType::equals(const Type& other) const noexcept
{
  return &other == this || (other.isOptionalType() && spec_ == other.asOptionalType()->spec_);
}

bool OptionalType::isBounded() const noexcept { return spec_.type->isBounded(); }

}  // namespace sen
