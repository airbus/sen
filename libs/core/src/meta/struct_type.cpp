// === struct_type.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/meta/struct_type.h"

// implementation
#include "utils.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/type.h"

// std
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace sen
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

namespace
{

void checkLocalFields(const StructSpec& spec)
{
  const auto fieldCount = spec.fields.size();

  // all fields shall have unique names and unique keys
  for (std::size_t i = 0U; i < fieldCount; i++)
  {
    const auto& currentField = spec.fields.at(i);

    impl::checkMemberName(currentField.name);

    for (std::size_t j = 0U; j < fieldCount; j++)
    {
      if (j != i)
      {
        const auto& otherField = spec.fields.at(j);

        // check the name
        if (otherField.name == currentField.name)
        {
          std::string err;
          err.append("repeated struct field name '");
          err.append(currentField.name);
          err.append("'");
          throwRuntimeError(err);
        }
      }
    }
  }
}

void checkParentFields(const StructSpec& spec, const StructType& parent)
{
  for (const auto& ourField: spec.fields)
  {
    for (const auto& parentField: parent.getFields())
    {
      if (ourField.name == parentField.name)
      {
        std::string err;
        err.append("parent structure '");
        err.append(parent.getQualifiedName());
        err.append("' already has a field named '");
        err.append(ourField.name);
        err.append("'");
        throwRuntimeError(err);
      }
    }
  }
}

void checkFields(const StructSpec& spec)
{
  checkLocalFields(spec);

  // if the struct has parents, fields shall have different names
  decltype(spec.parent) parent {std::nullopt};  // Workaround for gcc static analyzer bug
  parent = spec.parent;
  while (parent)
  {
    checkParentFields(spec, *parent.value().type());
    parent = parent.value()->getParent();
  }
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// StructType
//--------------------------------------------------------------------------------------------------------------

StructType::StructType(StructSpec spec, Private notUsable)
  : CustomType(impl::hash<StructSpec>()(spec)), spec_(std::move(spec))
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

TypeHandle<StructType> StructType::make(StructSpec spec)
{
  impl::checkUserTypeName(spec.name);
  impl::checkIdentifier(spec.qualifiedName, true);

  checkFields(spec);

  return {std::move(spec), Private {}};
}

const StructField* StructType::getFieldFromName(std::string_view name) const
{
  if (name.empty())
  {
    return nullptr;
  }

  for (const auto& item: spec_.fields)
  {
    if (item.name == name)
    {
      return &item;
    }
  }

  if (spec_.parent)
  {
    return spec_.parent.value()->getFieldFromName(name);
  }

  return nullptr;
}

Span<const StructField> StructType::getFields() const noexcept { return makeConstSpan(spec_.fields); }

std::vector<StructField> StructType::getAllFields() const noexcept
{
  auto allFields(spec_.fields);

  auto parent = getParent();
  while (parent)
  {
    allFields.insert(allFields.end(), parent.value()->getFields().begin(), parent.value()->getFields().end());
    parent = parent.value()->getParent();
  }

  return allFields;
}

std::string_view StructType::getQualifiedName() const noexcept { return spec_.qualifiedName; }

std::string_view StructType::getName() const noexcept { return spec_.name; }

std::string_view StructType::getDescription() const noexcept { return spec_.description; }

MaybeConstTypeHandle<StructType> StructType::getParent() const noexcept { return spec_.parent; }

bool StructType::equals(const Type& other) const noexcept
{
  return &other == this || (other.isStructType() && spec_ == other.asStructType()->spec_);
}

bool StructType::isBounded() const noexcept { return bounded_; }

}  // namespace sen
