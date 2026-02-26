// === hash32_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/hash32.h"
#include "sen/core/base/uuid.h"

// google test
#include <gtest/gtest.h>

// std
#include <filesystem>
#include <string_view>
#include <vector>

/// @test
/// Check crc32 using empty string and null char
/// @requirements(SEN-576)
TEST(Hash32, empty_string)
{
  const std::string_view str;

  const auto result = sen::crc32(str);
  EXPECT_EQ(result, 0);

  const auto result2 = sen::crc32("\0");
  EXPECT_EQ(result2, 0);
}

/// @test
/// Check basic equivalency between strings for the crc32
/// @requirements(SEN-576)
TEST(Hash32, basic_equivalency)
{
  const std::string_view inputStr = "test";
  const std::string_view inputStr2 = "TEST";
  std::vector inputVec {'t', 'e', 's', 't'};

  const auto result = sen::crc32(inputStr);
  const auto result2 = sen::crc32(inputStr2);
  const auto result3 = sen::crc32(inputVec.begin(), inputVec.end());

  EXPECT_NE(result, result2);
  EXPECT_EQ(result, result3);
}

/// @test
/// Check hash combine function varying upper/lower case in input strings and hash seed
/// @requirements(SEN-576)
TEST(Hash32, hash_combine)
{
  const std::string_view inputStr = "basicTestString";
  const std::string_view inputStr2 = "anotherTestString";
  const std::string_view inputStr3 = "ANOTHERTESTSTRING";

  const auto result1 = sen::hashCombine(5, inputStr, inputStr2);
  const auto result2 = sen::hashCombine(5, inputStr2, inputStr);
  const auto result3 = sen::hashCombine(7, inputStr, inputStr2);
  const auto result4 = sen::hashCombine(7, inputStr, inputStr3);

  EXPECT_NE(result1, result2);
  EXPECT_NE(result1, result3);
  EXPECT_NE(result3, result4);
}

/// @test
/// Check failure in file creation with non existing file
/// @requirements(SEN-576)
TEST(Hash32, file_creation_fails)
{
  const std::filesystem::path output = "debug.txt";
  const std::filesystem::path input = "does_not_exist.cpp";
  constexpr std::string_view symbol;

  const auto result = sen::fileToCompressedArrayFile(input, symbol, output);
  EXPECT_FALSE(result);
}

/// @test
/// Check successful file creation with an existing input file
/// @requirements(SEN-576)
TEST(Hash32, file_creation_ok)
{
  auto uuid = sen::UuidRandomGenerator()();

  const std::filesystem::path output = std::filesystem::temp_directory_path() / uuid.toString();

  const std::filesystem::path input = ABS_FILE;
  const std::string_view symbol = "TEST";
  const std::string_view unrealSymbol = "hello world";

  const auto result = sen::fileToCompressedArrayFile(input, symbol, output);
  EXPECT_TRUE(result);

  const auto result2 = sen::fileToCompressedArrayFile(input, unrealSymbol, output);
  EXPECT_TRUE(result2);
}
