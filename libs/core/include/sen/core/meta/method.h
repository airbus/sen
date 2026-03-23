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

/// Whether a method modifies the state of the object.
enum class Constness
{
  constant,     ///< The method does not modify the object's state (equivalent to a C++ `const` member function).
  nonConstant,  ///< The method may modify the object's state.
};

/// Indicates that the method is not directly related to any property.
struct NonPropertyRelated
{
};

/// Tag indicating that this method is the generated getter for a property.
struct PropertyGetter
{
  const Property* property;  ///< The property this getter reads; never null.
};

/// Tag indicating that this method is the generated setter for a property.
struct PropertySetter
{
  const Property* property;  ///< The property this setter writes; never null.
};

/// Information about the relation between a method and a property.
using PropertyRelation = std::variant<NonPropertyRelated, PropertyGetter, PropertySetter>;

/// Compile-time description of a method, consumed by the code generator and the type registry.
struct MethodSpec final
{
  /// @param callableSpec  Name, documentation, and parameter list of the method.
  /// @param returnType    Type handle for the return value; use the void type handle for void methods.
  /// @param constness     Whether the method is `const` (does not mutate object state).
  /// @param propertyRelation  Association to a property getter, setter, or neither.
  /// @param deferred      If true, the implementation receives a `std::promise` instead of returning directly.
  /// @param localOnly     If true, the method is only callable within the same component (not over the network).
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
  CallableSpec callableSpec;  ///< Name, doc, and parameters. NOLINT(misc-non-private-member-variables-in-classes)
  Constness constness =
    Constness::nonConstant;      ///< Whether the method is const. NOLINT(misc-non-private-member-variables-in-classes)
  ConstTypeHandle<> returnType;  ///< Return type; void type handle for void methods.
                                 ///< NOLINT(misc-non-private-member-variables-in-classes)
  PropertyRelation propertyRelation =
    NonPropertyRelated {};  ///< Association to a property, if any. NOLINT(misc-non-private-member-variables-in-classes)
  bool deferred = false;    ///< If true, the implementation returns via std::promise.
                            ///< NOLINT(misc-non-private-member-variables-in-classes)
  bool localOnly =
    false;  ///< If true, not callable across process boundaries. NOLINT(misc-non-private-member-variables-in-classes)
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
  /// Factory function that validates the spec and creates a `Method` instance.
  /// @param spec Descriptor containing the callable spec, return type, and property relation.
  /// @return Shared pointer to the newly created `Method`.
  /// @throws std::exception if the spec is not valid.
  [[nodiscard]] static std::shared_ptr<Method> make(MethodSpec spec);

public:  // special members
  ~Method() override = default;

public:
  /// @return Whether the method modifies the object's state (`constant` or `nonConstant`).
  [[nodiscard]] Constness getConstness() const noexcept;

  /// @return `true` if the implementation delivers its result via `std::promise` (async response).
  [[nodiscard]] bool getDeferred() const noexcept;

  /// @return Type handle for the return value; holds the void type handle for void methods.
  [[nodiscard]] ConstTypeHandle<> getReturnType() const noexcept;

  /// @return Variant describing whether this method is a property getter, setter, or unrelated.
  [[nodiscard]] const PropertyRelation& getPropertyRelation() const noexcept;

  /// @return True if the method is only available within the same component (not callable over the network).
  [[nodiscard]] bool getLocalOnly() const noexcept;

  /// Compares if it is equivalent to another method.
  [[nodiscard]] bool operator==(const Method& other) const noexcept;

  /// Compares if it is not equivalent to another method.
  [[nodiscard]] bool operator!=(const Method& other) const noexcept;

  /// @return 32-bit hash derived from the method's name, return type, and argument types.
  [[nodiscard]] MemberHash getHash() const noexcept;

  /// @return 32-bit hash of the method's unqualified name (used for fast lookup).
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
