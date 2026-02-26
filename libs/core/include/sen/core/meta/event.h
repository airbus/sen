// === event.h =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_META_EVENT_H
#define SEN_CORE_META_EVENT_H

#include "sen/core/base/class_helpers.h"  // for SEN_NOCOPY_NOMOVE
#include "sen/core/meta/callable.h"       // for Callable, CallableSpec

#include <memory>  // for shared_ptr

namespace sen
{

/// \addtogroup types
/// @{

/// Data of an event.
struct EventSpec final
{
  CallableSpec callableSpec;
};

// Comparison operators
bool operator==(const EventSpec& lhs, const EventSpec& rhs) noexcept;
bool operator!=(const EventSpec& lhs, const EventSpec& rhs) noexcept;

/// Represents an event.
class Event final: public Callable
{
  SEN_NOCOPY_NOMOVE(Event)

public:
  /// Factory function that validates the spec and creates a class type.
  /// Throws std::exception if the spec is not valid.
  [[nodiscard]] static std::shared_ptr<Event> make(const EventSpec& spec);

public:  // special members
  ~Event() override = default;

  /// Checks if other is equivalent to this.
  [[nodiscard]] bool operator==(const Event& other) const noexcept;

  /// Checks if other is not equivalent to this.
  [[nodiscard]] bool operator!=(const Event& other) const noexcept;

  /// Returns a unique hash to identify the Event
  [[nodiscard]] MemberHash getHash() const noexcept;

  /// Returns a name hash of the event
  [[nodiscard]] MemberHash getId() const noexcept;

private:
  /// Copies the spec into a member.
  explicit Event(const EventSpec& spec);

private:
  MemberHash hash_;
  MemberHash id_;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

/// Hash for the EventSpec
template <>
struct impl::hash<EventSpec>
{
  u32 operator()(const EventSpec& spec) const noexcept { return hashCombine(hashSeed, spec.callableSpec); }
};
}  // namespace sen

#endif  // SEN_CORE_META_EVENT_H
