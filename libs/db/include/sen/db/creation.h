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

/// Represents the creation of an object. \ingroup db
class Creation
{
  SEN_COPY_MOVE(Creation)

public:
  ~Creation() = default;

public:
  /// The object snapshot when it was created.
  [[nodiscard]] const Snapshot& getSnapshot() const noexcept;

private:
  friend class Input;
  explicit Creation(Snapshot snapshot);

private:
  Snapshot snapshot_;
};

}  // namespace sen::db

#endif  // SEN_DB_CREATION_H
