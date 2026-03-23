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

/// Abstract write cursor used by `OutputStreamTemplate` to append serialised bytes.
/// Subclasses provide different backing storage strategies (fixed buffer or resizable vector).
/// \ingroup io
class Writer
{
  SEN_NOCOPY_NOMOVE(Writer)

public:
  Writer() noexcept = default;
  virtual ~Writer() noexcept = default;

public:
  /// Advances the write cursor by `size` bytes and returns a pointer to the start of the reserved region.
  /// The caller must write exactly `size` bytes into the returned pointer before the next call.
  /// @param size Number of bytes to reserve.
  /// @return Pointer to the beginning of the reserved region.
  virtual uint8_t* advance(size_t size) = 0;

  /// Steps the write cursor backwards by `size` bytes, effectively un-writing the last `size` bytes.
  /// @param size Number of bytes to retract.
  virtual void reverse(size_t size) = 0;
};

/// `Writer` backed by a caller-owned, fixed-size byte buffer.
/// Writing beyond the buffer end is undefined behaviour; the caller must ensure sufficient capacity.
/// \ingroup io
class BufferWriter final: public Writer
{
  SEN_NOCOPY_NOMOVE(BufferWriter)

public:
  /// Constructs the writer over an externally owned buffer.
  /// @param buffer Span over the fixed-size byte buffer to write into; must outlive this writer.
  explicit BufferWriter(Span<uint8_t> buffer) noexcept: cursor_(buffer.data()) {}
  ~BufferWriter() override = default;

public:  // implements sen::Writer
  /// @copydoc Writer::advance
  [[nodiscard]] uint8_t* advance(size_t size) override;
  /// @copydoc Writer::reverse
  void reverse(size_t size) override
  {
    cursor_ -= size;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  }

private:
  uint8_t* cursor_;
};

/// `Writer` backed by a resizable container (default: `std::vector<uint8_t>`).
/// The container grows automatically on each `advance()` call, making this suitable
/// for serialising values of unknown size.
/// \ingroup io
/// @tparam Container Byte-sequence container type; must support `resize()`, `size()`, and `data()`.
template <typename Container = std::vector<uint8_t>>
class ResizableBufferWriter final: public Writer
{
  SEN_NOCOPY_NOMOVE(ResizableBufferWriter)

public:
  /// Constructs the writer over an externally owned resizable container.
  /// @param buffer Container to append bytes into; must outlive this writer.
  explicit ResizableBufferWriter(Container& buffer) noexcept: buffer_(buffer) {}
  ~ResizableBufferWriter() override = default;

public:  // implements sen::Writer
  /// @copydoc Writer::advance
  [[nodiscard]] uint8_t* advance(size_t size) override;
  /// @copydoc Writer::reverse
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
