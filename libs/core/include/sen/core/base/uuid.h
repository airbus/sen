// === uuid.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_BASE_UUID_H
#define SEN_CORE_BASE_UUID_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/static_vector.h"

// intrinsics
#include <emmintrin.h>
#include <immintrin.h>
#include <smmintrin.h>

// std
#include <array>
#include <cstdint>
#include <cstring>
#include <iosfwd>
#include <random>
#include <string>

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

/// Universal Unique Identifier
class Uuid
{
public:
  static constexpr std::size_t byteCount = 16U;

public:  // special members
  Uuid() = default;
  ~Uuid() = default;
  Uuid(const Uuid& other);
  Uuid(Uuid&& other) noexcept;

public:
  /// Constructs the Uuid using the low-level registry as source.
  /// Does not validate the data.
  explicit Uuid(__m128i uuid) noexcept;

  /// Construct it from an array of bytes.
  /// Does not validate the data.
  explicit Uuid(uint8_t (&arr)[byteCount]) noexcept;  // NOSONAR

  /// Construct it from an std::array of bytes.
  /// Does not validate the data.
  explicit Uuid(const std::array<uint8_t, byteCount>& arr) noexcept;

  /// Construct it from two consecutive 64-bit integers.
  /// Does not validate the data.
  explicit Uuid(uint64_t x, uint64_t y);

  /// Construct it from a memory area.
  /// Does not validate the data.
  explicit Uuid(const uint8_t* bytes);

public:
  Uuid& operator=(const Uuid& other);
  Uuid& operator=(Uuid&& other) noexcept;

public:
  [[nodiscard]] bool operator==(const Uuid& rhs) const noexcept;
  [[nodiscard]] bool operator!=(const Uuid& rhs) const noexcept;
  [[nodiscard]] bool operator<(const Uuid& rhs) const noexcept;

public:
  /// The variant of the UUID according to the spec.
  [[nodiscard]] UuidVariant getVariant() const noexcept;

  /// The version of the UUID according to the spec.
  [[nodiscard]] UuidVersion getVersion() const noexcept;

  /// True if all zeroes.
  [[nodiscard]] bool isNil() const noexcept;

  /// Computes a 64-bit hash of the UUID.
  [[nodiscard]] uint64_t getHash() const noexcept;

  /// Computes a 32-bit hash of the UUID.
  [[nodiscard]] uint32_t getHash32() const noexcept;

  /// True if the UUID is well-formed.
  [[nodiscard]] static bool isValid(const std::string& str) noexcept;

  /// Parses a string and generates a UUD.
  /// Returns a Nil UUID in case of error.
  [[nodiscard]] static Uuid fromString(const std::string& str) noexcept;

  /// Builds a string out of this UUID.
  [[nodiscard]] std::string toString() const;

  /// A copy of the bytes held in this UUID.
  [[nodiscard]] StaticVector<uint8_t, byteCount> getBytes() const;

  /// Copy this std::array of bytes into the UUID.
  void copy(std::array<uint8_t, byteCount>& arr) const { memcpy(arr.data(), bytes_.data(), arr.size()); }

  /// Copy this StaticVector of bytes into the UUID.
  void copy(StaticVector<uint8_t, byteCount>& vec) const;

  /// Swap the internal data.
  void swap(Uuid& other) noexcept;

private:
  [[nodiscard]] SEN_ALWAYS_INLINE static const __m128i* constDataCast(const uint8_t* data) noexcept
  {
    return reinterpret_cast<const __m128i*>(data);  // NOLINT NOSONAR
  }

  [[nodiscard]] SEN_ALWAYS_INLINE static __m128i* dataCast(uint8_t* data) noexcept
  {
    return reinterpret_cast<__m128i*>(data);  // NOLINT NOSONAR
  }

private:
  alignas(128) std::array<uint8_t, byteCount> bytes_ = {0U};
};

std::ostream& operator<<(std::ostream& s, const Uuid& id);

// --------------------------------------------------------------------------------------------------------------------------
// uuid generators
// --------------------------------------------------------------------------------------------------------------------------

/// Generates UUIDs using the std random number generator.
class UuidRandomGenerator
{
public:
  /// Use the default seed.
  UuidRandomGenerator()
    : generator_(std::random_device()())
    , distribution_(std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max())
  {
  }

  /// Sets a custom seed.
  explicit UuidRandomGenerator(uint64_t seed)
    : generator_(seed), distribution_(std::numeric_limits<uint64_t>::min(), std::numeric_limits<uint64_t>::max())
  {
  }

public:
  /// Makes a UUID.
  Uuid operator()();

private:
  std::mt19937_64 generator_;
  std::uniform_int_distribution<uint64_t> distribution_;
};

/// @}

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline Uuid::Uuid(const Uuid& other)
{
  __m128i x = _mm_load_si128(constDataCast(other.bytes_.data()));
  _mm_store_si128(dataCast(bytes_.data()), x);
}

inline Uuid::Uuid(Uuid&& other) noexcept
{
  __m128i x = _mm_load_si128(constDataCast(other.bytes_.data()));
  _mm_store_si128(dataCast(bytes_.data()), x);
}

inline Uuid::Uuid(__m128i uuid) noexcept { _mm_store_si128(dataCast(bytes_.data()), uuid); }

inline Uuid::Uuid(uint8_t (&arr)[byteCount]) noexcept  // NOSONAR
{
  __m128i x = _mm_loadu_si128(constDataCast(arr));
  _mm_store_si128(dataCast(bytes_.data()), x);
}

inline Uuid::Uuid(const std::array<uint8_t, byteCount>& arr) noexcept
{
  __m128i x = _mm_loadu_si128(constDataCast(arr.data()));
  _mm_store_si128(dataCast(bytes_.data()), x);
}

inline Uuid::Uuid(uint64_t x, uint64_t y)
{
  __m128i z = _mm_set_epi64x(x, y);  // NOLINT
  _mm_store_si128(dataCast(bytes_.data()), z);
}

inline Uuid::Uuid(const uint8_t* bytes)
{
  __m128i x = _mm_loadu_si128(constDataCast(bytes));
  _mm_store_si128(dataCast(bytes_.data()), x);
}

inline Uuid& Uuid::operator=(const Uuid& other)
{
  if (&other == this)
  {
    return *this;
  }
  __m128i x = _mm_load_si128(constDataCast(other.bytes_.data()));
  _mm_store_si128(dataCast(bytes_.data()), x);
  return *this;
}

inline Uuid& Uuid::operator=(Uuid&& other) noexcept
{
  if (&other == this)
  {
    return *this;
  }

  __m128i x = _mm_load_si128(constDataCast(other.bytes_.data()));
  _mm_store_si128(dataCast(bytes_.data()), x);
  return *this;
}

inline UuidVariant Uuid::getVariant() const noexcept
{
  if ((bytes_[8U] & 0x80) == 0x00)  // NOLINT
  {
    return UuidVariant::ncs;
  }
  else if ((bytes_[8U] & 0xC0) == 0x80)  // NOLINT
  {
    return UuidVariant::rfc;
  }
  else if ((bytes_[8U] & 0xE0) == 0xC0)  // NOLINT NOSONAR
  {
    return UuidVariant::microsoft;
  }

  return UuidVariant::reserved;
}

inline void Uuid::copy(StaticVector<uint8_t, byteCount>& vec) const
{
  vec.resize(byteCount);
  memcpy(vec.data(), bytes_.data(), byteCount);
}

inline StaticVector<uint8_t, Uuid::byteCount> Uuid::getBytes() const
{
  StaticVector<uint8_t, Uuid::byteCount> result;
  copy(result);
  return result;
}

inline UuidVersion Uuid::getVersion() const noexcept
{
  if ((bytes_[6U] & 0xF0) == 0x10)  // NOLINT
  {
    return UuidVersion::timeBased;
  }
  else if ((bytes_[6U] & 0xF0) == 0x20)  // NOLINT
  {
    return UuidVersion::dceSecurity;
  }
  else if ((bytes_[6U] & 0xF0) == 0x30)  // NOLINT
  {
    return UuidVersion::nameBasedMd5;
  }
  else if ((bytes_[6U] & 0xF0) == 0x40)  // NOLINT
  {
    return UuidVersion::randomNumberBased;
  }
  else if ((bytes_[6U] & 0xF0) == 0x50)  // NOLINT NOSONAR
  {
    return UuidVersion::nameBasedSha1;
  }

  return UuidVersion::none;
}

inline bool Uuid::isNil() const noexcept
{
  __m128i x = _mm_load_si128(constDataCast(bytes_.data()));
  return _mm_test_all_zeros(x, x);
}

inline void Uuid::swap(Uuid& other) noexcept { std::swap(bytes_, other.bytes_); }

inline uint64_t Uuid::getHash() const noexcept
{
  return *(reinterpret_cast<const uint64_t*>(bytes_.data())) ^                    // NOLINT NOSONAR
         *(reinterpret_cast<const uint64_t*>(bytes_.data()) + sizeof(uint64_t));  // NOLINT NOSONAR
}

inline uint32_t Uuid::getHash32() const noexcept
{
  const auto hash64 = getHash();
  return static_cast<uint32_t>(hash64 ^ (hash64 >> 32U));  // NOLINT NOSONAR
}

inline bool Uuid::operator==(const Uuid& rhs) const noexcept
{
  __m128i x = _mm_load_si128(constDataCast(bytes_.data()));
  __m128i y = _mm_load_si128(constDataCast(rhs.bytes_.data()));
  __m128i neq = _mm_xor_si128(x, y);
  return _mm_test_all_zeros(neq, neq);
}

inline bool Uuid::operator!=(const Uuid& rhs) const noexcept { return !(*this == rhs); }

inline bool Uuid::operator<(const Uuid& rhs) const noexcept
{
  // There are no trivial 128-bits comparisons in SSE/AVX
  // It's faster to compare two uint64_t
  const auto* x = reinterpret_cast<const uint64_t*>(bytes_.data());      // NOLINT NOSONAR
  const auto* y = reinterpret_cast<const uint64_t*>(rhs.bytes_.data());  // NOLINT NOSONAR
  return *x < *y || (*x == *y && *(x + 1U) < *(y + 1U));                 // NOLINT NOSONAR
}

// --------------------------------------------------------------------------------------------------------------------------
// UuidRandomGenerator
// --------------------------------------------------------------------------------------------------------------------------

inline Uuid UuidRandomGenerator::operator()()
{
  // The two masks set the uuid version (4) and variant (1)
  const __m128i and_mask = _mm_set_epi64x(0xffffffffffffff3fULL, 0xff0fffffffffffffULL);  // NOLINT
  const __m128i or_mask = _mm_set_epi64x(0x0000000000000080ULL, 0x0040000000000000ULL);   // NOLINT
  __m128i n = _mm_set_epi64x(distribution_(generator_), distribution_(generator_));       // NOLINT
  __m128i uuid = _mm_or_si128(_mm_and_si128(n, and_mask), or_mask);

  return Uuid(uuid);
}

}  // namespace sen

// --------------------------------------------------------------------------------------------------------------------------
// std
// --------------------------------------------------------------------------------------------------------------------------

namespace std  // NOLINT
{

template <>
struct hash<sen::Uuid>
{
  size_t operator()(const sen::Uuid& uuid) const noexcept { return uuid.getHash(); }
};

}  // namespace std

#endif  // SEN_CORE_BASE_UUID_H
