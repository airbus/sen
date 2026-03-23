// === deletion.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_DB_DELETION_H
#define SEN_DB_DELETION_H

#include "sen/core/obj/object.h"

namespace sen::db
{

class Input;

/// Recording entry produced when an object is un-published from a Sen bus.
/// Returned by a `DataCursor` when iterating over a recording that includes object-deletion events.
/// \ingroup db
class Deletion
{
  SEN_COPY_MOVE(Deletion)

public:
  ~Deletion() = default;

public:
  /// Returns the ID of the object that was deleted.
  /// @return The `ObjectId` of the un-published object.
  [[nodiscard]] ObjectId getObjectId() const noexcept;

private:
  friend class Input;
  explicit Deletion(ObjectId objectId);

private:
  ObjectId objectId_;
};

}  // namespace sen::db

#endif  // SEN_DB_DELETION_H
