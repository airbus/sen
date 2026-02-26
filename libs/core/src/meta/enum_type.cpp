// === enum_type.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/enum_type.h"

// implementation
#include "utils.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/type.h"

// std
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

//--------------------------------------------------------------------------------------------------------------
// Free functions
//--------------------------------------------------------------------------------------------------------------

namespace sen
{

//--------------------------------------------------------------------------------------------------------------
// EnumType
//--------------------------------------------------------------------------------------------------------------

EnumType::EnumType(EnumSpec spec, Private notUsable): CustomType(impl::hash<EnumSpec>()(spec)), spec_(std::move(spec))
{
  std::ignore = notUsable;

  const auto enumSize = spec_.enums.size();
  keyToEnumMap_.reserve(enumSize);
  nameToEnumMap_.reserve(enumSize);

  for (const auto& enumerator: spec_.enums)
  {
    keyToEnumMap_.emplace(enumerator.key, &enumerator);
    nameToEnumMap_.emplace(enumerator.name, &enumerator);
  }
}

const IntegralType& EnumType::getStorageType() const noexcept { return *spec_.storageType; }

Span<const Enumerator> EnumType::getEnums() const noexcept { return makeConstSpan(spec_.enums); }

std::string_view EnumType::getQualifiedName() const noexcept { return spec_.qualifiedName; }

std::string_view EnumType::getName() const noexcept { return spec_.name; }

std::string_view EnumType::getDescription() const noexcept { return spec_.description; }

bool EnumType::equals(const Type& other) const noexcept
{
  return &other == this || (other.isEnumType() && spec_ == other.asEnumType()->spec_);
}

bool EnumType::isBounded() const noexcept { return true; }

TypeHandle<EnumType> EnumType::make(EnumSpec spec)
{
  impl::checkUserTypeName(spec.name);
  impl::checkIdentifier(spec.qualifiedName, true);

  if (spec.enums.empty())
  {
    throwRuntimeError("no enumerators");
  }

  const auto enumCount = spec.enums.size();

  // all enums shall have unique names and unique keys
  for (std::size_t i = 0U; i < enumCount; i++)
  {
    const auto& currentEnum = spec.enums.at(i);

    for (std::size_t j = 0U; j < enumCount; j++)
    {
      if (j != i)
      {
        const auto& otherEnum = spec.enums.at(j);

        // check the key
        if (otherEnum.key == currentEnum.key)
        {
          std::string err;
          err.append("enumerator ");
          err.append(currentEnum.name);
          err.append(" and ");
          err.append(otherEnum.name);
          err.append(" have the same key ");
          err.append(std::to_string(currentEnum.key));
          throwRuntimeError(err);
        }

        // check the name
        if (otherEnum.name == currentEnum.name)
        {
          std::string err;
          err.append("enumerators with keys ");
          err.append(std::to_string(currentEnum.key));
          err.append(" and ");
          err.append(std::to_string(otherEnum.key));
          err.append(" have the same name '");
          err.append(currentEnum.name);
          err.append("'");
          throwRuntimeError(err);
        }
      }
    }
  }

  return TypeHandle<EnumType> {std::move(spec), Private {}};
}

const Enumerator* EnumType::getEnumFromKey(uint32_t key) const noexcept
{
  if (const auto& itr = keyToEnumMap_.find(key); itr != keyToEnumMap_.end())
  {
    return itr->second;
  }

  return nullptr;
}

const Enumerator* EnumType::getEnumFromName(std::string_view name) const noexcept
{
  if (name.empty())
  {
    return nullptr;
  }

  if (const auto& itr = nameToEnumMap_.find(name.data()); itr != nameToEnumMap_.end())
  {
    return itr->second;
  }

  return nullptr;
}

}  // namespace sen
