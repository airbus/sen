// === callable.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_CALLABLE_H
#define SEN_CORE_META_CALLABLE_H

// sen
#include "sen/core/base/class_helpers.h"  // for SEN_NOCOPY_NOMOVE
#include "sen/core/base/hash32.h"
#include "sen/core/base/span.h"  // for Span
#include "sen/core/meta/type.h"  // for MemberHash, TransportMode

// std
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sen
{

/// \addtogroup types
/// @{

/// Holds the information for an argument in the callable.
struct Arg final
{
  SEN_COPY_MOVE(Arg)

public:
  /// @param theName        Argument name (must be unique within the callable).
  /// @param theDescription Human-readable description of the argument's purpose.
  /// @param theType        Type handle describing the argument's value type.
  Arg(std::string theName, std::string theDescription, ConstTypeHandle<> theType)
    : name(std::move(theName)), description(std::move(theDescription)), type(std::move(theType))
  {
  }

  // Comparison operators
  friend bool operator==(const Arg& lhs, const Arg& rhs) noexcept
  {
    if (&lhs == &rhs)
    {
      return true;
    }

    return lhs.name == rhs.name && lhs.description == rhs.description && lhs.type == rhs.type;
  }

  friend bool operator!=(const Arg& lhs, const Arg& rhs) noexcept { return !(lhs == rhs); }

  ~Arg() = default;

public:
  std::string name;         ///< Argument name.  // NOLINT(misc-non-private-member-variables-in-classes)
  std::string description;  ///< Human-readable description.  // NOLINT(misc-non-private-member-variables-in-classes)
  ConstTypeHandle<> type;   ///< Type of the argument value.  // NOLINT(misc-non-private-member-variables-in-classes)

public:
  /// Returns the 32-bit FNV hash of the argument name, computed lazily and cached.
  /// @return Hash of `name`.
  [[nodiscard]] uint32_t getNameHash() const noexcept;

private:
  mutable uint32_t nameHash_ = 0;
};

/// Descriptor used to construct a `Callable` instance.
struct CallableSpec final
{

  friend bool operator==(const CallableSpec& lhs, const CallableSpec& rhs) noexcept
  {
    if (&lhs == &rhs)
    {
      return true;
    }

    return lhs.transportMode == rhs.transportMode && lhs.name == rhs.name && lhs.description == rhs.description &&
           lhs.args == rhs.args;
  }

  friend bool operator!=(const CallableSpec& lhs, const CallableSpec& rhs) noexcept { return !(lhs == rhs); }

  std::string name;                                        ///< Callable name (must be a valid lower-case identifier).
  std::string description;                                 ///< Human-readable description.
  std::vector<Arg> args;                                   ///< Ordered list of callable arguments.
  TransportMode transportMode = TransportMode::confirmed;  ///< Delivery guarantee for remote invocations.
};

/// Represents something that can be called.
class Callable
{
  SEN_NOCOPY_NOMOVE(Callable)

public:  // special members
  /// Copies the spec into a member.
  explicit Callable(CallableSpec spec);
  virtual ~Callable() = default;

public:
  /// @return The callable's name.
  [[nodiscard]] std::string_view getName() const noexcept;

  /// @return Human-readable description of the callable.
  [[nodiscard]] std::string_view getDescription() const noexcept;

  /// @return Delivery guarantee used when invoking this callable remotely.
  [[nodiscard]] TransportMode getTransportMode() const noexcept;

  /// @return Read-only span over the ordered argument list.
  [[nodiscard]] Span<const Arg> getArgs() const noexcept;

  /// Looks up an argument by name.
  /// @param name The argument name to search for.
  /// @return Pointer to the matching `Arg`, or `nullptr` if not found.
  [[nodiscard]] const Arg* getArgFromName(std::string_view name) const;

  /// Looks up the position of an argument by its name hash.
  /// @param nameHash 32-bit hash of the argument name.
  /// @return Zero-based index into the args vector.
  [[nodiscard]] size_t getArgIndexFromNameHash(MemberHash nameHash) const;

public:  // comparison
  [[nodiscard]] bool operator==(const Callable& other) const noexcept;

  [[nodiscard]] bool operator!=(const Callable& other) const noexcept;

public:
  /// Throws std::exception if the callable spec is invalid
  static void checkSpec(const CallableSpec& spec);

protected:
  /// The spec of this callable
  [[nodiscard]] const CallableSpec& getCallableSpec() const noexcept;

private:
  CallableSpec spec_;
  std::unordered_map<MemberHash, size_t> nameHashToArgIndex_;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline uint32_t Arg::getNameHash() const noexcept
{
  if (nameHash_ == 0)
  {
    nameHash_ = hashCombine(hashSeed, name);
  }
  return nameHash_;
}

template <>
struct impl::hash<Arg>
{
  inline u32 operator()(const Arg& spec) const noexcept
  {
    return hashCombine(hashSeed, spec.name, spec.type->getHash());
  }
};

template <>
struct impl::hash<CallableSpec>
{
  u32 operator()(const CallableSpec& spec) const noexcept
  {
    auto result = hashCombine(hashSeed, spec.name, spec.transportMode);

    for (const auto& arg: spec.args)
    {
      result = hashCombine(result, arg);
    }

    return result;
  }
};

}  // namespace sen

#endif  // SEN_CORE_META_CALLABLE_H
