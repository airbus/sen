// === creation.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_DB_CREATION_H
#define SEN_DB_CREATION_H

#include "sen/db/snapshot.h"

namespace sen::db
{

class Input;

/// Recording entry produced when a new object is published to a Sen bus.
/// Contains the full initial property state of the object as a `Snapshot`.
/// Returned by a `Cursor` when iterating over a recording that includes object-creation events.
/// \ingroup db
class Creation
{
  SEN_COPY_MOVE(Creation)

public:
  ~Creation() = default;

public:
  /// @return Immutable snapshot of the object's full property state at the moment of creation.
  [[nodiscard]] const Snapshot& getSnapshot() const noexcept;

private:
  friend class Input;
  explicit Creation(Snapshot snapshot);

private:
  Snapshot snapshot_;
};

}  // namespace sen::db

#endif  // SEN_DB_CREATION_H
