// === property_change.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_DB_PROPERTY_CHANGE_H
#define SEN_DB_PROPERTY_CHANGE_H

#include "sen/core/base/class_helpers.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/property.h"
#include "sen/core/obj/object.h"

namespace sen::db
{

class Input;

/// Represents the change of a property value.
/// \ingroup db
class PropertyChange
{
  SEN_COPY_MOVE(PropertyChange)

public:
  ~PropertyChange() = default;

public:
  /// The ID of the object that holds the property.
  [[nodiscard]] ObjectId getObjectId() const noexcept;

  /// The meta information of the property.
  [[nodiscard]] const Property* getProperty() const noexcept;

  /// The property value as a serialized buffer.
  [[nodiscard]] Span<const uint8_t> getValueAsBuffer() const noexcept;

  /// The property value as a variant.
  [[nodiscard]] const Var& getValueAsVariant() const;

private:
  friend class Input;
  PropertyChange(ObjectId objectId, const Property* property, std::vector<uint8_t> buffer);

private:
  ObjectId objectId_;
  const Property* property_;
  mutable bool variantExtracted_ = false;
  mutable Var variant_;
  std::vector<uint8_t> buffer_;
};

}  // namespace sen::db

#endif  // SEN_DB_PROPERTY_CHANGE_H
