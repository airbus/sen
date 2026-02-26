// === annotation.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/db/annotation.h"

// sen
#include "sen/core/base/span.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"

// std
#include <cstdint>
#include <utility>
#include <vector>

namespace sen::db
{

Annotation::Annotation(ConstTypeHandle<> type, std::vector<uint8_t> buffer)
  : type_(std::move(type)), buffer_(std::move(buffer))
{
}

const ConstTypeHandle<>& Annotation::getType() const noexcept { return type_; }

Span<const uint8_t> Annotation::getValueAsBuffer() const noexcept { return buffer_; }

const Var& Annotation::getValueAsVariant() const
{
  if (!variantExtracted_)
  {
    InputStream in(buffer_);
    ::sen::impl::readFromStream(variant_, in, *type_);
    variantExtracted_ = true;
  }

  return variant_;
}

}  // namespace sen::db
