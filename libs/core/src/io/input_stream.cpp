// === input_stream.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/io/input_stream.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace sen
{

const uint8_t* InputStreamBase::advance(size_t bytes)
{
  const auto newCursor = cursor_ + bytes;

  if (newCursor > buffer_.size())
  {
    sen::throwRuntimeError("input buffer underflow");
    SEN_UNREACHABLE();
  }

  auto* ptr = buffer_.data() + cursor_;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  cursor_ = newCursor;
  return ptr;
}

std::pair<const uint8_t*, std::size_t> InputStreamBase::tryAdvance(std::size_t bytes)
{
  const auto newCursor = std::min(cursor_ + bytes, buffer_.size());
  const auto advancedBytes = newCursor - cursor_;

  if (bytes != 0U && advancedBytes == 0U)
  {
    sen::throwRuntimeError("input buffer underflow");
    SEN_UNREACHABLE();
  }

  auto* ptr = buffer_.data() + cursor_;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  cursor_ = newCursor;
  return {ptr, advancedBytes};
}

}  // namespace sen
