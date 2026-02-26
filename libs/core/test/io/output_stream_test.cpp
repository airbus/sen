// === output_stream_test.cpp ==========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "../support/reader_writer.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/detail/endianness.h"
#include "sen/core/io/output_stream.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <limits>
#include <string>

using sen::OutputStream;
using sen::test::TestWriter;
using sen::test::valueToBytes;

namespace
{

void checkBasicWrite(const uint8_t data)
{
  TestWriter writer;
  OutputStream out(writer);
  const auto valueAsBytes = valueToBytes(data);

  out.writeUInt8(data);

  EXPECT_EQ(valueAsBytes, writer.getBuffer());
  EXPECT_EQ(sizeof(uint8_t), writer.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicWrite(const int8_t data)
{
  TestWriter writer;
  OutputStream out(writer);
  const auto valueAsBytes = valueToBytes(data);

  out.writeInt8(data);

  EXPECT_EQ(valueAsBytes, writer.getBuffer());
  EXPECT_EQ(sizeof(int8_t), writer.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicWrite(const uint16_t data)
{
  TestWriter writer;
  OutputStream out(writer);
  const auto valueAsBytes = valueToBytes(data);

  out.writeUInt16(data);

  EXPECT_EQ(valueAsBytes, writer.getBuffer());
  EXPECT_EQ(sizeof(uint16_t), writer.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicWrite(const int16_t data)
{
  TestWriter writer;
  OutputStream out(writer);
  const auto valueAsBytes = valueToBytes(data);

  out.writeInt16(data);

  EXPECT_EQ(valueAsBytes, writer.getBuffer());
  EXPECT_EQ(sizeof(int16_t), writer.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicWrite(const uint32_t data)
{
  TestWriter writer;
  OutputStream out(writer);
  const auto valueAsBytes = valueToBytes(data);

  out.writeUInt32(data);

  EXPECT_EQ(valueAsBytes, writer.getBuffer());
  EXPECT_EQ(sizeof(uint32_t), writer.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicWrite(const int32_t data)
{
  TestWriter writer;
  OutputStream out(writer);
  const auto valueAsBytes = valueToBytes(data);

  out.writeInt32(data);

  EXPECT_EQ(valueAsBytes, writer.getBuffer());
  EXPECT_EQ(sizeof(int32_t), writer.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicWrite(const uint64_t data)
{
  TestWriter writer;
  OutputStream out(writer);
  const auto valueAsBytes = valueToBytes(data);

  out.writeUInt64(data);

  EXPECT_EQ(valueAsBytes, writer.getBuffer());
  EXPECT_EQ(sizeof(uint64_t), writer.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicWrite(const int64_t data)
{
  TestWriter writer;
  OutputStream out(writer);
  const auto valueAsBytes = valueToBytes(data);

  out.writeInt64(data);

  EXPECT_EQ(valueAsBytes, writer.getBuffer());
  EXPECT_EQ(sizeof(int64_t), writer.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicWrite(const float32_t data)
{
  TestWriter writer;
  OutputStream out(writer);
  const auto valueAsBytes = valueToBytes(data);

  out.writeFloat32(data);

  EXPECT_EQ(valueAsBytes, writer.getBuffer());
  EXPECT_EQ(sizeof(float32_t), writer.getBuffer().size() * sizeof(uint8_t));
}

void checkBasicWrite(const float64_t data)
{
  TestWriter writer;
  OutputStream out(writer);
  const auto valueAsBytes = valueToBytes(data);

  out.writeFloat64(data);

  EXPECT_EQ(valueAsBytes, writer.getBuffer());
  EXPECT_EQ(sizeof(float64_t), writer.getBuffer().size() * sizeof(uint8_t));
}

void checkChar(const char data)
{
  TestWriter writer;
  OutputStream out(writer);
  const auto valueAsBytes = valueToBytes(data);

  out.writeChar(data);

  EXPECT_EQ(valueAsBytes, writer.getBuffer());
  EXPECT_EQ(sizeof(char), writer.getBuffer().size() * sizeof(uint8_t));
}

void checkUchar(const unsigned char data)
{
  TestWriter writer;
  OutputStream out(writer);
  const auto valueAsBytes = valueToBytes(data);

  out.writeUChar(data);

  EXPECT_EQ(valueAsBytes, writer.getBuffer());
  EXPECT_EQ(sizeof(unsigned char), writer.getBuffer().size() * sizeof(uint8_t));
}

void checkString(const std::string data)
{
  TestWriter writer;
  OutputStream out(writer);
  const auto valueAsBytes = valueToBytes(data);

  out.writeString(data);

  EXPECT_FALSE(writer.getBuffer().empty());
}

void checkTimeStamp(const sen::TimeStamp data)
{
  TestWriter writer;
  OutputStream out(writer);
  const auto valueAsBytes = valueToBytes(data.sinceEpoch().getNanoseconds());

  out.writeTimestamp(data);

  EXPECT_EQ(valueAsBytes, writer.getBuffer());
  EXPECT_EQ(sizeof(int64_t), writer.getBuffer().size() * sizeof(uint8_t));
}

void checkBoolean(const bool data)
{
  TestWriter writer;
  OutputStream out(writer);
  const auto valueAsBytes = valueToBytes(data);

  out.writeBool(data);

  EXPECT_EQ(valueAsBytes, writer.getBuffer());
  EXPECT_EQ(sizeof(bool), writer.getBuffer().size() * sizeof(uint8_t));
}

template <typename T>
void checkWriteNumber()
{
  checkBasicWrite(T {0});
  checkBasicWrite(std::numeric_limits<T>::max());
  checkBasicWrite(std::numeric_limits<T>::min());
  checkBasicWrite(std::numeric_limits<T>::lowest());
}

}  // namespace

/// @test
/// Check write of basic types
/// @requirements(SEN-1051)
TEST(OutputStream, basics)
{
  checkBasicWrite(int8_t {-8});
  checkBasicWrite(uint8_t {2U});
  checkBasicWrite(int16_t {304});
  checkBasicWrite(uint16_t {298U});
  checkBasicWrite(int32_t {-1025});
  checkBasicWrite(uint32_t {1214U});
  checkBasicWrite(int64_t {23023});
  checkBasicWrite(uint64_t {0U});
  checkBasicWrite(float32_t {-3.14159});
  checkBasicWrite(float64_t {2.89});
}

/// @test
/// Check correct writing of basic types numeric limits
/// @requirements(SEN-1051)
TEST(OutputStream, numberLimits)
{
  checkWriteNumber<int8_t>();
  checkWriteNumber<uint8_t>();
  checkWriteNumber<int16_t>();
  checkWriteNumber<uint16_t>();
  checkWriteNumber<int32_t>();
  checkWriteNumber<uint32_t>();
  checkWriteNumber<uint64_t>();
  checkWriteNumber<int64_t>();
  checkWriteNumber<float32_t>();
  checkWriteNumber<float64_t>();
}

/// @test
/// Check write of chars
/// @requirements(SEN-1051)
TEST(OutputStream, chars)
{
  checkChar('c');
  checkChar('\0');
  checkChar('\n');
  checkChar(' ');
  checkChar('_');
  checkUchar('c');
  checkUchar('\0');
  checkUchar('\n');
  checkUchar(' ');
  checkUchar('_');
}

/// @test
/// Check write of strings
/// @requirements(SEN-1051)
TEST(OutputStream, string)
{
  checkString("hello world this is me");
  checkString({});
  checkString("hello \0 world");              // NOLINT(bugprone-string-literal-with-embedded-nul)
  checkString("UPPER CASE minor \0 case\n");  // NOLINT(bugprone-string-literal-with-embedded-nul)
  checkString("@#$$%^&*(){}!@#$$#@\\@");
}

/// @test
/// Check write of sen timestamps types
/// @requirements(SEN-1051)
TEST(OutputStream, timestamp)
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
/// Check write of bool types
/// @requirements(SEN-1051)
TEST(OutputStream, boolean)
{
  checkBoolean(true);
  checkBoolean(false);
}

/// @test
/// Check writing of some types using big endian
/// @requirements(SEN-1051)
TEST(OutputStream, BigEndian)
{
  // int16_t
  {
    int16_t data = 2131U;
    TestWriter writer;
    sen::OutputStreamTemplate<sen::BigEndian> out(writer);
    const auto valueAsBytes = valueToBytes(sen::impl::swapBytes(data));

    out.writeInt16(data);

    EXPECT_EQ(valueAsBytes, writer.getBuffer());
    EXPECT_EQ(sizeof(int16_t), writer.getBuffer().size() * sizeof(uint8_t));
  }

  // float32_t
  {
    float32_t data = 2131U;
    TestWriter writer;
    sen::OutputStreamTemplate<sen::BigEndian> out(writer);
    const auto valueAsBytes = valueToBytes(sen::impl::swapBytes(data));

    out.writeFloat32(data);

    EXPECT_EQ(valueAsBytes, writer.getBuffer());
    EXPECT_EQ(sizeof(float32_t), writer.getBuffer().size() * sizeof(uint8_t));
  }
}
