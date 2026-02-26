// === keyframe.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_DB_KEYFRAME_H
#define SEN_DB_KEYFRAME_H

#include "sen/core/obj/object.h"
#include "sen/db/snapshot.h"

namespace sen::db
{

class Input;

/// Represents the state of the system at a given time.
/// The state of the system is made of the snapshots of all the
/// objects present at the time of the snapshot. \ingroup db
class Keyframe
{
  SEN_COPY_MOVE(Keyframe)

public:
  ~Keyframe() = default;

public:
  /// The set of snapshots for all the objects present at the time.
  [[nodiscard]] Span<const Snapshot> getSnapshots() const noexcept;

private:
  friend class Input;
  explicit Keyframe(std::vector<Snapshot> snapshots);

private:
  std::vector<Snapshot> snapshots_;
};

}  // namespace sen::db

#endif  // SEN_DB_KEYFRAME_H
