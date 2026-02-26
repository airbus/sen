// === alias_type.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_ALIAS_TYPE_H
#define SEN_CORE_META_ALIAS_TYPE_H

// sen
#include "sen/core/base/hash32.h"
#include "sen/core/meta/custom_type.h"  // for CustomType
#include "sen/core/meta/type.h"         // for Type (ptr only), ...

// std
#include <string>       // for string
#include <string_view>  // for string_view
#include <utility>

namespace sen
{

/// \addtogroup types
/// @{

/// Data of an alias type.
struct AliasSpec final
{
  AliasSpec(std::string name, std::string qualifiedName, std::string description, ConstTypeHandle<> aliasedType)
    : name(std::move(name))
    , qualifiedName(std::move(qualifiedName))
    , description(std::move(description))
    , aliasedType(std::move(aliasedType))
  {
  }

  // Comparison operators
  friend bool operator==(const AliasSpec& lhs, const AliasSpec& rhs) noexcept
  {
    return lhs.name == rhs.name && lhs.qualifiedName == rhs.qualifiedName && lhs.description == rhs.description &&
           lhs.aliasedType == rhs.aliasedType;
  }

  friend bool operator!=(const AliasSpec& lhs, const AliasSpec& rhs) noexcept { return !(lhs == rhs); }

public:
  std::string name;               // NOLINT(misc-non-private-member-variables-in-classes)
  std::string qualifiedName;      // NOLINT(misc-non-private-member-variables-in-classes)
  std::string description;        // NOLINT(misc-non-private-member-variables-in-classes)
  ConstTypeHandle<> aliasedType;  // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Represents an aliased type.
class AliasType final: public CustomType
{
  SEN_META_TYPE(AliasType)
  SEN_PRIVATE_TAG

public:  // special members
  /// Copies the spec into a member.
  AliasType(AliasSpec spec, Private notUsable);
  ~AliasType() override = default;

public:
  /// Factory function that validates the spec and creates an alias type.
  /// Throws std::exception if the spec is not valid.
  [[nodiscard]] static TypeHandle<AliasType> make(AliasSpec spec);

  /// Gets the underlying type.
  [[nodiscard]] ConstTypeHandle<> getAliasedType() const noexcept;

public:  // Type
  [[nodiscard]] std::string_view getQualifiedName() const noexcept override;
  [[nodiscard]] std::string_view getName() const noexcept override;
  [[nodiscard]] std::string_view getDescription() const noexcept override;
  [[nodiscard]] bool equals(const Type& other) const noexcept override;
  [[nodiscard]] bool isBounded() const noexcept override;

private:
  AliasSpec spec_;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

/// Hash for the AliasSpec
template <>
struct impl::hash<AliasSpec>
{
  u32 operator()(const AliasSpec& spec) const noexcept
  {
    return hashCombine(hashSeed, spec.name, spec.qualifiedName, spec.aliasedType->getHash());
  }
};

}  // namespace sen

#endif  // SEN_CORE_META_ALIAS_TYPE_H
