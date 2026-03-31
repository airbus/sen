// === serialization_traits.h ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_IO_DETAIL_SERIALIZATION_TRAITS_H
#define SEN_CORE_IO_DETAIL_SERIALIZATION_TRAITS_H

// sen
#include "sen/core/base/timestamp.h"
#include "sen/core/io/detail/endianness.h"

// std
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

namespace sen::impl
{

// clang-format off

/// True if T is integral (chars and booleans not included).
template <typename T>
inline constexpr bool isPureIntegral = std::is_same_v<int8_t, T>  || std::is_same_v<uint8_t, T>  ||
                                       std::is_same_v<int16_t, T> || std::is_same_v<uint16_t, T> ||
                                       std::is_same_v<int32_t, T> || std::is_same_v<uint32_t, T> ||
                                       std::is_same_v<int64_t, T> || std::is_same_v<uint64_t, T>;

// clang-format on

/// True if T is integral or IEEE-754 floating point.
template <typename T>
inline constexpr bool isNumeric =
  isPureIntegral<T> || (std::is_floating_point_v<T> && std::numeric_limits<T>::is_iec559);

/// True if T can be directly written to a buffer (without transformation) if endianness matches.
template <typename T>
inline constexpr bool isBasic = isNumeric<T> || std::is_same_v<char, T> || std::is_same_v<unsigned char, T>;

/// True if T can be memcopied without gaps in a sequence
/// of contiguous memory (like std::vector or sen::StaticVector)
template <typename T>
inline constexpr bool allowsContiguousIO =
  !std::is_same_v<T, bool> &&
  (isBasic<T> || std::is_enum_v<T> || (std::is_trivially_copyable_v<T> && !std::is_class_v<T>)) &&
  (hostIsLittleEndian() || sizeof(T) == 1U);

static_assert(!allowsContiguousIO<std::vector<std::string>>);
static_assert(!allowsContiguousIO<std::vector<bool>>);

/// R if T is basic.
template <typename T, typename R>
using IfBasic = typename std::enable_if_t<isBasic<T>, R>;

/// R if T is an enumeration.
template <typename T, typename R>
using IfEnum = typename std::enable_if_t<std::is_enum_v<T>, R>;

/// To transport booleans (non-zero means true).
using BoolTransportType = uint8_t;

/// Detects at compile time if T can be streamed to/from S
template <typename S, typename T>
struct IsStreamable
{
private:
  template <typename SS, typename TT>
  static auto outputTest(unsigned) -> decltype(std::declval<SS&>() << std::declval<TT>(), std::true_type());

  template <typename, typename>
  static auto outputTest(...) -> std::false_type;

  template <typename SS, typename TT>
  static auto inputTest(unsigned) -> decltype(std::declval<SS&>() >> std::declval<TT&>(), std::true_type());

  template <typename, typename>
  static auto inputTest(...) -> std::false_type;

public:
  static constexpr bool output = decltype(outputTest<S, T>(0U))::value;
  static constexpr bool input = decltype(inputTest<S, T>(0U))::value;
};

[[nodiscard]] constexpr uint32_t getSerializedSize(bool val) noexcept
{
  std::ignore = val;
  return sizeof(BoolTransportType);
}

template <typename T>
[[nodiscard]] constexpr IfBasic<T, uint32_t> getSerializedSize(T val) noexcept
{
  std::ignore = val;
  return sizeof(T);
}

template <typename T>
[[nodiscard]] constexpr IfEnum<T, uint32_t> getSerializedSize(T val) noexcept
{
  return getSerializedSize(static_cast<std::underlying_type_t<T>>(val));
}

[[nodiscard]] inline uint32_t getSerializedSize(const std::string& val) noexcept
{
  const auto size = static_cast<uint32_t>(val.size());
  const auto sizeSize = getSerializedSize(size);

  // early exit
  if (size == 0U)
  {
    return sizeSize;
  }

  return sizeSize + size;
}

[[nodiscard]] constexpr uint32_t getSerializedSize(TimeStamp val) noexcept
{
  std::ignore = val;
  return getSerializedSize(int64_t {0});
}

}  // namespace sen::impl

#endif  // SEN_CORE_IO_DETAIL_SERIALIZATION_TRAITS_H
