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

/// Compile-time description of a property, consumed by the code generator and the type registry.
struct PropertySpec final
{
  /// @param name          Identifier of the property (must be non-empty and a valid Sen name).
  /// @param description   Human-readable documentation string surfaced by introspection tools.
  /// @param type          Type handle describing the value type of the property.
  /// @param category      Visibility and mutability of the property (static/dynamic, RO/RW).
  /// @param transportMode QoS transport used when propagating property changes across components.
  /// @param checkedSet    If true, the owning object validates incoming sets via a generated `*AcceptsSet` method.
  /// @param tags          Optional user-defined labels for filtering or tooling purposes.
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
  std::string
    name;  ///< Identifier used to look up the property at runtime. NOLINT(misc-non-private-member-variables-in-classes)
  std::string description;  ///< Documentation string shown in shells and introspection tools.
                            ///< NOLINT(misc-non-private-member-variables-in-classes)
  PropertyCategory category = PropertyCategory::dynamicRO;  ///< Mutability and lifetime semantics of the value.
                                                            ///< NOLINT(misc-non-private-member-variables-in-classes)
  ConstTypeHandle<> type;  ///< Type of the property value. NOLINT(misc-non-private-member-variables-in-classes)
  TransportMode transportMode = TransportMode::multicast;  ///< Network QoS for inter-process propagation.
                                                           ///< NOLINT(misc-non-private-member-variables-in-classes)
  std::vector<std::string> tags;                           ///< Optional user-defined labels for tooling or filtering.
                                                           ///< NOLINT(misc-non-private-member-variables-in-classes)
  bool checkedSet = false;  ///< If true, sets are validated by the owning object before being applied.
                            ///< NOLINT(misc-non-private-member-variables-in-classes)
};

/// Represents a property.
class Property final
{
  SEN_NOCOPY_NOMOVE(Property)
  SEN_PRIVATE_TAG

public:
  /// Factory function that validates the spec and creates a Property.
  /// @param spec  Description of the property to create.
  /// @return Shared pointer to the new Property.
  /// @throws std::exception if @p spec has an empty name or an otherwise invalid configuration.
  [[nodiscard]] static std::shared_ptr<Property> make(PropertySpec spec);

public:  // special members
  Property(PropertySpec spec, Private notUsable);
  ~Property() = default;

public:
  /// @return The property's name (e.g. `"speed"`).
  [[nodiscard]] std::string_view getName() const noexcept;

  /// @return Human-readable description of this property's purpose.
  [[nodiscard]] std::string_view getDescription() const noexcept;

  /// @return 32-bit hash of the property name, used for fast lookup at runtime.
  [[nodiscard]] MemberHash getId() const noexcept;

  /// @return Mutability and lifetime semantics of this property's value.
  [[nodiscard]] PropertyCategory getCategory() const noexcept;

  /// @return Type handle describing the value type of this property.
  [[nodiscard]] ConstTypeHandle<> getType() const noexcept;

  /// @return Network QoS mode used when propagating this property's changes across components.
  [[nodiscard]] TransportMode getTransportMode() const noexcept;

  /// @return The generated getter `Method` (e.g. `"getSpeed"`).
  [[nodiscard]] const Method& getGetterMethod() const noexcept;

  /// @return The generated setter `Method` (e.g. `"setNextSpeed"`).
  [[nodiscard]] const Method& getSetterMethod() const noexcept;

  /// @return The generated change-notification `Event` (e.g. `"onSpeedChanged"`).
  [[nodiscard]] const Event& getChangeEvent() const noexcept;

  /// @return User-defined labels attached to this property for tooling or filtering.
  [[nodiscard]] const std::vector<std::string>& getTags() const noexcept;

  /// @return `true` if the owning object validates incoming set requests before applying them.
  [[nodiscard]] bool getCheckedSet() const noexcept;

  /// Derives the conventional getter method name from a property spec (e.g. `"speed"` â†’ `"getSpeed"`).
  /// @param spec Property spec whose name is used.
  /// @return Generated getter name string.
  /// @throws std::exception if @p spec has an empty name.
  [[nodiscard]] static std::string makeGetterMethodName(const PropertySpec& spec);

  /// Derives the conventional setter method name from a property spec (e.g. `"speed"` â†’ `"setNextSpeed"`).
  /// @param spec Property spec whose name is used.
  /// @return Generated setter name string.
  /// @throws std::exception if @p spec has an empty name.
  [[nodiscard]] static std::string makeSetterMethodName(const PropertySpec& spec);

  /// Derives the conventional change-notification event name from a property spec (e.g. `"speed"` â†’
  /// `"onSpeedChanged"`).
  /// @param spec Property spec whose name is used.
  /// @return Generated event name string.
  /// @throws std::exception if @p spec has an empty name.
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
