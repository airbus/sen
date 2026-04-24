// === hash32.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_HASH32_H
#define SEN_CORE_BASE_HASH32_H

#include "sen/core/base/numbers.h"

// std
#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <numeric>
#include <string>
#include <string_view>

/// \file hash32.h
/// This file contains functions related to hashing and compression. This is mainly used by
/// Sen internals, but it's exposed to the public API as a convenience utility.

namespace sen
{

/// \addtogroup hash
/// @{

/// Initial seed for all hashes
constexpr u32 hashSeed = 23835769U;
constexpr u32 propertyHashSeed = 19830715U;
constexpr u32 methodHashSeed = 93580253U;
constexpr u32 eventHashSeed = 12125807U;

/// This hash is combined when no Type is found in a certain spec
constexpr u32 nonPresentTypeHash = 18121997U;

/// Calculates the CRC32 for any sequence of values. (You could use type traits and a
/// static assert to ensure the values can be converted to 8 bits.).
template <typename InputIterator>
[[nodiscard]] u32 crc32(InputIterator first, InputIterator last) noexcept;

/// CRC32 for strings.
[[nodiscard]] inline u32 crc32(std::string_view str) noexcept { return crc32(str.begin(), str.end()); }

/// Combines the hash of different values into a single 32-bit hash.
template <typename... Types>
[[nodiscard]] u32 hashCombine(u32 seed, const Types&... args) noexcept;

/// Old version of the hashCombine method. The current one replaced this implementation to allow sen processes
/// to discover and connect themselves between platforms (Windows and Linux). We keep this old implementation to
/// enable replaying Sen recordings generated with the old hashes.
template <typename... Types>
[[nodiscard]] uint_fast32_t platformDependentHashCombine(uint_fast32_t seed, const Types&... args) noexcept;

/// Decompresses a blob into its original shape.
[[nodiscard]] std::unique_ptr<unsigned char[]> decompressSymbol(const void* compressedData);

/// Decompresses a blob into a string.
[[nodiscard]] std::string decompressSymbolToString(const void* compressedData, unsigned int originalSize);

/// Creates a C++ source file that contains an array representing the contents of another file.
[[nodiscard]] bool fileToCompressedArrayFile(const std::filesystem::path& inputFile,
                                             std::string_view symbolName,
                                             const std::filesystem::path& outputFile);

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

namespace impl
{

// FN-1a implementation of the hash
constexpr u32 fnv1aOffsetBasis = 0x811c9dc5;
constexpr u32 fnv1aPrime = 0x01000193;

/// Processor for integral types
template <typename T>
inline constexpr std::enable_if_t<std::is_integral_v<T>, u32> hashIntegral(T value) noexcept
{
  using UT = std::make_unsigned_t<T>;
  auto u = static_cast<UT>(value);

  auto hash = fnv1aOffsetBasis;

  // process from most-significant byte to least-significant
  for (int i = (sizeof(UT) - 1) * 8; i >= 0; i -= 8)
  {
    hash ^= static_cast<u8>((u >> i) & 0xFF);  // NOLINT
    hash *= fnv1aPrime;
  }

  return hash;
}

/// Processor for floating point types
template <typename T>
inline constexpr std::enable_if_t<std::is_floating_point_v<T>, u32> hashFloat(T value) noexcept
{
  using IntType = std::conditional_t<sizeof(T) == 4, u32, u64>;

  IntType bits = 0;
  std::memcpy(&bits, &value, sizeof(T));
  return hashIntegral(bits);
}

template <typename T, typename Enable = void>
struct hash;

template <typename T>
struct hash<T, typename std::enable_if_t<std::is_integral_v<T>>>
{
  inline u32 operator()(T value) const noexcept { return hashIntegral(value); }
};

template <typename T>
struct hash<T, typename std::enable_if_t<std::is_floating_point_v<T>>>
{
  inline u32 operator()(T value) const noexcept { return hashFloat(value); }
};

template <>
struct hash<bool>
{
  inline u32 operator()(bool value) const noexcept { return hashIntegral(static_cast<u8>(value)); }
};

template <>
struct hash<i8>
{
  inline u32 operator()(i8 value) const noexcept { return hashIntegral(static_cast<u8>(value)); }
};

template <>
struct hash<std::basic_string_view<char>>
{
  inline u32 operator()(const std::basic_string_view<char>& value) const noexcept
  {
    auto hash = fnv1aOffsetBasis;

    for (unsigned char c: value)
    {
      hash ^= c;
      hash *= fnv1aPrime;
    }

    return hash;
  }
};

template <>
struct hash<std::basic_string<char>>
{
  inline u32 operator()(const std::basic_string<char>& value) const noexcept
  {
    return hash<std::basic_string_view<char>>()(value);
  }
};

template <typename T>
struct hash<T, std::enable_if_t<std::is_enum_v<T>>>
{
  inline u32 operator()(T value) const noexcept { return hashIntegral(static_cast<std::underlying_type_t<T>>(value)); }
};

template <typename T>
struct hash<T*>
{
  u32 operator()(T* value) const noexcept
  {
    return hashIntegral(reinterpret_cast<std::uintptr_t>(value));  // NOLINT
  }
};

[[nodiscard]] std::array<u32, 256> generateCrc32LookupTable() noexcept;  // NOLINT

template <typename T>
inline void hashCombineImpl(u32& seed, const T& val) noexcept
{
  seed ^= hash<T>()(val) + 0x9e3779b9U + (seed << 6) + (seed >> 2);  // NOLINT
}

template <typename T>
inline void platformDependentHashCombineImpl(std::uint_fast32_t& seed, const T& val) noexcept
{
  seed ^= std::hash<T>()(val) + 0x9e3779b9U + (seed << 6) + (seed >> 2);  // NOLINT
}

}  // namespace impl

template <typename InputIterator>
inline u32 crc32(InputIterator first, InputIterator last) noexcept
{
  // generate lookup table only on first use then cache it - this is thread-safe.
  static auto const table = impl::generateCrc32LookupTable();

  // calculate the checksum - make sure to clip to 32 bits, for systems that don't
  // have a true (fast) 32-bit type.
  return u32 {0xffffffffUL} &  // NOLINT
         ~std::accumulate(first,
                          last,
                          ~u32 {0} & u32 {0xffffffffUL},  // NOLINT
                          [](u32 checksum, u32 value)
                          { return table[(checksum ^ value) & 0xffU] ^ (checksum >> 8U); });  // NOLINT
}

template <typename... Types>
inline u32 hashCombine(u32 seed, const Types&... args) noexcept
{
  (impl::hashCombineImpl(seed, args), ...);
  return seed;
}

template <typename... Types>
inline std::uint_fast32_t platformDependentHashCombine(std::uint_fast32_t seed, const Types&... args) noexcept
{
  (impl::platformDependentHashCombineImpl(seed, args), ...);
  return seed;
}

}  // namespace sen

#endif  // SEN_CORE_BASE_HASH32_H
