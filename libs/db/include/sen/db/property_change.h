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

/// Recording entry produced when a property value changes on a tracked object.
/// The new value is stored in the Sen wire format and lazily deserialised to `Var` on first access.
/// Returned by a `DataCursor` when iterating over a recording that includes property updates.
/// \ingroup db
class PropertyChange
{
  SEN_COPY_MOVE(PropertyChange)

public:
  ~PropertyChange() = default;

public:
  /// Returns the ID of the object whose property changed.
  /// @return The `ObjectId` of the source object.
  [[nodiscard]] ObjectId getObjectId() const noexcept;

  /// Returns the compile-time property descriptor (name, type, getter/setter info).
  /// @return Non-owning pointer to the `Property` meta object; valid for the lifetime of the type registry.
  [[nodiscard]] const Property* getProperty() const noexcept;

  /// Returns the new property value as a raw serialised byte span.
  /// Useful for zero-copy forwarding without deserialising to `Var`.
  /// @return Non-owning span into the internal buffer; valid for the lifetime of this PropertyChange.
  [[nodiscard]] Span<const uint8_t> getValueAsBuffer() const noexcept;

  /// Returns the new property value as a type-erased variant.
  /// The value is lazily deserialised from the internal buffer and cached on first call.
  /// @return Reference to the cached `Var`; valid for the lifetime of this PropertyChange.
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
