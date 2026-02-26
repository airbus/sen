// === sequence_type.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/sequence_type.h"

// implementation
#include "utils.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/type.h"

// std
#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>

namespace sen
{
//--------------------------------------------------------------------------------------------------------------
// SequenceType
//--------------------------------------------------------------------------------------------------------------

SequenceType::SequenceType(SequenceSpec spec, Private notUsable)
  : CustomType(impl::hash<SequenceSpec>()(spec)), spec_(std::move(spec))
{
  std::ignore = notUsable;
}

TypeHandle<SequenceType> SequenceType::make(SequenceSpec spec)
{
  impl::checkUserTypeName(spec.name);
  impl::checkIdentifier(spec.qualifiedName, true);

  return {std::move(spec), Private {}};
}

ConstTypeHandle<> SequenceType::getElementType() const noexcept { return spec_.elementType; }

std::optional<std::size_t> SequenceType::getMaxSize() const noexcept { return spec_.maxSize; }

bool SequenceType::hasFixedSize() const noexcept { return spec_.fixedSize; }

bool SequenceType::isBounded() const noexcept { return getMaxSize().has_value(); }

std::string_view SequenceType::getQualifiedName() const noexcept { return spec_.qualifiedName; }

std::string_view SequenceType::getName() const noexcept { return spec_.name; }

std::string_view SequenceType::getDescription() const noexcept { return spec_.description; }

bool SequenceType::equals(const Type& other) const noexcept
{
  return &other == this || (other.isSequenceType() && spec_ == other.asSequenceType()->spec_);
}

}  // namespace sen
