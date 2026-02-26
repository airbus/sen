// === struct_type.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_STRUCT_TYPE_H
#define SEN_CORE_META_STRUCT_TYPE_H

// sen
#include "sen/core/base/hash32.h"
#include "sen/core/base/span.h"
#include "sen/core/lang/vm.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/type.h"

// std
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sen
{

/// \addtogroup types
/// @{

class StructType;

/// Holds the information for a field in the struct.
struct StructField final
{
  StructField(std::string name, std::string description, ConstTypeHandle<> type)
    : name(std::move(name)), description(std::move(description)), type(std::move(type))
  {
  }

  // Comparison operators
  friend bool operator==(const StructField& lhs, const StructField& rhs) noexcept
  {
    return lhs.name == rhs.name && lhs.description == rhs.description && lhs.type == rhs.type;
  }

  friend bool operator!=(const StructField& lhs, const StructField& rhs) noexcept { return !(lhs == rhs); }

public:
  std::string name;         // NOLINT(misc-non-private-member-variables-in-classes)
  std::string description;  // NOLINT(misc-non-private-member-variables-in-classes)
  ConstTypeHandle<> type;   // NOLINT(misc-non-private-member-variables-in-classes)
};

/// A function that extracts a native value from a field
using NativeFieldValueGetter = std::function<lang::Value(const void*)>;

/// Data of struct type.
struct StructSpec final
{
  StructSpec(std::string name,
             std::string qualifiedName,
             std::string description,
             std::vector<StructField> fields,
             MaybeConstTypeHandle<StructType> parent = std::nullopt)
    : name(std::move(name))
    , qualifiedName(std::move(qualifiedName))
    , description(std::move(description))
    , fields(std::move(fields))
    , parent(std::move(parent))
  {
  }

  // Comparison operators
  friend bool operator==(const StructSpec& lhs, const StructSpec& rhs) noexcept
  {
    return lhs.name == rhs.name && lhs.qualifiedName == rhs.qualifiedName && lhs.description == rhs.description &&
           lhs.fields == rhs.fields && lhs.parent == rhs.parent;
  }

  friend bool operator!=(const StructSpec& lhs, const StructSpec& rhs) noexcept { return !(lhs == rhs); }

public:
  std::string name;                         // NOLINT(misc-non-private-member-variables-in-classes)
  std::string qualifiedName;                // NOLINT(misc-non-private-member-variables-in-classes)
  std::string description;                  // NOLINT(misc-non-private-member-variables-in-classes)
  std::vector<StructField> fields;          // NOLINT(misc-non-private-member-variables-in-classes)
  MaybeConstTypeHandle<StructType> parent;  // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Represents a structure type.
class StructType final: public CustomType
{
  SEN_META_TYPE(StructType)
  SEN_PRIVATE_TAG

public:  // special members
  /// Moves the spec into a member.
  StructType(StructSpec spec, Private notUsable);
  ~StructType() override = default;

public:
  /// Factory function that validates the spec and creates a struct type.
  /// Throws std::exception if the spec is not valid.
  [[nodiscard]] static TypeHandle<StructType> make(StructSpec spec);

public:
  /// Get the field data given a name.. Nullptr means not found.
  /// @param name the name of the field.
  [[nodiscard]] const StructField* getFieldFromName(std::string_view name) const;

  /// Gets the fields.
  [[nodiscard]] Span<const StructField> getFields() const noexcept;

  /// Gets the fields including parents in the search
  [[nodiscard]] std::vector<StructField> getAllFields() const noexcept;

  /// Gets the parent struct, if any.
  [[nodiscard]] MaybeConstTypeHandle<StructType> getParent() const noexcept;

public:  // Type
  [[nodiscard]] std::string_view getQualifiedName() const noexcept override;
  [[nodiscard]] std::string_view getName() const noexcept override;
  [[nodiscard]] std::string_view getDescription() const noexcept override;
  [[nodiscard]] bool equals(const Type& other) const noexcept override;
  [[nodiscard]] bool isBounded() const noexcept override;

private:
  StructSpec spec_;
  bool bounded_ = true;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

/// Hash for the StructField
template <>
struct impl::hash<StructField>
{
  u32 operator()(const StructField& spec) const noexcept
  {
    return hashCombine(hashSeed, spec.name, spec.type->getHash());
  }
};

/// Hash for the StructSpec
template <>
struct impl::hash<StructSpec>
{
  u32 operator()(const StructSpec& spec) const noexcept
  {
    auto result = hashCombine(
      hashSeed, spec.name, spec.qualifiedName, spec.parent ? spec.parent.value()->getHash() : nonPresentTypeHash);

    for (const auto& field: spec.fields)
    {
      result = hashCombine(result, field);
    }

    return result;
  }
};

}  // namespace sen

#endif  // SEN_CORE_META_STRUCT_TYPE_H
