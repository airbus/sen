// === enum_type.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_ENUM_TYPE_H
#define SEN_CORE_META_ENUM_TYPE_H

// sen
#include "sen/core/base/hash32.h"
#include "sen/core/base/span.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/type.h"

// std
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sen
{

/// \addtogroup types
/// @{

/// Holds the information for an enumerator.
struct Enumerator final
{
  // Comparison operators
  friend bool operator==(const Enumerator& lhs, const Enumerator& rhs) noexcept
  {
    return &lhs == &rhs || (lhs.key == rhs.key && lhs.name == rhs.name && lhs.description == rhs.description);
  }

  friend bool operator!=(const Enumerator& lhs, const Enumerator& rhs) noexcept { return !(lhs == rhs); }

public:
  std::string name;         /// The name of the enumerator
  uint32_t key = 0;         /// The key of the enumerator
  std::string description;  /// The description of the enumerator
};

/// Data of an enum type.
struct EnumSpec final
{
  EnumSpec(std::string name,
           std::string qualifiedName,
           std::string description,
           std::vector<Enumerator> enums,
           ConstTypeHandle<IntegralType> storageType)
    : name(std::move(name))
    , qualifiedName(std::move(qualifiedName))
    , description(std::move(description))
    , enums(std::move(enums))
    , storageType(std::move(storageType))
  {
  }

  // Comparison operators
  friend bool operator==(const EnumSpec& lhs, const EnumSpec& rhs) noexcept
  {
    return &lhs == &rhs ||
           (lhs.name == rhs.name && lhs.qualifiedName == rhs.qualifiedName && lhs.description == rhs.description &&
            lhs.enums == rhs.enums && lhs.storageType == rhs.storageType);
  }

  friend bool operator!=(const EnumSpec& lhs, const EnumSpec& rhs) noexcept { return !(lhs == rhs); }

public:
  std::string name;                           // NOLINT(misc-non-private-member-variables-in-classes)
  std::string qualifiedName;                  // NOLINT(misc-non-private-member-variables-in-classes)
  std::string description;                    // NOLINT(misc-non-private-member-variables-in-classes)
  std::vector<Enumerator> enums;              // NOLINT(misc-non-private-member-variables-in-classes)
  ConstTypeHandle<IntegralType> storageType;  // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Represents an enumeration.
class EnumType final: public CustomType
{
  SEN_META_TYPE(EnumType)
  SEN_PRIVATE_TAG

public:  // special members
  /// Copies the spec into a member.
  /// Private to prevent direct instances.
  EnumType(EnumSpec spec, Private notUsable);

  ~EnumType() override = default;

public:
  /// Factory function that validates the spec and creates a enum type.
  /// Throws std::exception if the spec is not valid.
  [[nodiscard]] static TypeHandle<EnumType> make(EnumSpec spec);

public:
  /// Gets the type used to store the enumeration's values.
  [[nodiscard]] const IntegralType& getStorageType() const noexcept;

  /// Get the enumerator data using a key. Nullptr means not found.
  /// @param key the enumerator's value.
  [[nodiscard]] const Enumerator* getEnumFromKey(uint32_t key) const noexcept;

  /// Get the enumerator data using a name. Nullptr means not found.
  /// @param name the name of the enumerator.
  [[nodiscard]] const Enumerator* getEnumFromName(std::string_view name) const noexcept;

  /// Gets the enumerators lists.
  /// @return the enumerators lists.
  [[nodiscard]] Span<const Enumerator> getEnums() const noexcept;

public:  // Type
  [[nodiscard]] std::string_view getQualifiedName() const noexcept override;
  [[nodiscard]] std::string_view getName() const noexcept override;
  [[nodiscard]] std::string_view getDescription() const noexcept override;
  [[nodiscard]] bool equals(const Type& other) const noexcept override;
  [[nodiscard]] bool isBounded() const noexcept override;

private:
  EnumSpec spec_;
  std::unordered_map<u32, const Enumerator*> keyToEnumMap_;
  std::unordered_map<std::string, const Enumerator*> nameToEnumMap_;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

/// Hash for the Enumerator
template <>
struct impl::hash<Enumerator>
{
  u32 operator()(const Enumerator& spec) const noexcept { return hashCombine(hashSeed, spec.name, spec.key); }
};

/// Hash for the EnumSpec
template <>
struct impl::hash<EnumSpec>
{
  u32 operator()(const EnumSpec& spec) const noexcept
  {
    auto result = hashCombine(hashSeed, spec.name, spec.qualifiedName, spec.storageType->getHash());

    for (const auto& elem: spec.enums)
    {
      result = hashCombine(result, elem);
    }

    return result;
  }
};

}  // namespace sen

#endif  // SEN_CORE_META_ENUM_TYPE_H
