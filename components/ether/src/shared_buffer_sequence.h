// === shared_buffer_sequence.h ========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_SHARED_BUFFER_SEQUENCE_H
#define SEN_COMPONENTS_ETHER_SRC_SHARED_BUFFER_SEQUENCE_H

#include "sen/core/base/memory_block.h"
#include "sen/core/base/numbers.h"

// std
#include <memory>
#include <vector>

// asio
#include <asio/buffer.hpp>

namespace sen::components::ether
{

class SharedBufferSequence
{
  SEN_COPY_MOVE(SharedBufferSequence)

public:
  using BufferArray = std::array<asio::const_buffer, 3>;

public:
  SharedBufferSequence() noexcept = default;
  SharedBufferSequence(MemBlockPtr&& data1, MemBlockPtr&& data2) noexcept;
  SharedBufferSequence(MemBlockPtr&& data1, MemBlockPtr&& data2, MemBlockPtr&& data3) noexcept;
  ~SharedBufferSequence() = default;

public:
  using value_type = asio::const_buffer;                        // NOLINT(readability-identifier-naming)
  using const_iterator = typename BufferArray::const_iterator;  // NOLINT(readability-identifier-naming)

public:
  [[nodiscard]] const_iterator begin() const noexcept { return buffers_.begin(); }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  [[nodiscard]] const_iterator end() const noexcept { return buffers_.begin() + size_; }

private:
  std::array<MemBlockPtr, 3> data_;
  BufferArray buffers_;
  std::size_t size_ = 0;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline SharedBufferSequence::SharedBufferSequence(MemBlockPtr&& data1,
                                                  MemBlockPtr&& data2,
                                                  MemBlockPtr&& data3) noexcept
  : size_(3U)
{
  {
    auto span = data1->getConstSpan();
    buffers_[0] = asio::const_buffer(span.data(), span.size());
    data_[0] = std::move(data1);
  }

  {
    auto span = data2->getConstSpan();
    buffers_[1] = asio::const_buffer(span.data(), span.size());
    data_[1] = std::move(data2);
  }

  {
    auto span = data3->getConstSpan();
    buffers_[2] = asio::const_buffer(span.data(), span.size());
    data_[2] = std::move(data3);
  }
}

inline SharedBufferSequence::SharedBufferSequence(MemBlockPtr&& data1, MemBlockPtr&& data2) noexcept: size_(2U)
{
  {
    auto span = data1->getConstSpan();
    buffers_[0] = asio::const_buffer(span.data(), span.size());
    data_[0] = std::move(data1);
  }

  {
    auto span = data2->getConstSpan();
    buffers_[1] = asio::const_buffer(span.data(), span.size());
    data_[1] = std::move(data2);
  }
}

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_SHARED_BUFFER_SEQUENCE_H
