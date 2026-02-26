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
  std::string name;         // NOLINT(misc-non-private-member-variables-in-classes)
  std::string description;  // NOLINT(misc-non-private-member-variables-in-classes)
  ConstTypeHandle<> type;   // NOLINT(misc-non-private-member-variables-in-classes)

public:
  [[nodiscard]] uint32_t getNameHash() const noexcept;

private:
  mutable uint32_t nameHash_ = 0;
};

/// Data of a callable.
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

  std::string name;
  std::string description;
  std::vector<Arg> args;
  TransportMode transportMode = TransportMode::confirmed;
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
  /// Callable name
  [[nodiscard]] std::string_view getName() const noexcept;

  /// Callable description
  [[nodiscard]] std::string_view getDescription() const noexcept;

  /// The transport mode of this callable
  [[nodiscard]] TransportMode getTransportMode() const noexcept;

  /// Gets the arguments.
  [[nodiscard]] Span<const Arg> getArgs() const noexcept;

  /// Get the field data given a name.
  /// Nullptr means not found.
  /// @param name the name of the field.
  [[nodiscard]] const Arg* getArgFromName(std::string_view name) const;

  /// Get the index in the vector of Args given a name hash
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
