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
  /// @param name        Field name (must be unique within the struct).
  /// @param description Human-readable description of the field's purpose.
  /// @param type        Type handle describing the field's value type.
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
  std::string name;         ///< Field name. NOLINT(misc-non-private-member-variables-in-classes)
  std::string description;  ///< Human-readable description. NOLINT(misc-non-private-member-variables-in-classes)
  ConstTypeHandle<> type;   ///< Type of the field value. NOLINT(misc-non-private-member-variables-in-classes)
};

/// A function that extracts a native value from a field
using NativeFieldValueGetter = std::function<lang::Value(const void*)>;

/// Descriptor used to construct a `StructType` instance.
struct StructSpec final
{
  /// @param name          Short type name (e.g. `"Point"`).
  /// @param qualifiedName Fully-qualified name including namespace (e.g. `"ns.Point"`).
  /// @param description   Human-readable description of the struct.
  /// @param fields        Ordered list of fields belonging to this struct.
  /// @param parent        Optional handle to a parent struct whose fields are inherited.
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
  std::string name;           ///< Short type name. NOLINT(misc-non-private-member-variables-in-classes)
  std::string qualifiedName;  ///< Fully-qualified type name. NOLINT(misc-non-private-member-variables-in-classes)
  std::string description;    ///< Human-readable description. NOLINT(misc-non-private-member-variables-in-classes)
  std::vector<StructField> fields;  ///< Ordered list of fields. NOLINT(misc-non-private-member-variables-in-classes)
  MaybeConstTypeHandle<StructType>
    parent;  ///< Optional parent struct type. NOLINT(misc-non-private-member-variables-in-classes)
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
  /// @param spec Descriptor containing name, fields, and optional parent.
  /// @return Owning handle to the newly created `StructType`.
  /// @throws std::exception if the spec is not valid (e.g. duplicate field names or empty name).
  [[nodiscard]] static TypeHandle<StructType> make(StructSpec spec);

public:
  /// Looks up a field by name (own fields only, no parent traversal).
  /// @param name The field name to search for.
  /// @return Pointer to the matching `StructField`, or `nullptr` if not found.
  [[nodiscard]] const StructField* getFieldFromName(std::string_view name) const;

  /// @return Read-only span over this struct's own fields (does not include parent fields).
  [[nodiscard]] Span<const StructField> getFields() const noexcept;

  /// @return All fields including those inherited from parent structs, in declaration order.
  [[nodiscard]] std::vector<StructField> getAllFields() const noexcept;

  /// @return Handle to the parent struct, or `std::nullopt` if this is a root struct.
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
