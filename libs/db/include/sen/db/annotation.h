// === annotation.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_DB_ANNOTATION_H
#define SEN_DB_ANNOTATION_H

// sen
#include "sen/core/base/span.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"

// std
#include <vector>

namespace sen::db
{

class Input;

/// Recording entry that carries a user-defined typed annotation attached to a point in time.
/// Annotations are written via `Output::annotation()` and iterated with an `AnnotationCursor`.
/// The value is stored in the Sen wire format and lazily deserialised to `Var` on first access.
/// \ingroup db
class Annotation
{
  SEN_COPY_MOVE(Annotation)

public:
  ~Annotation() = default;

public:
  /// Returns the compile-time type descriptor for this annotation's value.
  /// @return Non-owning handle to the type; valid for the lifetime of the type registry.
  [[nodiscard]] const ConstTypeHandle<>& getType() const noexcept;

  /// Returns the annotation value as a raw serialised byte span.
  /// Useful for zero-copy forwarding without deserialising to `Var`.
  /// @return Non-owning span into the internal buffer; valid for the lifetime of this Annotation.
  [[nodiscard]] Span<const uint8_t> getValueAsBuffer() const noexcept;

  /// Returns the annotation value as a type-erased variant.
  /// The value is lazily deserialised from the internal buffer and cached on first call.
  /// @return Reference to the cached `Var`; valid for the lifetime of this Annotation.
  [[nodiscard]] const Var& getValueAsVariant() const;

private:
  friend class Input;
  Annotation(ConstTypeHandle<> type, std::vector<uint8_t> buffer);

private:
  ConstTypeHandle<> type_;
  mutable bool variantExtracted_ = false;
  mutable Var variant_;
  std::vector<uint8_t> buffer_;
};

}  // namespace sen::db

#endif  // SEN_DB_ANNOTATION_H
