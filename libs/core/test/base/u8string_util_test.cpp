// === u8string_util_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// gtest
#include <gtest/gtest.h>

// sen
#include "sen/core/base/u8string_util.h"

// std
#include <string>
#include <utility>

/// @test
/// Verifies fromU8string correctly returns a copy of a standard lvalue std::string without modifying data
/// @requirements(SEN-576)
TEST(U8StringUtilTest, FromU8StringConstStdString)
{
  const std::string input = "Hello UTF-8 \xF0\x9F\x8C\x8D";

  const std::string result = sen::std_util::fromU8string(input);

  EXPECT_EQ(result, input);
  EXPECT_EQ(result.size(), input.size());
}

/// @test
/// Verifies fromU8string correctly moves a rvalue string
/// @requirements(SEN-576)
TEST(U8StringUtilTest, FromU8StringRvalueStdString)
{
  std::string input = "Temporary String \xE2\x9C\x93";
  const std::string expected = input;

  const std::string result = sen::std_util::fromU8string(std::move(input));

  EXPECT_EQ(result, expected);
}

/// @test
/// Verifies castToCharPtr accurately returns the exact same const char* pointer passed to it
/// @requirements(SEN-576)
TEST(U8StringUtilTest, CastToCharPtrFromConstChar)
{
  const auto input = "Standard char array";

  const char* result = sen::std_util::castToCharPtr(input);

  EXPECT_EQ(result, input);
  EXPECT_STREQ(result, "Standard char array");
}

#if defined(__cpp_lib_char8_t)
/// @test
/// Verifies fromU8string correctly constructs a string from an u8string preserving all byte values
/// @requirements(SEN-576)
TEST(U8StringUtilTest, FromU8StringStdU8String)
{
  const std::u8string input = u8"Native u8string \xF0\x9F\x9A\x80";

  std::string result = sen::std_util::fromU8string(input);

  ASSERT_EQ(result.size(), input.size());
  for (size_t i = 0; i < input.size(); ++i)
  {
    EXPECT_EQ(result[i], static_cast<char>(input[i]));
  }
}

#endif

#if defined(__cpp_lib_char8_t)
/// @test
/// Verifies castToCharPtr accurately casts a const char8_t* to const char* maintaining the correct memory address and
/// content
/// @requirements(SEN-576)
TEST(U8StringUtilTest, CastToCharPtrFromConstChar8)
{
  const char8_t* input = u8"Char8_t array";

  const char* result = sen::std_util::castToCharPtr(input);

  EXPECT_EQ(static_cast<const void*>(result), static_cast<const void*>(input));
  EXPECT_STREQ(result, reinterpret_cast<const char*>(u8"Char8_t array"));
}

#endif
