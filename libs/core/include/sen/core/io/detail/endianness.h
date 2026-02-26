// === endianness.h ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_IO_DETAIL_ENDIANNESS_H
#define SEN_CORE_IO_DETAIL_ENDIANNESS_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/numbers.h"

// std
#include <cstdlib>

namespace sen
{

/// Tag type for referring to little endian encoding
struct LittleEndian
{
};

/// Tag type for referring to big endian encoding
struct BigEndian
{
};

namespace impl
{

/// Returns true if the current host is little endian
[[nodiscard]] constexpr bool hostIsBigEndian() noexcept
{
#if defined(__BYTE_ORDER__)
#  if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return true;
#  elif __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return false;
#  endif
#elif defined(__BIG_ENDIAN__)
  return true;
#elif defined(__LITTLE_ENDIAN__) || defined(__i386__) || defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64)
  return false;
#else
#  error "platform endianness not supported"
#endif
}

/// Returns true if the current host is big endian
[[nodiscard]] constexpr bool hostIsLittleEndian() noexcept { return !hostIsBigEndian(); }

[[nodiscard]] SEN_ALWAYS_INLINE int8_t swapBytes(int8_t value) noexcept { return value; }

[[nodiscard]] SEN_ALWAYS_INLINE uint8_t swapBytes(uint8_t value) noexcept { return value; }

[[nodiscard]] SEN_ALWAYS_INLINE int16_t swapBytes(int16_t value) noexcept
{
#if defined(__GNUC__)
  return static_cast<int16_t>(__builtin_bswap16(value));
#else
  return static_cast<int16_t>(_byteswap_ushort(value));
#endif
}

[[nodiscard]] SEN_ALWAYS_INLINE uint16_t swapBytes(uint16_t value) noexcept
{
#if defined(__GNUC__)
  return __builtin_bswap16(value);
#else
  return _byteswap_ushort(value);
#endif
}

[[nodiscard]] SEN_ALWAYS_INLINE int32_t swapBytes(int32_t value) noexcept
{
#if defined(__GNUC__)
  return static_cast<int32_t>(__builtin_bswap32(value));
#else
  return static_cast<int32_t>(_byteswap_ulong(value));
#endif
}

[[nodiscard]] SEN_ALWAYS_INLINE uint32_t swapBytes(uint32_t value) noexcept
{
#if defined(__GNUC__)
  return __builtin_bswap32(value);
#else
  return _byteswap_ulong(value);
#endif
}

[[nodiscard]] SEN_ALWAYS_INLINE int64_t swapBytes(int64_t value) noexcept
{
#if defined(__GNUC__)
  return static_cast<int64_t>(__builtin_bswap64(value));
#else
  return static_cast<int64_t>(_byteswap_uint64(value));
#endif
}

[[nodiscard]] SEN_ALWAYS_INLINE uint64_t swapBytes(uint64_t value) noexcept
{
#if defined(__GNUC__)
  return __builtin_bswap64(value);
#else
  return _byteswap_uint64(value);
#endif
}

[[nodiscard]] SEN_ALWAYS_INLINE float32_t swapBytes(float32_t value) noexcept { return value; }

[[nodiscard]] SEN_ALWAYS_INLINE float64_t swapBytes(float64_t value) noexcept { return value; }

/// Swaps the bytes of val if this host is big endian
template <typename T>
SEN_ALWAYS_INLINE void swapBytesIfNeeded(T& val, LittleEndian /* target */) noexcept
{
  if constexpr (hostIsBigEndian())
  {
    val = swapBytes(val);
  }
}

/// Swaps the bytes of val if this host is little endian
template <typename T>
SEN_ALWAYS_INLINE void swapBytesIfNeeded(T& val, BigEndian /* target */) noexcept
{
  if constexpr (hostIsLittleEndian())
  {
    val = swapBytes(val);
  }
}

}  // namespace impl

}  // namespace sen

#endif  // SEN_CORE_IO_DETAIL_ENDIANNESS_H
