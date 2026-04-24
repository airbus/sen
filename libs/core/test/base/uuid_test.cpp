// === uuid_test.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/span.h"
#include "sen/core/base/static_vector.h"
#include "sen/core/base/uuid.h"

// std
#include <array>
#include <cstdint>
#include <functional>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

// gtest
#include <gtest/gtest.h>

using sen::Uuid;
using sen::UuidVariant;
using sen::UuidVersion;

/// @test
/// Verifies that the default constructor creates a nil UUID
/// @requirements(SEN-584)
TEST(UuidTest, DefaultConstructorIsNil)
{
  const Uuid uuid;
  EXPECT_TRUE(uuid.isNil());
  EXPECT_EQ(uuid.getHash(), 0ULL);

  const auto bytes = uuid.bytes();
  for (const auto b: bytes)
  {
    EXPECT_EQ(b, 0U);
  }
}

/// @test
/// Verifies that constructing with two 64-bit integers stores the data correctly
/// @requirements(SEN-584, SEN-575)
TEST(UuidTest, ValueConstructorAndHash)
{
  constexpr uint64_t hi = 0x1122334455667788ULL;
  constexpr uint64_t lo = 0x99AABBCCDDEEFF00ULL;
  const Uuid uuid(hi, lo);

  EXPECT_FALSE(uuid.isNil());
  EXPECT_EQ(uuid.getHash(), hi ^ lo);

  constexpr uint64_t expectedHash = hi ^ lo;
  const auto expectedHash32 =
    sen::std_util::ignoredLossyConversion<uint32_t>((expectedHash ^ expectedHash >> 32U) & 0xFFFFFFFFULL);
  EXPECT_EQ(uuid.getHash32(), expectedHash32);
}

/// @test
/// Verifies that constructing from a span of bytes maps correctly to the internal state
/// @requirements(SEN-584, SEN-1051)
TEST(UuidTest, SpanConstructor)
{
  constexpr std::array<uint8_t, 16> inputBytes {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

  const Uuid uuid(sen::Span(inputBytes.data(), inputBytes.size()));
  const auto outputBytes = uuid.bytes();

  EXPECT_EQ(inputBytes, outputBytes);
}

/// @test
/// Verifies copying UUID to an array
/// @requirements(SEN-584, SEN-1051)
TEST(UuidTest, CopyArray)
{
  constexpr std::array<uint8_t, 16> inputBytes {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

  const Uuid uuid(sen::Span(inputBytes.data(), inputBytes.size()));
  std::array<uint8_t, Uuid::byteCount> outputArray {};
  uuid.copy(outputArray);

  EXPECT_EQ(inputBytes, outputArray);
}

/// @test
/// Verifies copying UUID to a StaticVector
/// @requirements(SEN-584, SEN-1051)
TEST(UuidTest, CopyStaticVector)
{
  constexpr uint64_t hi = 0x1122334455667788ULL;
  constexpr uint64_t lo = 0x99AABBCCDDEEFF00ULL;
  const Uuid uuid(hi, lo);
  sen::StaticVector<uint8_t, Uuid::byteCount> vec;
  uuid.copy(vec);

  const auto bytes = uuid.bytes();
  ASSERT_EQ(vec.size(), Uuid::byteCount);

  auto vecIt = vec.begin();
  for (const auto& b: bytes)
  {
    EXPECT_EQ(*vecIt, b);
    std::advance(vecIt, 1);
  }
}

/// @test
/// Verifies the parsing of the UUID variant based on the top bits
/// @requirements(SEN-584)
TEST(UuidTest, GetVariant)
{
  const std::vector<uint64_t> ncsValues = {0x0000000000000000ULL, 0x7000000000000000ULL, 0x7FFFFFFFFFFFFFFFULL};
  for (const auto val: ncsValues)
  {
    Uuid u(0ULL, val);
    EXPECT_EQ(u.getVariant(), UuidVariant::ncs);
  }

  const std::vector<uint64_t> rfcValues = {0x8000000000000000ULL, 0xA000000000000000ULL, 0xBFFFFFFFFFFFFFFFULL};
  for (const auto val: rfcValues)
  {
    Uuid u(0ULL, val);
    EXPECT_EQ(u.getVariant(), UuidVariant::rfc);
  }

  const std::vector<uint64_t> msValues = {0xC000000000000000ULL, 0xD000000000000000ULL, 0xDFFFFFFFFFFFFFFFULL};
  for (const auto val: msValues)
  {
    Uuid u(0ULL, val);
    EXPECT_EQ(u.getVariant(), UuidVariant::microsoft);
  }

  const std::vector<uint64_t> resValues = {0xE000000000000000ULL, 0xF000000000000000ULL, 0xFFFFFFFFFFFFFFFFULL};
  for (const auto val: resValues)
  {
    Uuid u(0ULL, val);
    EXPECT_EQ(u.getVariant(), UuidVariant::reserved);
  }
}

/// @test
/// Verifies the parsing of the UUID version based on the high bits
/// @requirements(SEN-584)
TEST(UuidTest, GetVersion)
{
  const std::vector<uint64_t> timeValues = {0x0000000000001000ULL, 0xFFFFFFFFFFFF1FFFULL};
  for (const auto val: timeValues)
  {
    Uuid u(val, 0ULL);
    EXPECT_EQ(u.getVersion(), UuidVersion::timeBased);
  }

  const std::vector<uint64_t> dceValues = {0x0000000000002000ULL, 0xFFFFFFFFFFFF2FFFULL};
  for (const auto val: dceValues)
  {
    Uuid u(val, 0ULL);
    EXPECT_EQ(u.getVersion(), UuidVersion::dceSecurity);
  }

  const std::vector<uint64_t> md5Values = {0x0000000000003000ULL, 0xFFFFFFFFFFFF3FFFULL};
  for (const auto val: md5Values)
  {
    Uuid u(val, 0ULL);
    EXPECT_EQ(u.getVersion(), UuidVersion::nameBasedMd5);
  }

  const std::vector<uint64_t> randValues = {0x0000000000004000ULL, 0xFFFFFFFFFFFF4FFFULL};
  for (const auto val: randValues)
  {
    Uuid u(val, 0ULL);
    EXPECT_EQ(u.getVersion(), UuidVersion::randomNumberBased);
  }

  const std::vector<uint64_t> sha1Values = {0x0000000000005000ULL, 0xFFFFFFFFFFFF5FFFULL};
  for (const auto val: sha1Values)
  {
    Uuid u(val, 0ULL);
    EXPECT_EQ(u.getVersion(), UuidVersion::nameBasedSha1);
  }

  const std::vector<uint64_t> noneValues = {
    0x0000000000000000ULL, 0x0000000000006000ULL, 0x000000000000A000ULL, 0x000000000000F000ULL};
  for (const auto val: noneValues)
  {
    Uuid u(val, 0ULL);
    EXPECT_EQ(u.getVersion(), UuidVersion::none);
  }
}

/// @test
/// Verifies string conversions and round-tripping string->uuid->string
/// @requirements(SEN-584, SEN-576)
TEST(UuidTest, StringConversions)
{
  const std::string validStr = "12345678-90ab-cdef-1234-567890abcdef";
  const std::string validStrWithBraces = "{12345678-90ab-cdef-1234-567890abcdef}";
  const std::string validStrUpper = "12345678-90AB-CDEF-1234-567890ABCDEF";

  Uuid u;

  Uuid fromNormal = u.fromString(validStr);
  EXPECT_FALSE(fromNormal.isNil());
  EXPECT_EQ(fromNormal.toString(), validStr);

  Uuid fromBraces = u.fromString(validStrWithBraces);
  EXPECT_FALSE(fromBraces.isNil());
  EXPECT_EQ(fromBraces.toString(), validStr);

  Uuid fromUpper = u.fromString(validStrUpper);
  EXPECT_FALSE(fromUpper.isNil());
  EXPECT_EQ(fromUpper.toString(), validStr);

  EXPECT_TRUE(u.fromString("").isNil());
  EXPECT_TRUE(u.fromString("{12345678-90ab-cdef-1234-567890abcdef").isNil());
  EXPECT_TRUE(u.fromString("12345678-90ab-cdef-1234-567890abcdeG").isNil());
  EXPECT_TRUE(u.fromString("12345678-90ab-cdef-1234-567890abcde").isNil());
  EXPECT_TRUE(u.fromString("12345678-90ab-cdef-1234-567890abcdef1").isNil());
  EXPECT_TRUE(u.fromString("{}").isNil());
}

/// @test
/// Verifies validation logic of string UUIDs
/// @requirements(SEN-584, SEN-576)
TEST(UuidTest, StringValidation)
{
  Uuid u;

  EXPECT_TRUE(u.isValid("12345678-90ab-cdef-1234-567890abcdef"));
  EXPECT_TRUE(u.isValid("{12345678-90ab-cdef-1234-567890abcdef}"));
  EXPECT_TRUE(u.isValid("1234567890abcdef1234567890abcdef"));

  EXPECT_FALSE(u.isValid(""));
  EXPECT_FALSE(u.isValid("{12345678-90ab-cdef-1234-567890abcdef"));
  EXPECT_FALSE(u.isValid("12345678-90ab-cdef-1234-567890abcdef}"));
  EXPECT_FALSE(u.isValid("12345678-90ab-cdef-1234-567890abcdeG"));
  EXPECT_FALSE(u.isValid("12345678-90ab-cdef-1234-567890abcde"));
  EXPECT_FALSE(u.isValid("12345678-90ab-cdef-1234-567890abcdef1"));
  EXPECT_FALSE(u.isValid("{}"));
  EXPECT_FALSE(u.isValid("}12345678-90ab-cdef-1234-567890abcdef"));
}

/// @test
/// Verifies comparison operators
/// @requirements(SEN-584)
TEST(UuidTest, Operators)
{
  constexpr uint64_t val1 = 1;
  constexpr uint64_t val2 = 2;
  constexpr uint64_t val3 = 3;
  constexpr uint64_t val0 = 0;

  const Uuid u1(val1, val2);
  const Uuid u2(val1, val2);
  const Uuid u3(val1, val3);
  const Uuid u4(val2, val0);

  EXPECT_TRUE(u1 == u2);
  EXPECT_FALSE(u1 != u2);

  EXPECT_TRUE(u1 != u3);
  EXPECT_FALSE(u1 == u3);

  EXPECT_TRUE(u1 < u3);
  EXPECT_FALSE(u3 < u1);
  EXPECT_TRUE(u1 < u4);
  EXPECT_TRUE(u3 < u4);
}

/// @test
/// Verifies the behavior of Uuid swap mechanism
/// @requirements(SEN-584)
TEST(UuidTest, Swap)
{
  Uuid u1(0xAAAA, 0xBBBB);
  Uuid u2(0xCCCC, 0xDDDD);

  u1.swap(u2);

  EXPECT_EQ(u1.getHash(), Uuid(0xCCCC, 0xDDDD).getHash());
  EXPECT_EQ(u2.getHash(), Uuid(0xAAAA, 0xBBBB).getHash());
}

/// @test
/// Verifies standard library integrations of hash and ostream
/// @requirements(SEN-584, SEN-1051)
TEST(UuidTest, StdIntegrations)
{
  const Uuid u(0x1111, 0x2222);

  constexpr std::hash<Uuid> hasher;
  EXPECT_EQ(hasher(u), u.getHash());

  std::stringstream ss;
  ss << u;
  EXPECT_EQ(ss.str(), u.toString());
}

/// @test
/// Verifies the properties of the random UUID generator
/// @requirements(SEN-584)
TEST(UuidTest, RandomGeneratorProducesCorrectSpec)
{
  constexpr uint64_t seed = 12345;
  sen::UuidRandomGenerator gen(seed);

  const Uuid randomId = gen();

  EXPECT_FALSE(randomId.isNil());

  EXPECT_EQ(randomId.getVersion(), UuidVersion::randomNumberBased);
  EXPECT_EQ(randomId.getVariant(), UuidVariant::rfc);
}
