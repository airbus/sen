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
  std::string name;         ///< Display name of the enumerator (e.g. `"red"`).
  uint32_t key = 0;         ///< Numeric value associated with this enumerator.
  std::string description;  ///< Human-readable description of the enumerator.
};

/// Descriptor used to construct an `EnumType` instance.
struct EnumSpec final
{
  /// @param name          Short type name (e.g. `"Color"`).
  /// @param qualifiedName Fully-qualified name including namespace (e.g. `"ns.Color"`).
  /// @param description   Human-readable description of the enumeration.
  /// @param enums         List of enumerators that belong to this type.
  /// @param storageType   Integral type used to store the enumerator values on the wire.
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
  std::string name;           ///< Short type name.  // NOLINT(misc-non-private-member-variables-in-classes)
  std::string qualifiedName;  ///< Fully-qualified type name.  // NOLINT(misc-non-private-member-variables-in-classes)
  std::string description;    ///< Human-readable description.  // NOLINT(misc-non-private-member-variables-in-classes)
  std::vector<Enumerator> enums;  ///< List of enumerators.  // NOLINT(misc-non-private-member-variables-in-classes)
  ConstTypeHandle<IntegralType>
    storageType;  ///< Underlying integral storage type.  // NOLINT(misc-non-private-member-variables-in-classes)
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
  /// Factory function that validates the spec and creates an enum type.
  /// @param spec Descriptor containing name, enumerators, and storage type.
  /// @return Owning handle to the newly created `EnumType`.
  /// @throws std::exception if the spec is not valid (e.g. duplicate keys or empty name).
  [[nodiscard]] static TypeHandle<EnumType> make(EnumSpec spec);

public:
  /// @return The integral type used to store this enumeration's values on the wire.
  [[nodiscard]] const IntegralType& getStorageType() const noexcept;

  /// Looks up an enumerator by its numeric key.
  /// @param key The enumerator value to search for.
  /// @return Pointer to the matching `Enumerator`, or `nullptr` if not found.
  [[nodiscard]] const Enumerator* getEnumFromKey(uint32_t key) const noexcept;

  /// Looks up an enumerator by name.
  /// @param name The enumerator name to search for.
  /// @return Pointer to the matching `Enumerator`, or `nullptr` if not found.
  [[nodiscard]] const Enumerator* getEnumFromName(std::string_view name) const noexcept;

  /// @return Read-only span over the full list of enumerators.
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
