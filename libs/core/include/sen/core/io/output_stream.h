// === output_stream.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_IO_OUTPUT_STREAM_H
#define SEN_CORE_IO_OUTPUT_STREAM_H

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/detail/serialization_traits.h"

// std
#include <cstring>

namespace sen
{

/// Binary serialisation stream that writes values into a `Writer`, parameterised by byte order.
/// All `write*()` methods append bytes to the underlying writer in declaration order.
/// @tparam BufferEndian Byte-order tag (`LittleEndian` or `BigEndian`).
/// \ingroup io
template <typename BufferEndian>
class OutputStreamTemplate final
{
  SEN_NOCOPY_NOMOVE(OutputStreamTemplate)

public:
  /// Constructs the stream over an existing `Writer`.
  /// @param writer Byte sink to write into; must outlive this stream.
  explicit OutputStreamTemplate(Writer& writer) noexcept: writer_(writer) {}
  ~OutputStreamTemplate() noexcept = default;

public:
  /// @name Primitive write methods
  /// Each method serialises the value and appends it to the underlying writer.
  /// @{
  void writeBool(bool val) { writeBasic(impl::BoolTransportType {val ? uchar_t {1U} : uchar_t {0U}}); }
  void writeChar(char val) { writeBasic(val); }
  void writeUChar(unsigned char val) { writeBasic(val); }
  void writeInt8(int8_t val) { writeBasic(val); }
  void writeUInt8(uint8_t val) { writeBasic(val); }
  void writeInt16(int16_t val) { writeBasic(val); }
  void writeUInt16(uint16_t val) { writeBasic(val); }
  void writeInt32(int32_t val) { writeBasic(val); }
  void writeUInt32(uint32_t val) { writeBasic(val); }
  void writeInt64(int64_t val) { writeBasic(val); }
  void writeUInt64(uint64_t val) { writeBasic(val); }
  void writeFloat32(float32_t val) { writeBasic(val); }
  void writeFloat64(float64_t val) { writeBasic(val); }
  /// Serialises a string as a 4-byte `uint32` length prefix followed by the UTF-8 bytes.
  void writeString(std::string_view val);
  /// Serialises a `TimeStamp` as a 64-bit nanosecond count since the epoch.
  void writeTimestamp(TimeStamp val) { writeInt64(val.sinceEpoch().getNanoseconds()); }
  /// @}

public:
  /// Returns the underlying `Writer` (mutable).
  /// @return Reference to the writer passed at construction.
  [[nodiscard]] Writer& getWriter() noexcept { return writer_; }

  /// Returns the underlying `Writer` (const).
  /// @return Const reference to the writer passed at construction.
  [[nodiscard]] const Writer& getWriter() const noexcept { return writer_; }

private:
  template <typename T>
  impl::IfBasic<T, void> writeBasic(T val);

private:
  Writer& writer_;
};

using OutputStream = OutputStreamTemplate<LittleEndian>;

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <typename BufferEndian>
inline void OutputStreamTemplate<BufferEndian>::writeString(std::string_view val)
{
  // write the size of the sequence
  auto size = static_cast<uint32_t>(val.size());
  writeUInt32(size);

  if (size == 0U)
  {
    return;
  }

  std::memcpy(writer_.advance(size), val.data(), size);
}

template <typename BufferEndian>
template <typename T>
inline impl::IfBasic<T, void> OutputStreamTemplate<BufferEndian>::writeBasic(T val)
{
  static_assert(std::is_trivially_copyable_v<T>);
  constexpr auto size = sizeof(T);

  ::sen::impl::swapBytesIfNeeded(val, BufferEndian {});
  std::memcpy(writer_.advance(size), &val, size);
}

}  // namespace sen

#endif  // SEN_CORE_IO_OUTPUT_STREAM_H
