// === variant_type.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/variant_type.h"

// implementation
#include "utils.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/type.h"

// std
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace sen
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

void checkFields(const VariantSpec& spec)
{
  const auto fieldCount = spec.fields.size();

  // all fields shall have unique types and unique keys
  for (std::size_t i = 0U; i < fieldCount; i++)
  {
    const auto& currentField = spec.fields.at(i);

    for (std::size_t j = 0U; j < fieldCount; j++)
    {
      if (j != i)
      {
        const auto& otherField = spec.fields.at(j);

        // check the key
        if (otherField.key == currentField.key)
        {
          std::string err;
          err.append("fields of type ");
          err.append(currentField.type->getName());
          err.append(" and ");
          err.append(otherField.type->getName());
          err.append(" have the same key ");
          err.append(std::to_string(currentField.key));
          sen::throwRuntimeError(err);
        }
      }
    }
  }
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// VariantType
//--------------------------------------------------------------------------------------------------------------

VariantType::VariantType(VariantSpec spec, Private notUsable)
  : CustomType(impl::hash<VariantSpec>()(spec)), spec_(std::move(spec))
{
  std::ignore = notUsable;

  for (const auto& field: spec_.fields)
  {
    if (!field.type->isBounded())
    {
      bounded_ = false;
      break;
    }
  }
}

TypeHandle<VariantType> VariantType::make(VariantSpec spec)
{
  impl::checkUserTypeName(spec.name);
  impl::checkIdentifier(spec.qualifiedName, true);

  checkFields(spec);

  return {std::move(spec), Private {}};
}

const VariantField* VariantType::getFieldFromKey(uint32_t key) const noexcept
{
  for (const auto& item: spec_.fields)
  {
    if (item.key == key)
    {
      return &item;
    }
  }

  return nullptr;
}

Span<const VariantField> VariantType::getFields() const noexcept { return makeConstSpan(spec_.fields); }

std::string_view VariantType::getQualifiedName() const noexcept { return spec_.qualifiedName; }

std::string_view VariantType::getName() const noexcept { return spec_.name; }

std::string_view VariantType::getDescription() const noexcept { return spec_.description; }

bool VariantType::equals(const Type& other) const noexcept
{
  return &other == this || (other.isVariantType() && spec_ == other.asVariantType()->spec_);
}

bool VariantType::isBounded() const noexcept { return bounded_; }

}  // namespace sen
