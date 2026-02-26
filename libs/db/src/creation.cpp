// === creation.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/db/creation.h"

// sen
#include "sen/db/snapshot.h"

// std
#include <utility>

namespace sen::db
{

Creation::Creation(Snapshot snapshot): snapshot_(std::move(snapshot)) {}

const Snapshot& Creation::getSnapshot() const noexcept { return snapshot_; }

}  // namespace sen::db
