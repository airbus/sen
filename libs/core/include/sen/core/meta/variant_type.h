// === variant_type.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_VARIANT_TYPE_H
#define SEN_CORE_META_VARIANT_TYPE_H

#include "sen/core/base/assert.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/span.h"         // for Span
#include "sen/core/meta/custom_type.h"  // for CustomType
#include "sen/core/meta/type.h"         // for Type (ptr ...

// std
#include <cstdint>
#include <memory>       // for shared_ptr
#include <string>       // for string
#include <string_view>  // for string_view
#include <utility>
#include <vector>  // for vector

namespace sen
{

/// \addtogroup types
/// @{

/// Holds the information for a field in the variant.
struct VariantField final
{
  /// @param key         Discriminant value that selects this field.
  /// @param description Human-readable description of the field's purpose.
  /// @param type        Type handle describing the field's value type (must not be void).
  VariantField(uint32_t key, std::string description, ConstTypeHandle<> type)
    : key(key), description(std::move(description)), type(std::move(type))
  {
    SEN_ENSURE(!this->type->isVoidType() && "Variants cannot have void fields.");
  }

  // Comparison operators
  friend bool operator==(const VariantField& lhs, const VariantField& rhs) noexcept
  {
    return lhs.key == rhs.key && lhs.description == rhs.description && lhs.type == rhs.type;
  }

  friend bool operator!=(const VariantField& lhs, const VariantField& rhs) noexcept { return !(lhs == rhs); }

public:
  uint32_t key =
    0;  ///< Discriminant value that selects this alternative. NOLINT(misc-non-private-member-variables-in-classes)
  std::string description;  ///< Human-readable description of this alternative.
                            ///< NOLINT(misc-non-private-member-variables-in-classes)
  ConstTypeHandle<>
    type;  ///< Type of the value carried by this alternative. NOLINT(misc-non-private-member-variables-in-classes)
};

/// Data of a variant type.
struct VariantSpec final
{
  // Comparison operators
  friend bool operator==(const VariantSpec& lhs, const VariantSpec& rhs) noexcept
  {
    return lhs.name == rhs.name && lhs.qualifiedName == rhs.qualifiedName && lhs.description == rhs.description &&
           lhs.fields == rhs.fields;
  }

  friend bool operator!=(const VariantSpec& lhs, const VariantSpec& rhs) noexcept { return !(lhs == rhs); }

public:
  std::string name;                  ///< Short type name (e.g. `"Shape"`).
  std::string qualifiedName;         ///< Fully-qualified type name (e.g. `"ns.Shape"`).
  std::string description;           ///< Human-readable description of the variant.
  std::vector<VariantField> fields;  ///< Ordered list of alternative fields.
};

/// Represents a variant type.
class VariantType final: public CustomType
{
  SEN_META_TYPE(VariantType)
  SEN_PRIVATE_TAG

public:  // special members
  /// Copies the spec into a member.
  /// Private to prevent direct instances.
  VariantType(VariantSpec spec, Private notUsable);
  ~VariantType() override = default;

public:
  /// Factory function that validates the spec and creates a variant type.
  /// @param spec Descriptor containing the name and alternative fields.
  /// @return Owning handle to the newly created `VariantType`.
  /// @throws std::exception if the spec is not valid (e.g. duplicate keys or empty name).
  [[nodiscard]] static TypeHandle<VariantType> make(VariantSpec spec);

public:
  /// Looks up an alternative by its discriminant key.
  /// @param key The discriminant value to search for.
  /// @return Pointer to the matching `VariantField`, or `nullptr` if not found.
  [[nodiscard]] const VariantField* getFieldFromKey(uint32_t key) const noexcept;

  /// @return Read-only span over the full list of alternative fields.
  [[nodiscard]] Span<const VariantField> getFields() const noexcept;

public:  // Type
  [[nodiscard]] std::string_view getQualifiedName() const noexcept override;
  [[nodiscard]] std::string_view getName() const noexcept override;
  [[nodiscard]] std::string_view getDescription() const noexcept override;
  [[nodiscard]] bool equals(const Type& other) const noexcept override;
  [[nodiscard]] bool isBounded() const noexcept override;

private:
  VariantSpec spec_;
  bool bounded_ = true;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

/// Hash for the VariantField
template <>
struct impl::hash<VariantField>
{
  u32 operator()(const VariantField& spec) const noexcept
  {
    return hashCombine(hashSeed, spec.key, spec.type->getHash());
  }
};

/// Hash for the VariantSpec
template <>
struct impl::hash<VariantSpec>
{
  u32 operator()(const VariantSpec& spec) const noexcept
  {
    auto result = hashCombine(hashSeed, spec.name, spec.qualifiedName);

    for (const auto& field: spec.fields)
    {
      result = hashCombine(result, field);
    }

    return result;
  }
};

}  // namespace sen

#endif  // SEN_CORE_META_VARIANT_TYPE_H
