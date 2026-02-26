// === method.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_METHOD_H
#define SEN_CORE_META_METHOD_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/hash32.h"
#include "sen/core/meta/callable.h"
#include "sen/core/meta/type.h"

// std
#include <memory>
#include <utility>
#include <variant>

namespace sen
{

/// \addtogroup types
/// @{

// forward declarations
class Property;

/// Method constness.
enum class Constness
{
  constant,
  nonConstant,
};

/// Indicates that the method is not directly related to any property.
struct NonPropertyRelated
{
};

/// Indicates that the method represents a getter of a property.
struct PropertyGetter
{
  const Property* property;
};

/// Indicates that the method represents a setter of a property.
struct PropertySetter
{
  const Property* property;
};

/// Information about the relation between a method and a property.
using PropertyRelation = std::variant<NonPropertyRelated, PropertyGetter, PropertySetter>;

/// Data of a method.
struct MethodSpec final
{
  MethodSpec(CallableSpec callableSpec,
             ConstTypeHandle<> returnType,
             Constness constness = Constness::nonConstant,
             PropertyRelation propertyRelation = NonPropertyRelated {},
             bool deferred = false,
             bool localOnly = false)
    : callableSpec(std::move(callableSpec))
    , constness(constness)
    , returnType(std::move(returnType))
    , propertyRelation(propertyRelation)
    , deferred(deferred)
    , localOnly(localOnly)
  {
  }

  // Comparison operators
  friend bool operator==(const MethodSpec& lhs, const MethodSpec& rhs) noexcept
  {
    return lhs.callableSpec == rhs.callableSpec && lhs.constness == rhs.constness && lhs.deferred == rhs.deferred &&
           lhs.returnType == rhs.returnType;
  }

  friend bool operator!=(const MethodSpec& lhs, const MethodSpec& rhs) noexcept { return !(lhs == rhs); }

public:
  CallableSpec callableSpec;                                  // NOLINT(misc-non-private-member-variables-in-classes)
  Constness constness = Constness::nonConstant;               // NOLINT(misc-non-private-member-variables-in-classes)
  ConstTypeHandle<> returnType;                               // NOLINT(misc-non-private-member-variables-in-classes)
  PropertyRelation propertyRelation = NonPropertyRelated {};  // NOLINT(misc-non-private-member-variables-in-classes)
  bool deferred = false;                                      // NOLINT(misc-non-private-member-variables-in-classes)
  bool localOnly = false;                                     // NOLINT(misc-non-private-member-variables-in-classes)
};

/// Represents a method.
class Method final: public Callable
{
  SEN_NOCOPY_NOMOVE(Method)
  SEN_PRIVATE_TAG

public:
  /// Copies the spec into a member.
  /// Private to prevent direct instances.
  Method(const MethodSpec& spec, Private notUsable);

public:
  /// Factory function that validates the spec and creates a method.
  /// Throws std::exception if the spec is not valid.
  [[nodiscard]] static std::shared_ptr<Method> make(MethodSpec spec);

public:  // special members
  ~Method() override = default;

public:
  /// The constness of the method.
  [[nodiscard]] Constness getConstness() const noexcept;

  /// True if the method is marked as deferred.
  [[nodiscard]] bool getDeferred() const noexcept;

  /// Gets the return type of the method (nullptr means void).
  [[nodiscard]] ConstTypeHandle<> getReturnType() const noexcept;

  /// Information about the relation to a property.
  [[nodiscard]] const PropertyRelation& getPropertyRelation() const noexcept;

  /// True if the method is the method is only locally available (within the same component).
  [[nodiscard]] bool getLocalOnly() const noexcept;

  /// Compares if it is equivalent to another method.
  [[nodiscard]] bool operator==(const Method& other) const noexcept;

  /// Compares if it is not equivalent to another method.
  [[nodiscard]] bool operator!=(const Method& other) const noexcept;

  /// Returns a unique hash to identify the method
  [[nodiscard]] MemberHash getHash() const noexcept;

  /// Returns the hash of the method's unqualified name
  [[nodiscard]] MemberHash getId() const noexcept;

private:
  Constness constness_;
  bool deferred_;
  bool localOnly_;
  ConstTypeHandle<> returnType_;
  PropertyRelation propertyRelation_;
  MemberHash id_;
  MemberHash hash_;
};

/// @}

/// Hash for the MethodSpec
template <>
struct impl::hash<MethodSpec>
{
  u32 operator()(const MethodSpec& spec) const noexcept
  {
    return hashCombine(hashSeed, spec.callableSpec, spec.constness, spec.returnType->getHash());
  }
};

}  // namespace sen

#endif  // SEN_CORE_META_METHOD_H
