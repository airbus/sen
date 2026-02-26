// === buffer_writer.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_IO_BUFFER_WRITER_H
#define SEN_CORE_IO_BUFFER_WRITER_H

#include "sen/core/base/span.h"

#include <vector>

namespace sen
{

/// Interface for writing to a device. \ingroup io
class Writer
{
  SEN_NOCOPY_NOMOVE(Writer)

public:
  Writer() noexcept = default;
  virtual ~Writer() noexcept = default;

public:
  /// Returns a buffer of 'size' bytes, owned by the writer.
  virtual uint8_t* advance(size_t size) = 0;

  /// Moves the cursor 'size' bytes.
  virtual void reverse(size_t size) = 0;
};

/// A writer that owns a fixed size buffer. \ingroup io
class BufferWriter final: public Writer
{
  SEN_NOCOPY_NOMOVE(BufferWriter)

public:
  explicit BufferWriter(Span<uint8_t> buffer) noexcept: cursor_(buffer.data()) {}
  ~BufferWriter() override = default;

public:  // implements sen::Writer
  [[nodiscard]] uint8_t* advance(size_t size) override;
  void reverse(size_t size) override
  {
    cursor_ -= size;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

private:
  uint8_t* cursor_;
};

/// A writer that owns a buffer that gets resized on demand. \ingroup io
template <typename Container = std::vector<uint8_t>>
class ResizableBufferWriter final: public Writer
{
  SEN_NOCOPY_NOMOVE(ResizableBufferWriter)

public:
  explicit ResizableBufferWriter(Container& buffer) noexcept: buffer_(buffer) {}
  ~ResizableBufferWriter() override = default;

public:  // implements sen::Writer
  [[nodiscard]] uint8_t* advance(size_t size) override;
  void reverse(size_t size) override { buffer_.resize(buffer_.size() - size); }

private:
  Container& buffer_;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// BufferWriter
//--------------------------------------------------------------------------------------------------------------

inline uint8_t* BufferWriter::advance(size_t size)
{
  auto* oldCursor = cursor_;
  cursor_ += size;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return oldCursor;
}

//--------------------------------------------------------------------------------------------------------------
// ResizableBufferWriter
//--------------------------------------------------------------------------------------------------------------

template <typename Container>
inline uint8_t* ResizableBufferWriter<Container>::advance(size_t size)
{
  const auto cursor = buffer_.size();
  buffer_.resize(buffer_.size() + size);
  return buffer_.data() + cursor;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
}

}  // namespace sen

#endif  // SEN_CORE_IO_BUFFER_WRITER_H
