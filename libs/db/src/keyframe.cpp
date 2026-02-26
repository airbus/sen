// === keyframe.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/db/keyframe.h"

// sen
#include "sen/core/base/span.h"
#include "sen/db/snapshot.h"

// std
#include <utility>
#include <vector>

namespace sen::db
{

Keyframe::Keyframe(std::vector<Snapshot> snapshots): snapshots_(std::move(snapshots)) {}

Span<const Snapshot> Keyframe::getSnapshots() const noexcept { return snapshots_; }

}  // namespace sen::db
