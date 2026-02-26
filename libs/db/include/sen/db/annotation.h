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

/// Represents an annotation which can be added to a recording. \ingroup db
class Annotation
{
  SEN_COPY_MOVE(Annotation)

public:
  ~Annotation() = default;

public:
  /// The data type of the annotation value.
  [[nodiscard]] const ConstTypeHandle<>& getType() const noexcept;

  /// The annotation content, as a buffer.
  [[nodiscard]] Span<const uint8_t> getValueAsBuffer() const noexcept;

  /// The annotation content, as a variant.
  /// Contents of the variant are obtained by interpreting the buffer as the type of the annotation.
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
