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

/// Represents the emission of an event by some object.
/// \ingroup db
class Event
{
  SEN_COPY_MOVE(Event)

public:
  ~Event() = default;

public:
  /// The ID of the object that emitted the event.
  [[nodiscard]] ObjectId getObjectId() const noexcept;

  /// The meta information about the event.
  [[nodiscard]] const ::sen::Event* getEvent() const noexcept;

  /// The list of arguments, as variants.
  [[nodiscard]] const std::vector<Var>& getArgsAsVariants() const;

  /// The list of arguments, as a (serialized) binary buffer.
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
