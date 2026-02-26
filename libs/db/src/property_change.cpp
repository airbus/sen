// === property_change.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/db/property_change.h"

// sen
#include "sen/core/base/span.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/object.h"

// std
#include <cstdint>
#include <utility>
#include <vector>

namespace sen::db
{

PropertyChange::PropertyChange(ObjectId objectId, const Property* property, std::vector<uint8_t> buffer)
  : objectId_(std::move(objectId)), property_(property), buffer_(std::move(buffer))
{
}

ObjectId PropertyChange::getObjectId() const noexcept { return objectId_; }

const Property* PropertyChange::getProperty() const noexcept { return property_; }

Span<const uint8_t> PropertyChange::getValueAsBuffer() const noexcept { return buffer_; }

const Var& PropertyChange::getValueAsVariant() const
{
  if (!variantExtracted_)
  {
    InputStream in(buffer_);
    ::sen::impl::readFromStream(variant_, in, *property_->getType());
    variantExtracted_ = true;
  }

  return variant_;
}

}  // namespace sen::db
