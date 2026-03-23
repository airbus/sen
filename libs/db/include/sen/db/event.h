// === event.h =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_DB_EVENT_H
#define SEN_DB_EVENT_H

#include "sen/core/meta/event.h"
#include "sen/core/obj/object.h"

namespace sen::db
{

class Input;

/// Recording entry produced when an object emits a Sen event.
/// Arguments are stored in the Sen wire format and lazily deserialised to `Var` on first access.
/// Returned by a `Cursor` when iterating over a recording that includes event emissions.
/// \ingroup db
class Event
{
  SEN_COPY_MOVE(Event)

public:
  ~Event() = default;

public:
  /// @return ID of the object that emitted this event.
  [[nodiscard]] ObjectId getObjectId() const noexcept;

  /// @return Pointer to the compile-time event descriptor (name, argument types, QoS).
  ///         Valid for the lifetime of the type registry; never null for a well-formed recording.
  [[nodiscard]] const ::sen::Event* getEvent() const noexcept;

  /// Returns the event arguments as a list of type-erased variants.
  /// Arguments are lazily deserialised and cached on first call.
  /// @return Reference to the cached argument list; valid for the lifetime of this Event.
  [[nodiscard]] const std::vector<Var>& getArgsAsVariants() const;

  /// Returns the event arguments as a raw serialised byte span.
  /// Useful for zero-copy forwarding without deserialising to `Var`.
  /// @return Non-owning span into the internal buffer; valid for the lifetime of this Event.
  [[nodiscard]] Span<const uint8_t> getArgsAsBuffer() const noexcept;

private:
  friend class Input;
  Event(ObjectId objectId, const ::sen::Event* event, std::vector<uint8_t> buffer);

private:
  ObjectId objectId_;
  const ::sen::Event* event_;
  mutable bool argsExtracted_ = false;
  mutable std::vector<Var> args_;
  std::vector<uint8_t> buffer_;
};

}  // namespace sen::db

#endif  // SEN_DB_EVENT_H
