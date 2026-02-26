// === deletion.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/db/deletion.h"

// sen
#include "sen/core/obj/object.h"

// std
#include <utility>

namespace sen::db
{

Deletion::Deletion(ObjectId objectId): objectId_(std::move(objectId)) {}

ObjectId Deletion::getObjectId() const noexcept { return objectId_; }

}  // namespace sen::db
