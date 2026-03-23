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

/// Recording entry that captures the complete property state of every live object
/// at a specific point in time (a "save point" within the recording).
/// Allows `Input::at()` to seek directly to a keyframe without replaying from the beginning.
/// \ingroup db
class Keyframe
{
  SEN_COPY_MOVE(Keyframe)

public:
  ~Keyframe() = default;

public:
  /// Returns a span over the property snapshots for all objects alive at keyframe time.
  /// @return Non-owning span of `Snapshot` objects, one per live object; valid for the
  ///         lifetime of this Keyframe.
  [[nodiscard]] Span<const Snapshot> getSnapshots() const noexcept;

private:
  friend class Input;
  explicit Keyframe(std::vector<Snapshot> snapshots);

private:
  std::vector<Snapshot> snapshots_;
};

}  // namespace sen::db

#endif  // SEN_DB_KEYFRAME_H
