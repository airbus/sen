// === hash32_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/hash32.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/uuid.h"

// google test
#include <gtest/gtest.h>

// std
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <vector>

/// @test
/// Computes a CRC32 of zero for empty input, both an empty string_view and a "\0" literal whose effective
/// length is zero.
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
/// Hashes test and TEST to different CRC32 values, while an iterator range over the characters t, e, s, t
/// hashes identically to the string overload.
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
/// Produces distinct hashCombine results when the argument order changes, when the seed changes, and when a
/// string argument changes case.
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
/// Returns false from fileToCompressedArrayFile for a nonexistent input file and for an output path whose
/// directory cannot be written.
/// @requirements(SEN-576)
TEST(Hash32, file_creation_failures)
{
  const std::filesystem::path output = "debug.txt";
  const std::filesystem::path input = "does_not_exist.cpp";
  const auto result = sen::fileToCompressedArrayFile(input, "TEST", output);
  EXPECT_FALSE(result);

  const std::filesystem::path invalidOutput = "/_sen_system_invalid_/output.cpp";
  const auto result2 = sen::fileToCompressedArrayFile(ABS_FILE, "TEST", invalidOutput);
  EXPECT_FALSE(result2);
}

/// @test
/// Compresses files of 5, 1000, and 70000 bytes into C++ array source files, fileToCompressedArrayFile
/// succeeding at each size.
/// @requirements(SEN-576)
TEST(Hash32, compression_cycle_variations)
{
  auto runTest = [](const std::string& data)
  {
    const auto uuid = sen::UuidRandomGenerator()();
    const std::filesystem::path in = std::filesystem::temp_directory_path() / (uuid.toString() + ".in");
    const std::filesystem::path out = std::filesystem::temp_directory_path() / (uuid.toString() + ".cpp");

    {
      std::ofstream f(in, std::ios::binary);
      f << data;
    }

    EXPECT_TRUE(sen::fileToCompressedArrayFile(in, "TEST", out));

    std::filesystem::remove(in);
    std::filesystem::remove(out);
  };

  runTest("Short");
  runTest(std::string(1000, 'A'));
  runTest(std::string(70000, 'B'));
}

/// @test
/// Produces a nonzero hashCombine for i8, bool, float, double, pointer, and enum class values, plus
/// platformDependentHashCombine on a std::string.
/// @requirements(SEN-576)
TEST(Hash32, hash_type_coverage)
{
  EXPECT_NE(sen::hashCombine(0, static_cast<i8>(1)), 0);
  EXPECT_NE(sen::hashCombine(0, true), 0);
  EXPECT_NE(sen::hashCombine(0, 1.0f), 0);
  EXPECT_NE(sen::hashCombine(0, 1.0), 0);

  int x = 0;
  EXPECT_NE(sen::hashCombine(0, &x), 0);

  enum class TestEnum : u32
  {
    a,
    b
  };
  EXPECT_NE(sen::hashCombine(0, TestEnum::a), 0);

  EXPECT_NE(sen::platformDependentHashCombine(1, std::string("test")), 0);
}

/// @test
/// Rejects corrupted compressed blobs, decompressSymbol returning null for a wrong magic number, a zero
/// original size, and a failing Adler checksum, and decompressSymbolToString returning an empty string for the
/// wrong magic.
/// @requirements(SEN-576)
TEST(Hash32, decompression_validation)
{
  const unsigned char badMagic[16] = {};
  EXPECT_EQ(sen::decompressSymbol(badMagic), nullptr);
  EXPECT_TRUE(sen::decompressSymbolToString(badMagic, 0).empty());

  const unsigned char zeroSize[16] = {
    0x57, 0xbc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  EXPECT_EQ(sen::decompressSymbol(zeroSize), nullptr);

  // Test decompression failure when the Adler checksum at the end of the stream is incorrect
  const unsigned char badAdler[] = {0x57, 0xbc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
                                    0x00, 0x04, 0x00, 0x00, 0x20, 0x41, 0x05, 0xfa, 0x00, 0x00, 0x00, 0x02};
  EXPECT_EQ(sen::decompressSymbol(badAdler), nullptr);
}

/// @test
/// Compresses a 70000 byte file of bytes cycling through values 0 to 127, fileToCompressedArrayFile
/// succeeding.
/// @requirements(SEN-576)
TEST(Hash32, large_literal_block)
{
  const auto uuid = sen::UuidRandomGenerator()();
  const std::filesystem::path in = std::filesystem::temp_directory_path() / (uuid.toString() + ".in");
  const std::filesystem::path out = std::filesystem::temp_directory_path() / (uuid.toString() + ".out");

  {
    std::ofstream f(in, std::ios::binary);
    std::string largeData;
    for (int i = 0; i < 70000; ++i)
    {
      largeData += static_cast<char>(i % 128);
    }
    f << largeData;
  }

  EXPECT_TRUE(sen::fileToCompressedArrayFile(in, "LARGE_LIT", out));

  std::filesystem::remove(in);
  std::filesystem::remove(out);
}
