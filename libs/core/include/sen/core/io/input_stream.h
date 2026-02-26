// === input_stream.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_IO_INPUT_STREAM_H
#define SEN_CORE_IO_INPUT_STREAM_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/detail/serialization_traits.h"

// std
#include <cstring>

namespace sen
{

/// \addtogroup io
/// @{

/// Base class for input streams.
class InputStreamBase
{
  SEN_COPY_MOVE(InputStreamBase)

public:
  explicit InputStreamBase(Span<const uint8_t> buffer) noexcept: buffer_(std::move(buffer)) {}
  ~InputStreamBase() = default;

public:
  /// Skips a number of bytes.
  [[nodiscard]] const uint8_t* advance(std::size_t bytes);
  [[nodiscard]] std::pair<const uint8_t*, std::size_t> tryAdvance(std::size_t bytes);
  [[nodiscard]] bool atEnd() const noexcept { return cursor_ == buffer_.size(); }
  [[nodiscard]] std::size_t getPosition() const noexcept { return cursor_; }
  void setPosition(std::size_t pos) noexcept { cursor_ = pos; }

protected:
  void reverse(std::size_t bytes) noexcept { cursor_ -= bytes; }

private:
  Span<const uint8_t> buffer_;
  std::size_t cursor_ = 0U;
};

/// Binary input stream.
/// Deserializes values.
/// In general, I/O operations throw on failure.
template <typename BufferEndian>
class InputStreamTemplate final: public InputStreamBase
{
  SEN_COPY_MOVE(InputStreamTemplate)

public:
  using InputStreamBase::InputStreamBase;
  ~InputStreamTemplate() noexcept = default;

public:
  void readBool(bool& val);
  void readInt8(int8_t& val) { readBasic(val); }
  void readUInt8(uint8_t& val) { readBasic(val); }
  void readInt16(int16_t& val) { readBasic(val); }
  void readUInt16(uint16_t& val) { readBasic(val); }
  void readInt32(int32_t& val) { readBasic(val); }
  void readUInt32(uint32_t& val) { readBasic(val); }
  void readInt64(int64_t& val) { readBasic(val); }
  void readUInt64(uint64_t& val) { readBasic(val); }
  void readFloat32(float32_t& val);
  void readFloat64(float64_t& val);
  void readString(std::string& val);
  void readTimeStamp(TimeStamp& val);

private:
  template <typename T>
  impl::IfBasic<T, void> readBasic(T& val);
};

using InputStream = InputStreamTemplate<LittleEndian>;

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <typename BufferEndian>
inline void InputStreamTemplate<BufferEndian>::readBool(bool& val)
{
  impl::BoolTransportType tmp;
  readBasic(tmp);
  val = (tmp != 0U);
}

template <typename BufferEndian>
inline void InputStreamTemplate<BufferEndian>::readFloat32(float32_t& val)
{
  constexpr std::size_t size = sizeof(val);
  std::memcpy(&val, advance(size), size);
}

template <typename BufferEndian>
inline void InputStreamTemplate<BufferEndian>::readFloat64(float64_t& val)
{
  constexpr std::size_t size = sizeof(val);
  std::memcpy(&val, advance(size), size);
}

template <typename BufferEndian>
inline void InputStreamTemplate<BufferEndian>::readTimeStamp(TimeStamp& val)
{
  int64_t nanoseconds;
  readInt64(nanoseconds);
  val = TimeStamp(nanoseconds);
}

//--------------------------------------------------------------------------------------------------------------
// InputStreamTemplate
//--------------------------------------------------------------------------------------------------------------

template <typename BufferEndian>
inline void InputStreamTemplate<BufferEndian>::readString(std::string& val)
{
  // read the size of the sequence
  uint32_t size = 0U;
  readUInt32(size);

  if (size == 0U)
  {
    val.clear();
    return;
  }

  // ensure the sequence is empty
  val.clear();
  val.resize(size);

  // read the data
  std::memcpy(val.data(), advance(size), size);
}

template <typename BufferEndian>
template <typename T>
inline impl::IfBasic<T, void> InputStreamTemplate<BufferEndian>::readBasic(T& val)
{
  static_assert(std::is_trivially_copyable_v<T>);
  constexpr std::size_t size = sizeof(T);

  // copy the bytes into our memory
  std::memcpy(&val, advance(size), size);

  // swap if needed
  ::sen::impl::swapBytesIfNeeded(val, BufferEndian {});
}

}  // namespace sen

#endif  // SEN_CORE_IO_INPUT_STREAM_H
