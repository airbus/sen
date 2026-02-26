// === utils_io_test.cpp ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "../support/reader_writer.h"

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/base/quantity.h"
#include "sen/core/base/span.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/io/util.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/var.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

SEN_RANGED_QUANTITY(MyQuantity, float32_t, -15.0f, 15.0f)

struct TrivialType
{
  int32_t x;
  int32_t y;
};

inline bool operator==(const TrivialType& lhs, const TrivialType& rhs) { return (lhs.x == rhs.x && lhs.y == rhs.y); }

enum Level : uint8_t
{
  low = 0,
  mid = 1,
  high = 2
};

enum PhonePrefixes : int64_t
{
  usa = 1,
  france = 33,
  spain = 34,
  germany = 49
};

/// @test
/// Check struct fields insertion and extraction to/from sen var maps
/// @requirements(SEN-1053)
TEST(IOUtils, Structs)
{
  constexpr TrivialType st {132, 332};
  TrivialType st2 {};
  sen::VarMap map;

  EXPECT_TRUE(map.empty());
  sen::impl::structFieldToVarMap("xAxis", st.x, map);
  EXPECT_FALSE(map.empty());

  // check map values;
  for (const auto& [key, var]: map)
  {
    EXPECT_EQ(key, "xAxis");
    EXPECT_EQ(var, st.x);
  }

  sen::impl::extractStructFieldFromMap("xAxis", st2.x, &map);
  EXPECT_EQ(st.x, st2.x);
  EXPECT_NE(st.y, st2.y);

  sen::impl::structFieldToVarMap("yAxis", st.y, map);
  sen::impl::extractStructFieldFromMap("yAxis", st2.y, &map);
  EXPECT_EQ(st.y, st2.y);
}

/// @test
/// Check string split method works ok
/// @requirements(SEN-576)
TEST(IOUtils, splitString)
{
  // case delimiter found
  {
    const std::string string = "hello world this is me";
    std::vector<std::string> expectedVec = {"hello", "world", "this", "is", "me"};

    const auto vec = sen::impl::split(string, ' ');
    EXPECT_EQ(vec.size(), 5);
    EXPECT_EQ(vec, expectedVec);
  }

  // case no delimiter found
  {
    const std::string string = "this is an example string test";
    std::vector<std::string> expectedVec = {"this is an example string test"};

    const auto vec = sen::impl::split(string, 'Y');
    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(vec, expectedVec);
  }

  // case sensitive
  {
    const std::string string = "ZzeezzZzezZ";
    std::vector<std::string> expectedVec = {"", "zeezz", "zez", ""};

    const auto vec = sen::impl::split(string, 'Z');
    EXPECT_EQ(vec.size(), 4);
    EXPECT_EQ(vec, expectedVec);
  }

  // case null char
  {
    const std::string string = "hello world";
    std::vector<std::string> expectedVec = {"hello world\0\0"};  // NOLINT(bugprone-string-literal-with-embedded-nul)

    const auto vec = sen::impl::split(string, '\0');
    EXPECT_EQ(vec.size(), 1);
    EXPECT_EQ(vec, expectedVec);
  }
}

/// @test
/// Check sequence conversion to/from sen variant
/// @requirements(SEN-1053)
TEST(IOUtils, SequenceToFromVariant)
{
  const std::vector<float32_t> vec {10.2f, 11.1f};
  sen::Var myVar;

  sen::impl::sequenceToVariant(vec, myVar);
  std::vector<float32_t> vec2;
  sen::impl::variantToSequence(myVar, vec2);

  EXPECT_EQ(vec[0], vec2[0]);
  EXPECT_EQ(vec[1], vec2[1]);
}

/// @test
/// Check array conversion to/from sen variant
/// @requirements(SEN-1053)
TEST(IOUtils, Array)
{
  const std::vector<float32_t> vec1 {1.2, 3.4, 5.6, 7.8};
  sen::test::TestWriter writer;
  sen::OutputStream out(writer);

  // write data to out buffer
  sen::impl::writeArray(out, vec1);
  EXPECT_FALSE(writer.getBuffer().empty());

  // create reader with writer's buffer
  auto buff = writer.getBuffer();
  sen::test::TestReader reader(buff);
  sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));

  // read vector from in buffer
  std::vector<float32_t> vec2;
  vec2.resize(4);

  sen::impl::readArray(in, vec2);
  EXPECT_EQ(vec1, vec2);
}

/// @test
/// Check reading/writing of sequences
/// @requirements(SEN-579)
TEST(IOUtils, Sequence)
{
  const std::vector<uint32_t> vec1 {1, 2, 3};
  sen::test::TestWriter writer;
  sen::OutputStream out(writer);

  // write data to out buffer
  sen::impl::writeSequence(out, vec1);
  EXPECT_FALSE(writer.getBuffer().empty());

  // create reader with writer's buffer
  auto buff = writer.getBuffer();
  sen::test::TestReader reader(buff);
  sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));

  // read vector from in buffer
  std::vector<uint32_t> vec2;
  sen::impl::readSequence(in, vec2);
  EXPECT_EQ(vec1, vec2);
}

/// @test
/// Check reading/writing of multiple sequences
/// @requirements(SEN-579)
TEST(IOUtils, MultipleSequences)
{
  const std::vector<float32_t> vec1 {5.5, 6.6, 7.7, 8.8, 9.9};
  const std::vector<float32_t> vec2 {10.5, 11.4, 12.3, 13.2, 14.1};
  const std::vector<float32_t> vec3 {-0.324f, -0.842, 0.99};
  const std::vector<float32_t> vec4 {3.3f, -3.14};
  const std::vector vecs {vec1, vec2, vec3, vec4};
  sen::test::TestWriter writer;
  sen::OutputStream out(writer);

  // write data to out buffer
  for (const auto& vec: vecs)
  {
    sen::impl::writeSequence(out, vec);
  }

  // create reader with writer's buffer
  auto buff = writer.getBuffer();
  sen::test::TestReader reader(buff);
  sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));

  // read vector from in buffer
  for (const auto& vec: vecs)
  {
    std::vector<float32_t> expectedVec;
    sen::impl::readSequence(in, expectedVec);
    EXPECT_EQ(vec, expectedVec);
  }
}

/// @test
/// Check reading/writing of quantities
/// @requirements(SEN-1054)
TEST(IOUtils, Quantities)
{
  MyQuantity quantity = 4.0f;
  sen::test::TestWriter writer;
  sen::OutputStream out(writer);

  // write data to out buffer
  sen::impl::writeQuantity(out, quantity);
  EXPECT_FALSE(writer.getBuffer().empty());

  // create reader with writer's buffer
  auto buff = writer.getBuffer();
  sen::test::TestReader reader(buff);
  sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));

  // read vector from in buffer
  MyQuantity expectedQuantity;
  sen::impl::readQuantity(in, expectedQuantity);
  EXPECT_EQ(quantity, expectedQuantity);
}

/// @test
/// Check reading/writing of enums
/// @requirements(SEN-902)
TEST(IOUtils, Enums)
{
  {
    constexpr Level difficulty = mid;
    sen::test::TestWriter writer;
    sen::OutputStream out(writer);

    // write data to out buffer
    sen::impl::writeEnum(out, difficulty);
    EXPECT_FALSE(writer.getBuffer().empty());

    // create reader with writer's buffer
    auto buff = writer.getBuffer();
    sen::test::TestReader reader(buff);
    sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));

    // read vector from in buffer
    Level expectedDifficulty;
    sen::impl::readEnum(in, expectedDifficulty);
    EXPECT_EQ(difficulty, expectedDifficulty);
  }

  {
    constexpr PhonePrefixes prefix = spain;
    sen::test::TestWriter writer;
    sen::OutputStream out(writer);

    // write data to out buffer
    sen::impl::writeEnum(out, prefix);
    EXPECT_FALSE(writer.getBuffer().empty());

    // create reader with writer's buffer
    auto buff = writer.getBuffer();
    sen::test::TestReader reader(buff);
    sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));

    // read vector from in buffer
    PhonePrefixes expectedPhonePrefix;
    sen::impl::readEnum(in, expectedPhonePrefix);
    EXPECT_EQ(prefix, expectedPhonePrefix);
  }
}

/// @test
/// Check reading/writing of variants
/// @requirements(SEN-1053)
TEST(IOUtils, Variants)
{
  uint32_t variantValue = 60U;
  std::variant<std::string, uint32_t, float64_t> variant = variantValue;
  sen::test::TestWriter writer;
  sen::OutputStream out(writer);

  // write data to out buffer
  ::sen::SerializationTraits<uint32_t>::write(out, variantValue);
  EXPECT_FALSE(writer.getBuffer().empty());

  // create reader with writer's buffer
  auto buff = writer.getBuffer();
  sen::test::TestReader reader(buff);
  sen::InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));

  // read vector from in buffer
  uint32_t expectedVariantValue;
  sen::impl::readVariantField<uint32_t>(in, expectedVariantValue);
  EXPECT_EQ(variantValue, expectedVariantValue);
}
