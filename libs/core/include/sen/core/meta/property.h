// === property.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_PROPERTY_H
#define SEN_CORE_META_PROPERTY_H

#include "sen/core/base/class_helpers.h"
#include "sen/core/meta/event.h"
#include "sen/core/meta/method.h"
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

/// How a property is seen by others.
enum class PropertyCategory
{
  staticRO,   ///< Does not change over time, can only be set via code.
  staticRW,   ///< Does not change over time, can be set via code and configuration.
  dynamicRW,  ///< May change over time, can be set from outside.
  dynamicRO,  ///< May change over time, cannot be set from outside.
};

/// Data of a property.
struct PropertySpec final
{
  PropertySpec(std::string name,
               std::string description,
               ConstTypeHandle<> type,
               PropertyCategory category = PropertyCategory::dynamicRO,
               TransportMode transportMode = TransportMode::multicast,
               bool checkedSet = false,
               std::vector<std::string> tags = {})
    : name(std::move(name))
    , description(std::move(description))
    , category(category)
    , type(std::move(type))
    , transportMode(transportMode)
    , tags(std::move(tags))
    , checkedSet(checkedSet)
  {
  }

  // Comparison operators
  friend bool operator==(const PropertySpec& lhs, const PropertySpec& rhs) noexcept
  {
    return lhs.name == rhs.name && lhs.description == rhs.description && lhs.category == rhs.category &&
           lhs.transportMode == rhs.transportMode && lhs.tags == rhs.tags && lhs.checkedSet == rhs.checkedSet &&
           lhs.type == rhs.type;
  }
  friend bool operator!=(const PropertySpec& lhs, const PropertySpec& rhs) noexcept { return !(lhs == rhs); }

public:
  std::string name;                                         // NOLINT(misc-non-private-member-variables-in-classes)
  std::string description;                                  // NOLINT(misc-non-private-member-variables-in-classes)
  PropertyCategory category = PropertyCategory::dynamicRO;  // NOLINT(misc-non-private-member-variables-in-classes)
  ConstTypeHandle<> type;                                   // NOLINT(misc-non-private-member-variables-in-classes)
  TransportMode transportMode = TransportMode::multicast;   // NOLINT(misc-non-private-member-variables-in-classes)
  std::vector<std::string> tags;                            // NOLINT(misc-non-private-member-variables-in-classes)
  bool checkedSet = false;                                  // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Represents a property.
class Property final
{
  SEN_NOCOPY_NOMOVE(Property)
  SEN_PRIVATE_TAG

public:
  /// Factory function that validates the spec and creates a property.
  /// Throws std::exception if the spec is not valid.
  [[nodiscard]] static std::shared_ptr<Property> make(PropertySpec spec);

public:  // special members
  Property(PropertySpec spec, Private notUsable);
  ~Property() = default;

public:
  /// The name of the property.
  [[nodiscard]] std::string_view getName() const noexcept;

  /// The documentation of the property.
  [[nodiscard]] std::string_view getDescription() const noexcept;

  /// The propety id.
  [[nodiscard]] MemberHash getId() const noexcept;

  /// Gets the property category.
  [[nodiscard]] PropertyCategory getCategory() const noexcept;

  /// Gets the return type of the method (nullptr means void).
  [[nodiscard]] ConstTypeHandle<> getType() const noexcept;

  /// Gets the transport mode for this property.
  [[nodiscard]] TransportMode getTransportMode() const noexcept;

  /// The getter method.
  [[nodiscard]] const Method& getGetterMethod() const noexcept;

  /// The setter method.
  [[nodiscard]] const Method& getSetterMethod() const noexcept;

  /// The change notification event.
  [[nodiscard]] const Event& getChangeEvent() const noexcept;

  /// Any user-defined tags
  [[nodiscard]] const std::vector<std::string>& getTags() const noexcept;

  /// If true, the user will validate sets to the property value.
  [[nodiscard]] bool getCheckedSet() const noexcept;

  /// Utility function that creates the name of a getter method.
  /// Throws std::exception if the spec has an empty name.
  [[nodiscard]] static std::string makeGetterMethodName(const PropertySpec& spec);

  /// Utility function that creates the name of a setter method.
  /// Throws std::exception if the spec has an empty name.
  [[nodiscard]] static std::string makeSetterMethodName(const PropertySpec& spec);

  /// Utility function that creates the name of a change notification event.
  /// Throws std::exception if the spec has an empty name.
  [[nodiscard]] static std::string makeChangeNotificationEventName(const PropertySpec& spec);

  /// Compares if it is equivalent to another property.
  [[nodiscard]] bool operator==(const Property& other) const noexcept;

  /// Compares if it is not equivalent to another property.
  [[nodiscard]] bool operator!=(const Property& other) const noexcept;

  /// Returns a unique hash for the Property computed at compile time
  [[nodiscard]] MemberHash getHash() const noexcept;

private:
  PropertySpec spec_;
  MemberHash id_;
  MemberHash hash_;
  std::shared_ptr<const Method> setterMethod_;
  std::shared_ptr<const Method> getterMethod_;
  std::shared_ptr<const Event> changeEvent_;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

/// Hash for the PropertySpec
template <>
struct impl::hash<PropertySpec>
{
  u32 operator()(const PropertySpec& spec) const noexcept
  {
    auto result = hashCombine(hashSeed, spec.name, spec.category, spec.transportMode, spec.type->getHash());

    for (const auto& tag: spec.tags)
    {
      result = hashCombine(result, tag);
    }
    return result;
  }
};

}  // namespace sen

#endif  // SEN_CORE_META_PROPERTY_H
