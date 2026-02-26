// === sequence_type.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_SEQUENCE_TYPE_H
#define SEN_CORE_META_SEQUENCE_TYPE_H

// sen
#include "sen/core/base/hash32.h"
#include "sen/core/meta/custom_type.h"  // for CustomType
#include "sen/core/meta/type.h"         // for Type (ptr only)

// std
#include <cstddef>
#include <optional>     // for optional
#include <string>       // for string
#include <string_view>  // for string_view
#include <utility>

namespace sen
{

/// \addtogroup types
/// @{

/// Data of a sequence type.
struct SequenceSpec final
{
  SequenceSpec(std::string name,
               std::string qualifiedName,
               std::string description,
               ConstTypeHandle<> elementType,
               std::optional<std::size_t> maxSize = std::nullopt,
               bool fixedSize = false)
    : name(std::move(name))
    , qualifiedName(std::move(qualifiedName))
    , description(std::move(description))
    , elementType(std::move(elementType))
    , maxSize(maxSize)
    , fixedSize(fixedSize)
  {
  }

  // Comparison operators
  friend bool operator==(const SequenceSpec& lhs, const SequenceSpec& rhs) noexcept
  {
    return lhs.name == rhs.name && lhs.qualifiedName == rhs.qualifiedName && lhs.description == rhs.description &&
           lhs.maxSize == rhs.maxSize && lhs.fixedSize == rhs.fixedSize && lhs.elementType == rhs.elementType;
  }

  friend bool operator!=(const SequenceSpec& lhs, const SequenceSpec& rhs) noexcept { return !(lhs == rhs); }

public:
  std::string name;                    // NOLINT(misc-non-private-member-variables-in-classes)
  std::string qualifiedName;           // NOLINT(misc-non-private-member-variables-in-classes)
  std::string description;             // NOLINT(misc-non-private-member-variables-in-classes)
  ConstTypeHandle<> elementType;       // NOLINT(misc-non-private-member-variables-in-classes)
  std::optional<std::size_t> maxSize;  // NOLINT(misc-non-private-member-variables-in-classes)
  bool fixedSize = false;              // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Represents a sequence type.
class SequenceType final: public CustomType
{
  SEN_META_TYPE(SequenceType)
  SEN_PRIVATE_TAG

public:
  /// Copies the spec into a member.
  explicit SequenceType(SequenceSpec spec, Private notUsable);
  ~SequenceType() override = default;

public:
  /// Factory function that validates the spec and creates a sequence type.
  /// Throws std::exception if the spec is not valid.
  [[nodiscard]] static TypeHandle<SequenceType> make(SequenceSpec spec);

  /// Gets the type of the stored elements.
  [[nodiscard]] ConstTypeHandle<> getElementType() const noexcept;

  /// Gets the maximum size of the sequence (if any).
  [[nodiscard]] std::optional<std::size_t> getMaxSize() const noexcept;

  /// True if the sequence has a fixed size
  [[nodiscard]] bool hasFixedSize() const noexcept;

public:  // Type
  [[nodiscard]] std::string_view getQualifiedName() const noexcept override;
  [[nodiscard]] std::string_view getName() const noexcept override;
  [[nodiscard]] std::string_view getDescription() const noexcept override;
  [[nodiscard]] bool equals(const Type& other) const noexcept override;
  [[nodiscard]] bool isBounded() const noexcept override;

private:
  SequenceSpec spec_;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

/// Hash for the SequenceSpec
template <>
struct impl::hash<SequenceSpec>
{
  u32 operator()(const SequenceSpec& spec) const noexcept
  {
    return hashCombine(hashSeed,
                       spec.name,
                       spec.qualifiedName,
                       spec.elementType->getHash(),
                       spec.maxSize.has_value() ? spec.maxSize.value() : nonPresentTypeHash,
                       spec.fixedSize);
  }
};

}  // namespace sen

#endif  // SEN_CORE_META_SEQUENCE_TYPE_H
