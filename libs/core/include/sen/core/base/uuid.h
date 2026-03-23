// === uuid.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_UUID_H
#define SEN_CORE_BASE_UUID_H

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/span.h"
#include "sen/core/base/static_vector.h"

// std
#include <array>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <string_view>

namespace sen
{
/// \addtogroup util
/// @{

/// Indicated by a bit pattern in octet 8, marked with N in xxxxxxxx-xxxx-xxxx-Nxxx-xxxxxxxxxxxx.
enum class UuidVariant
{
  ncs = 0,        ///< NCS backward compatibility
  rfc = 1,        ///< RFC 4122/DCE 1.1
  microsoft = 2,  ///< Microsoft Corporation backward compatibility
  reserved = 3    ///< reserved for possible future definition
};

/// Indicated by a bit pattern in octet 6, marked with M in xxxxxxxx-xxxx-Mxxx-xxxx-xxxxxxxxxxxx.
enum class UuidVersion
{
  none = 0,               ///< Only possible for nil or invalid uuids.
  timeBased = 1,          ///< The time-based version specified in RFC 4122.
  dceSecurity = 2,        ///< DCE Security version, with embedded POSIX UIDs.
  nameBasedMd5 = 3,       ///< The name-based version specified in RFS 4122 with MD5 hashing.
  randomNumberBased = 4,  ///< The randomly or pseudo-randomly generated version specified in RFS 4122.
  nameBasedSha1 = 5       ///< The name-based version specified in RFS 4122 with SHA1 hashing.
};

/// Universal Unique Identifier (RFC 4122).
/// Stored as two 64-bit integers (hi and lo) for efficient comparison and hashing.
/// The nil UUID (all zeroes) is the default-constructed value.
class Uuid
{
  SEN_COPY_MOVE(Uuid)

public:
  /// Number of bytes in a UUID (16, as per RFC 4122).
  static constexpr std::size_t byteCount = 16;

public:
  /// Constructs a nil UUID (all zeroes).
  constexpr Uuid() noexcept = default;

  /// Constructs a UUID from two consecutive 64-bit integers.
  /// @note Does not validate the data against RFC 4122.
  /// @param hi High 64 bits (time-low, time-mid, time-hi-and-version).
  /// @param lo Low 64 bits (clock-seq and node).
  constexpr Uuid(uint64_t hi, uint64_t lo) noexcept: hi_(hi), lo_(lo) {}

  /// Constructs a UUID from a raw byte span.
  /// @note Does not validate the data against RFC 4122.
  /// @param bytes Span of at least 16 bytes in big-endian RFC 4122 order.
  explicit Uuid(Span<const uint8_t> bytes) noexcept;

  ~Uuid() = default;

public:
  /// @return True if all 128 bits are zero (nil UUID).
  [[nodiscard]] constexpr bool isNil() const noexcept { return (hi_ | lo_) == 0; }

  /// @return The variant field of the UUID as defined in RFC 4122.
  [[nodiscard]] constexpr UuidVariant getVariant() const noexcept;

  /// @return The version field of the UUID as defined in RFC 4122.
  [[nodiscard]] constexpr UuidVersion getVersion() const noexcept;

  /// Computes a 64-bit hash by XOR-ing the high and low halves.
  /// @return A 64-bit hash value suitable for use in hash maps.
  [[nodiscard]] constexpr uint64_t getHash() const noexcept { return hi_ ^ lo_; }

  /// Computes a 32-bit hash by folding the 64-bit hash.
  /// @return A 32-bit hash value derived from getHash().
  [[nodiscard]] constexpr uint32_t getHash32() const noexcept;

  /// Returns the UUID as a 16-byte array in RFC 4122 byte order.
  /// @return Array of 16 bytes, high bytes first.
  [[nodiscard]] std::array<uint8_t, byteCount> bytes() const noexcept;

  /// Serialises this UUID to the canonical string form
  /// `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`.
  /// @return Lowercase hex string with dashes, 36 characters long.
  [[nodiscard]] std::string toString() const;

  /// Checks whether the string @p s is a well-formed UUID string.
  /// @param s String to validate (expected format: `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`).
  /// @return True if @p s can be parsed as a valid UUID.
  [[nodiscard]] bool isValid(std::string_view s) noexcept;

  /// Parses a UUID from a canonical string.
  /// @param s String in the form `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx`.
  /// @return Parsed UUID, or a nil UUID if @p s is malformed.
  [[nodiscard]] Uuid fromString(std::string_view s) noexcept;

  /// Copies the UUID bytes into an existing std::array.
  /// @param arr Output array of exactly 16 bytes; overwritten with the UUID bytes.
  void copy(std::array<uint8_t, byteCount>& arr) const { arr = bytes(); }

  /// Copies the UUID bytes into an existing StaticVector.
  /// @param vec Output vector; resized to 16 and overwritten with the UUID bytes.
  void copy(StaticVector<uint8_t, byteCount>& vec) const;

  /// Swaps the contents of this UUID with @p other.
  /// @param other The UUID to swap with.
  void swap(Uuid& other) noexcept;

private:
  friend constexpr bool operator==(const Uuid& lhs, const Uuid& rhs) noexcept;
  friend constexpr bool operator!=(const Uuid& lhs, const Uuid& rhs) noexcept;
  friend constexpr bool operator<(const Uuid& lhs, const Uuid& rhs) noexcept;

private:
  uint64_t hi_ {};
  uint64_t lo_ {};
};

// --------------------------------------------------------------------------------------------------------------------------
// Inline implementation
// --------------------------------------------------------------------------------------------------------------------------

[[nodiscard]] constexpr bool operator==(const Uuid& lhs, const Uuid& rhs) noexcept
{
  return lhs.hi_ == rhs.hi_ && lhs.lo_ == rhs.lo_;
}

[[nodiscard]] constexpr bool operator!=(const Uuid& lhs, const Uuid& rhs) noexcept { return !(lhs == rhs); }

[[nodiscard]] constexpr bool operator<(const Uuid& lhs, const Uuid& rhs) noexcept
{
  return (lhs.hi_ < rhs.hi_) || (lhs.hi_ == rhs.hi_ && lhs.lo_ < rhs.lo_);
}

inline Uuid::Uuid(Span<const uint8_t> bytes) noexcept
{
  SEN_ASSERT(bytes.size() >= 16);
  std::memcpy(&hi_, bytes.data(), sizeof(uint64_t));
  std::memcpy(&lo_, bytes.data() + sizeof(uint64_t), sizeof(uint64_t));
}

constexpr uint32_t Uuid::getHash32() const noexcept
{
  uint64_t h = getHash();
  return static_cast<uint32_t>(h ^ (h >> 32));
}

constexpr UuidVariant Uuid::getVariant() const noexcept
{
  auto b = static_cast<uint8_t>(lo_ >> 56);

  if ((b & 0x80) == 0)
  {
    return UuidVariant::ncs;
  }

  if ((b & 0xC0) == 0x80)
  {
    return UuidVariant::rfc;
  }

  if ((b & 0xE0) == 0xC0)
  {
    return UuidVariant::microsoft;
  }

  return UuidVariant::reserved;
}

constexpr UuidVersion Uuid::getVersion() const noexcept
{
  uint8_t v = static_cast<uint8_t>(hi_ >> 12) & 0xF;

  switch (v)
  {
    case 1:
      return UuidVersion::timeBased;
    case 2:
      return UuidVersion::dceSecurity;
    case 3:
      return UuidVersion::nameBasedMd5;
    case 4:
      return UuidVersion::randomNumberBased;
    case 5:
      return UuidVersion::nameBasedSha1;
    default:
      return UuidVersion::none;
  }
}

inline std::array<uint8_t, Uuid::byteCount> Uuid::bytes() const noexcept
{
  std::array<uint8_t, byteCount> out {};
  std::memcpy(out.data(), &hi_, 8U);
  std::memcpy(out.data() + 8U, &lo_, 8U);
  return out;
}

inline void Uuid::swap(Uuid& other) noexcept
{
  std::swap(hi_, other.hi_);
  std::swap(lo_, other.lo_);
}

inline void Uuid::copy(StaticVector<uint8_t, byteCount>& vec) const
{
  vec.resize(byteCount);
  memcpy(vec.data(), bytes().data(), byteCount);
}

// --------------------------------------------------------------------------------------------------------------------------
// UuidRandomGenerator
// --------------------------------------------------------------------------------------------------------------------------

/// Generates version-4 (randomly generated) UUIDs.
/// Satisfies the `UniformRandomBitGenerator` concept so it can be used
/// wherever a standard generator is expected.
///
/// @code
/// UuidRandomGenerator gen;
/// Uuid id = gen();  // new random UUID
/// @endcode
class UuidRandomGenerator
{
public:
  /// Constructs a generator seeded from a non-deterministic source (`std::random_device`).
  UuidRandomGenerator(): engine_(std::random_device {}()) {}

  /// Constructs a generator with a fixed @p seed for reproducible output.
  /// @param seed Seed value for the internal Mersenne Twister engine.
  explicit UuidRandomGenerator(uint64_t seed): engine_(seed) {}

  /// Generates and returns a new version-4 UUID.
  /// @return A randomly generated UUID with variant RFC 4122 and version 4.
  Uuid operator()()
  {
    uint64_t hi = dist_(engine_);
    uint64_t lo = dist_(engine_);

    hi &= 0xFFFFFFFFFFFF0FFFULL;
    hi |= 0x0000000000004000ULL;

    lo &= 0x3FFFFFFFFFFFFFFFULL;
    lo |= 0x8000000000000000ULL;

    return {hi, lo};
  }

private:
  std::mt19937_64 engine_;
  std::uniform_int_distribution<uint64_t> dist_;
};

std::ostream& operator<<(std::ostream& s, const Uuid& id);
}  // namespace sen

// --------------------------------------------------------------------------------------------------------------------------
// Hashing support
// --------------------------------------------------------------------------------------------------------------------------

namespace std
{
template <>
struct hash<sen::Uuid>
{
  size_t operator()(const sen::Uuid& uuid) const noexcept { return uuid.getHash(); }
};
}  // namespace std

#endif  // SEN_CORE_BASE_UUID_H
