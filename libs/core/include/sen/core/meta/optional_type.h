// === optional_type.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_OPTIONAL_TYPE_H
#define SEN_CORE_META_OPTIONAL_TYPE_H

// sen
#include "sen/core/base/hash32.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/type.h"

// std
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace sen
{

/// \addtogroup types
/// @{

/// Data of an optional type.
struct OptionalSpec final
{
  OptionalSpec(std::string name, std::string qualifiedName, std::string description, ConstTypeHandle<> type)
    : name(std::move(name))
    , qualifiedName(std::move(qualifiedName))
    , description(std::move(description))
    , type(std::move(type))
  {
  }

  // Comparison operators
  friend bool operator==(const OptionalSpec& lhs, const OptionalSpec& rhs) noexcept
  {
    return lhs.name == rhs.name && lhs.qualifiedName == rhs.qualifiedName && lhs.description == rhs.description &&
           lhs.type == rhs.type;
  }

  friend bool operator!=(const OptionalSpec& lhs, const OptionalSpec& rhs) noexcept { return !(lhs == rhs); }

  std::string name;           // NOLINT(misc-non-private-member-variables-in-classes)
  std::string qualifiedName;  // NOLINT(misc-non-private-member-variables-in-classes)
  std::string description;    // NOLINT(misc-non-private-member-variables-in-classes)
  ConstTypeHandle<> type;     // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Represents an optional type.
class OptionalType final: public CustomType
{
  SEN_META_TYPE(OptionalType)
  SEN_PRIVATE_TAG

public:  // special members
  /// Copies the spec into a member.
  OptionalType(OptionalSpec spec, Private notUsable);
  ~OptionalType() override = default;

public:
  /// Factory function that validates the spec and creates an alias type.
  /// Throws std::exception if the spec is not valid.
  [[nodiscard]] static TypeHandle<OptionalType> make(OptionalSpec spec);

  /// Gets the underlying type.
  [[nodiscard]] ConstTypeHandle<> getType() const noexcept;

public:  // Type
  [[nodiscard]] std::string_view getQualifiedName() const noexcept override;
  [[nodiscard]] std::string_view getName() const noexcept override;
  [[nodiscard]] std::string_view getDescription() const noexcept override;
  [[nodiscard]] bool equals(const Type& other) const noexcept override;
  [[nodiscard]] bool isBounded() const noexcept override;

private:
  OptionalSpec spec_;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

/// Hash for the OptionalSpec
template <>
struct impl::hash<OptionalSpec>
{
  u32 operator()(const OptionalSpec& spec) const noexcept
  {
    return hashCombine(hashSeed, spec.name, spec.qualifiedName, spec.type->getHash());
  }
};

}  // namespace sen

#endif  // SEN_CORE_META_OPTIONAL_TYPE_H
