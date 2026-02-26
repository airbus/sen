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
  uint32_t key = 0;         // NOLINT(misc-non-private-member-variables-in-classes)
  std::string description;  // NOLINT(misc-non-private-member-variables-in-classes)
  ConstTypeHandle<> type;   // NOLINT(misc-non-private-member-variables-in-classes)
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
  std::string name;
  std::string qualifiedName;
  std::string description;
  std::vector<VariantField> fields;
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
  /// Throws std::exception if the spec is not valid.
  [[nodiscard]] static TypeHandle<VariantType> make(VariantSpec spec);

public:
  /// Get the field data using a key. Nullptr means not found.
  /// @param key the field's value.
  [[nodiscard]] const VariantField* getFieldFromKey(uint32_t key) const noexcept;

  /// Gets the fields.
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
