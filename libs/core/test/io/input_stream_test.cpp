// === input_stream_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "../support/reader_writer.h"

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/detail/endianness.h"
#include "sen/core/io/detail/serialization_traits.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>
#include <tuple>
#include <vector>

using sen::InputStream;
using sen::test::BufferedTestReader;

namespace
{

class TestInputStream: public sen::InputStreamBase
{
public:
  using InputStreamBase::InputStreamBase;
  using InputStreamBase::reverse;
};

template <typename T>
void copyIntoBufferAsBytes(BufferedTestReader& reader, const T data)
{
  if constexpr (std::is_same_v<T, std::string>)
  {
    sen::test::TestWriter writer;
    sen::OutputStream out(writer);
    out.writeString(data);

    reader.getBuffer() = writer.getBuffer();
  }
  else
  {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    const auto dataPtr = reinterpret_cast<const uint8_t*>(&data);  // NOSONAR
    for (std::size_t i = 0U; i < sizeof(T); ++i)
    {
      reader.getBuffer().push_back(*std::next(dataPtr, sen::std_util::checkedConversion<std::ptrdiff_t>(i)));
    }
  }
}

void copyIntoBufferAsBytes(BufferedTestReader& reader, const sen::Duration& data)
{
  copyIntoBufferAsBytes(reader, data.getNanoseconds());
}

void copyIntoBufferAsBytesT(BufferedTestReader& reader, const sen::TimeStamp& data)
{
  const auto duration = data.sinceEpoch();
  copyIntoBufferAsBytes(reader, duration);
}

void checkBasicRead(const uint8_t data)
{
  BufferedTestReader reader;
  copyIntoBufferAsBytes(reader, data);

  InputStream in(reader.getBuffer());

  uint8_t val {};
  in.readUInt8(val);
  EXPECT_EQ(val, data);
  EXPECT_EQ(sizeof(uint8_t), reader.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicRead(const int8_t data)
{
  BufferedTestReader reader;
  copyIntoBufferAsBytes(reader, data);

  InputStream in(reader.getBuffer());

  int8_t val {};
  in.readInt8(val);
  EXPECT_EQ(val, data);
  EXPECT_EQ(sizeof(int8_t), reader.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicRead(const uint16_t data)
{
  BufferedTestReader reader;
  copyIntoBufferAsBytes(reader, data);

  InputStream in(reader.getBuffer());

  uint16_t val {};
  in.readUInt16(val);
  EXPECT_EQ(val, data);
  EXPECT_EQ(sizeof(uint16_t), reader.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicRead(const int16_t data)
{
  BufferedTestReader reader;
  copyIntoBufferAsBytes(reader, data);

  InputStream in(reader.getBuffer());

  int16_t val {};
  in.readInt16(val);
  EXPECT_EQ(val, data);
  EXPECT_EQ(sizeof(int16_t), reader.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicRead(const uint32_t data)
{
  BufferedTestReader reader;
  copyIntoBufferAsBytes(reader, data);

  InputStream in(reader.getBuffer());

  uint32_t val {};
  in.readUInt32(val);
  EXPECT_EQ(val, data);
  EXPECT_EQ(sizeof(uint32_t), reader.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicRead(const int32_t data)
{
  BufferedTestReader reader;
  copyIntoBufferAsBytes(reader, data);

  InputStream in(reader.getBuffer());

  int32_t val {};
  in.readInt32(val);
  EXPECT_EQ(val, data);
  EXPECT_EQ(sizeof(int32_t), reader.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicRead(const uint64_t data)
{
  BufferedTestReader reader;
  copyIntoBufferAsBytes(reader, data);

  InputStream in(reader.getBuffer());

  uint64_t val {};
  in.readUInt64(val);
  EXPECT_EQ(val, data);
  EXPECT_EQ(sizeof(uint64_t), reader.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicRead(const int64_t data)
{
  BufferedTestReader reader;
  copyIntoBufferAsBytes(reader, data);

  InputStream in(reader.getBuffer());

  int64_t val {};
  in.readInt64(val);
  EXPECT_EQ(val, data);
  EXPECT_EQ(sizeof(int64_t), reader.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicRead(const float32_t data)
{
  BufferedTestReader reader;
  copyIntoBufferAsBytes(reader, data);

  InputStream in(reader.getBuffer());

  float32_t val {};
  in.readFloat32(val);
  EXPECT_EQ(val, data);
  EXPECT_EQ(sizeof(float32_t), reader.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicRead(const float64_t data)
{
  BufferedTestReader reader;
  copyIntoBufferAsBytes(reader, data);

  InputStream in(reader.getBuffer());

  float64_t val {};
  in.readFloat64(val);
  EXPECT_EQ(val, data);
  EXPECT_EQ(sizeof(float64_t), reader.getBuffer().size() * sizeof(uint8_t));
}

void checkString(const std::string& data)
{
  std::string val {};

  BufferedTestReader reader;
  copyIntoBufferAsBytes(reader, data);

  InputStream in(reader.getBuffer());
  in.readString(val);

  EXPECT_EQ(data, val);
}

void checkTimeStamp(const sen::TimeStamp data)
{
  BufferedTestReader reader;
  copyIntoBufferAsBytesT(reader, data);

  InputStream in(reader.getBuffer());
  sen::TimeStamp val;
  in.readTimeStamp(val);

  EXPECT_EQ(data, val);
  EXPECT_EQ(sizeof(int64_t), reader.getBuffer().size() * sizeof(uint8_t));
}

void checkBoolean(const bool data)
{
  BufferedTestReader reader;
  copyIntoBufferAsBytes(reader, sen::std_util::checkedConversion<sen::impl::BoolTransportType>(data ? 1U : 0U));

  InputStream in(reader.getBuffer());
  bool val = !data;
  in.readBool(val);

  EXPECT_EQ(data, val);
  EXPECT_EQ(sizeof(bool), reader.getBuffer().size() * sizeof(uint8_t));
}

template <typename T>
void checkReadNumber()
{
  checkBasicRead(T {0});
  checkBasicRead(std::numeric_limits<T>::max());
  checkBasicRead(std::numeric_limits<T>::min());
  checkBasicRead(std::numeric_limits<T>::lowest());
}

}  // namespace

/// @test
/// Check reading of basic types
/// @requirements(SEN-1051)
TEST(InputStream, basics)
{
  checkBasicRead(int8_t {-8});
  checkBasicRead(uint8_t {2U});
  checkBasicRead(int16_t {304});
  checkBasicRead(uint16_t {298U});
  checkBasicRead(int32_t {-1025});
  checkBasicRead(uint32_t {1214U});
  checkBasicRead(int64_t {23023});
  checkBasicRead(uint64_t {0U});
  checkBasicRead(float32_t {-3.14159});
  checkBasicRead(float64_t {2.89});
}

/// @test
/// Check correct reading of basic types numeric limits
/// @requirements(SEN-1051)
TEST(InputStream, numberLimits)
{
  checkReadNumber<int8_t>();
  checkReadNumber<uint8_t>();
  checkReadNumber<int16_t>();
  checkReadNumber<uint16_t>();
  checkReadNumber<int32_t>();
  checkReadNumber<uint32_t>();
  checkReadNumber<uint64_t>();
  checkReadNumber<int64_t>();
  checkReadNumber<float32_t>();
  checkReadNumber<float64_t>();
}

/// @test
/// Check reading of string types
/// @requirements(SEN-1051)
TEST(InputStream, string)
{
  checkString("hello world this is me");
  checkString({});
  checkString("hello \0 world");              // NOLINT(bugprone-string-literal-with-embedded-nul)
  checkString("UPPER CASE minor \0 case\n");  // NOLINT(bugprone-string-literal-with-embedded-nul)
  checkString("@#$$%^&*(){}!@#$$#@\\@");
}

/// @test
/// Check reading of timestamps types
/// @requirements(SEN-1051)
TEST(InputStream, timestamp)
{
  checkTimeStamp({});
  checkTimeStamp(sen::TimeStamp {sen::Duration {10}});
  checkTimeStamp(sen::TimeStamp {sen::Duration {-10}});
  checkTimeStamp(sen::TimeStamp {sen::Duration {10000000}});
  checkTimeStamp(sen::TimeStamp {sen::Duration {-10000000}});
  checkTimeStamp(sen::TimeStamp {sen::Duration(99999)});
  checkTimeStamp(sen::TimeStamp {sen::Duration({})});
  checkTimeStamp(sen::TimeStamp {sen::Duration(0.223)});
  checkTimeStamp(sen::TimeStamp {std::numeric_limits<sen::Duration::ValueType>::max()});
  checkTimeStamp(sen::TimeStamp {std::numeric_limits<sen::Duration::ValueType>::min()});
  checkTimeStamp(sen::TimeStamp {std::numeric_limits<sen::Duration::ValueType>::lowest()});
}

/// @test
/// Check reading of boolean types
/// @requirements(SEN-1051)
TEST(InputStream, boolean)
{
  checkBoolean(true);
  checkBoolean(false);
}

/// @test
/// Check reading of some types using big endian
/// @requirements(SEN-1051)
TEST(InputStream, BigEndian)
{
  // uint32_t
  {
    constexpr uint32_t data = 93982U;
    BufferedTestReader reader;
    copyIntoBufferAsBytes(reader, data);

    sen::InputStreamTemplate<sen::BigEndian> in(reader.getBuffer());

    uint32_t val {};
    in.readUInt32(val);
    EXPECT_EQ(sen::impl::swapBytes(val), data);
    EXPECT_EQ(sizeof(uint32_t), reader.getBuffer().size() * sizeof(uint8_t));
  }

  // float64_t
  {
    constexpr float64_t data = 6533.32;
    BufferedTestReader reader;
    copyIntoBufferAsBytes(reader, data);

    sen::InputStreamTemplate<sen::BigEndian> in(reader.getBuffer());

    float64_t val {};
    in.readFloat64(val);
    EXPECT_EQ(sen::impl::swapBytes(val), data);
    EXPECT_EQ(sizeof(float64_t), reader.getBuffer().size() * sizeof(uint8_t));
  }
}

/// @test
/// Check positioning logic and boundary manipulation
/// @requirements(SEN-1051)
TEST(InputStream, positionAndReverse)
{
  const std::vector<uint8_t> buff = {10, 20, 30, 40};
  TestInputStream in(sen::makeConstSpan<uint8_t>(buff));

  EXPECT_EQ(in.getPosition(), 0U);

  in.setPosition(2U);
  EXPECT_EQ(in.getPosition(), 2U);
  EXPECT_FALSE(in.atEnd());

  const auto* ptr = in.advance(1U);
  EXPECT_EQ(*ptr, 30);
  EXPECT_EQ(in.getPosition(), 3U);

  in.reverse(2U);
  EXPECT_EQ(in.getPosition(), 1U);

  in.setPosition(4U);
  EXPECT_EQ(in.getPosition(), 4U);
  EXPECT_TRUE(in.atEnd());
}

/// @test
/// Check exception trigger on buffer underflow via advance
/// @requirements(SEN-1051)
TEST(InputStream, advanceUnderflow)
{
  const std::vector<uint8_t> buff = {10, 20};
  sen::InputStreamBase in(sen::makeConstSpan<uint8_t>(buff));

  EXPECT_NO_THROW(std::ignore = in.advance(1U));
  EXPECT_ANY_THROW(std::ignore = in.advance(5U));
}

/// @test
/// Check tryAdvance logic including partial advances and exceptions
/// @requirements(SEN-1051)
TEST(InputStream, tryAdvance)
{
  const std::vector<uint8_t> buff = {10, 20, 30};
  sen::InputStreamBase in(sen::makeConstSpan<uint8_t>(buff));

  const auto result1 = in.tryAdvance(0U);
  EXPECT_EQ(result1.second, 0U);
  EXPECT_EQ(in.getPosition(), 0U);

  const auto result2 = in.tryAdvance(2U);
  EXPECT_EQ(result2.second, 2U);
  EXPECT_EQ(*result2.first, 10);
  EXPECT_EQ(*std::next(result2.first), 20);
  EXPECT_EQ(in.getPosition(), 2U);

  const auto result3 = in.tryAdvance(5U);
  EXPECT_EQ(result3.second, 1U);
  EXPECT_EQ(*result3.first, 30);
  EXPECT_EQ(in.getPosition(), 3U);
  EXPECT_TRUE(in.atEnd());

  EXPECT_ANY_THROW(std::ignore = in.tryAdvance(1U));
}
