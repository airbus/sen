// === base64_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/base64.h"

// google test
#include <gtest/gtest.h>

// std
#include <ios>
#include <string>
#include <string_view>

namespace
{

/// @test
/// A length that is a multiple of three encodes with no padding.
TEST(Base64, NoPaddingFor3ByteMultiple)
{
  // "abc" -> YWJj (three bytes are four characters, no '=').
  EXPECT_EQ(sen::std_util::base64Encode("abc"), "YWJj");
}
/// @test
/// Two bytes encode with one padding character.
TEST(Base64, SinglePaddingFor2Bytes) { EXPECT_EQ(sen::std_util::base64Encode("ab"), "YWI="); }
/// @test
/// One byte encodes with two padding characters.
TEST(Base64, DoublePaddingFor1Byte) { EXPECT_EQ(sen::std_util::base64Encode("a"), "YQ=="); }
/// @test
/// Empty input encodes to an empty string.
TEST(Base64, EmptyInput) { EXPECT_EQ(sen::std_util::base64Encode(""), ""); }
/// @test
/// Encodes bytes above the ASCII range, 0xFF 0xFE 0xFD becoming //79 at the top of the six bit
/// range.
TEST(Base64, HandlesNonAsciiBytes)
{
  // 0xFF 0xFE 0xFD -> //79, which exercises the top of the 6-bit range and both alphabet tails.
  EXPECT_EQ(sen::std_util::base64Encode(std::string("\xFF\xFE\xFD", 3)), "//79");
}
/// @test
/// Encodes "Hello, World!" to "SGVsbG8sIFdvcmxkIQ==".
TEST(Base64, KnownValueHelloWorld) { EXPECT_EQ(sen::std_util::base64Encode("Hello, World!"), "SGVsbG8sIFdvcmxkIQ=="); }
/// @test
/// The output holds only base64 alphabet characters and padding.
TEST(Base64, OutputUsesOnlyAlphabetChars)
{
  const auto encoded = sen::std_util::base64Encode("The quick brown fox jumps over the lazy dog");
  EXPECT_FALSE(encoded.empty());
  for (char c: encoded)
  {
    const bool ok =
      (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
    EXPECT_TRUE(ok) << "Unexpected character in base64 payload: 0x" << std::hex << static_cast<int>(c);
  }
}

}  // namespace
