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

/// Initial seed for generic Sen hashes.
constexpr u32 hashSeed = 23835769U;
/// Seed used specifically for property member hashes.
constexpr u32 propertyHashSeed = 19830715U;
/// Seed used specifically for method member hashes.
constexpr u32 methodHashSeed = 93580253U;
/// Seed used specifically for event member hashes.
constexpr u32 eventHashSeed = 12125807U;

/// Sentinel hash value combined into a spec hash when a `Type` is absent (null handle).
constexpr u32 nonPresentTypeHash = 18121997U;

/// Computes a CRC-32 checksum over any iterator range whose elements are byte-convertible.
/// @tparam InputIterator  Iterator type; elements must be convertible to 8-bit values.
/// @param first  Start of the range.
/// @param last   Past-the-end of the range.
/// @return 32-bit CRC checksum of the byte sequence `[first, last)`.
template <typename InputIterator>
[[nodiscard]] u32 crc32(InputIterator first, InputIterator last) noexcept;

/// Computes a CRC-32 checksum over the bytes of a string.
/// @param str  Input string view.
/// @return 32-bit CRC checksum of @p str's bytes.
[[nodiscard]] inline u32 crc32(std::string_view str) noexcept { return crc32(str.begin(), str.end()); }

/// Combines the hashes of one or more values into a single 32-bit hash starting from @p seed.
/// Produces platform-independent (portable) results suitable for cross-process comparison.
/// @tparam Types  Pack of hashable value types.
/// @param seed  Starting seed (use `hashSeed` or a domain-specific seed constant).
/// @param args  Values to fold into the hash.
/// @return Combined 32-bit hash.
template <typename... Types>
[[nodiscard]] u32 hashCombine(u32 seed, const Types&... args) noexcept;

/// Legacy (platform-dependent) hash-combine kept for backward compatibility with old recordings.
/// The current `hashCombine()` replaced this to enable cross-platform (Windows â†” Linux) discovery.
/// Use only when replaying Sen recordings that were generated with the old hash scheme.
/// @tparam Types  Pack of hashable value types.
/// @param seed  Starting seed.
/// @param args  Values to fold into the hash.
/// @return Combined platform-dependent 32-bit hash.
template <typename... Types>
[[nodiscard]] uint_fast32_t platformDependentHashCombine(uint_fast32_t seed, const Types&... args) noexcept;

/// Decompresses an in-memory compressed blob and returns a pointer to the raw bytes.
/// The caller is responsible for freeing the returned buffer.
/// @param compressedData  Pointer to the compressed byte blob.
/// @return Pointer to the decompressed bytes; must be freed by the caller.
unsigned char* decompressSymbol(const void* compressedData);

/// Decompresses an in-memory blob into a `std::string`.
/// @param compressedData  Pointer to the compressed byte blob.
/// @param originalSize    Expected size of the decompressed content in bytes.
/// @return Decompressed content as a `std::string`.
[[nodiscard]] std::string decompressSymbolToString(const void* compressedData, unsigned int originalSize);

/// Compresses the contents of @p inputFile and writes a C++ source file containing
/// the result as a named `unsigned char[]` array â€” for embedding binary assets in code.
/// @param inputFile   Path to the binary file to embed.
/// @param symbolName  C identifier to use for the generated array symbol.
/// @param outputFile  Path to write the generated `.cpp` source file.
/// @return `true` on success; `false` if the input file cannot be read or the output cannot be written.
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
