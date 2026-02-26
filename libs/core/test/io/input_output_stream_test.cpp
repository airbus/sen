// === input_output_stream_test.cpp ====================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "../support/reader_writer.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/detail/endianness.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <limits>
#include <string>

using sen::InputStream;
using sen::OutputStream;
using sen::test::TestReader;
using sen::test::TestWriter;

namespace
{

void checkWriteRead(const uint8_t valueToWrite)
{
  uint8_t readValue {};

  // writes value in output stream buffer
  TestWriter writer;
  OutputStream out(writer);
  out.writeUInt8(valueToWrite);

  // create reader using writer's buffer
  auto buff = writer.getBuffer();
  TestReader reader(buff);

  // read value from input stream buffer
  InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  in.readUInt8(readValue);

  // check read value and original writen value are the same
  EXPECT_EQ(valueToWrite, readValue);
}

void checkWriteRead(const int8_t valueToWrite)
{
  int8_t readValue {};

  // writes value in output stream buffer
  TestWriter writer;
  OutputStream out(writer);
  out.writeUInt8(valueToWrite);

  // create reader using writer's buffer
  auto buff = writer.getBuffer();
  TestReader reader(buff);

  // read value from input stream buffer
  InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  in.readInt8(readValue);

  // check read value and original writen value are the same
  EXPECT_EQ(valueToWrite, readValue);
}

void checkWriteRead(const uint16_t valueToWrite)
{
  uint16_t readValue {};

  // writes value in output stream buffer
  TestWriter writer;
  OutputStream out(writer);
  out.writeUInt16(valueToWrite);

  // create reader using writer's buffer
  auto buff = writer.getBuffer();
  TestReader reader(buff);

  // read value from input stream buffer
  InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  in.readUInt16(readValue);

  // check read value and original writen value are the same
  EXPECT_EQ(valueToWrite, readValue);
}

void checkWriteRead(const int16_t valueToWrite)
{
  int16_t readValue {};

  // writes value in output stream buffer
  TestWriter writer;
  OutputStream out(writer);
  out.writeInt16(valueToWrite);

  // create reader using writer's buffer
  auto buff = writer.getBuffer();
  TestReader reader(buff);

  // read value from input stream buffer
  InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  in.readInt16(readValue);

  // check read value and original writen value are the same
  EXPECT_EQ(valueToWrite, readValue);
}

void checkWriteRead(const uint32_t valueToWrite)
{
  uint32_t readValue {};

  // writes value in output stream buffer
  TestWriter writer;
  OutputStream out(writer);
  out.writeUInt32(valueToWrite);

  // create reader using writer's buffer
  auto buff = writer.getBuffer();
  TestReader reader(buff);

  // read value from input stream buffer
  InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  in.readUInt32(readValue);

  // check read value and original writen value are the same
  EXPECT_EQ(valueToWrite, readValue);
}

void checkWriteRead(const int32_t valueToWrite)
{
  int32_t readValue {};

  // writes value in output stream buffer
  TestWriter writer;
  OutputStream out(writer);
  out.writeInt32(valueToWrite);

  // create reader using writer's buffer
  auto buff = writer.getBuffer();
  TestReader reader(buff);

  // read value from input stream buffer
  InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  in.readInt32(readValue);

  // check read value and original writen value are the same
  EXPECT_EQ(valueToWrite, readValue);
}

void checkWriteRead(const int64_t valueToWrite)
{
  int64_t readValue {};

  // writes value in output stream buffer
  TestWriter writer;
  OutputStream out(writer);
  out.writeInt64(valueToWrite);

  // create reader using writer's buffer
  auto buff = writer.getBuffer();
  TestReader reader(buff);

  // read value from input stream buffer
  InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  in.readInt64(readValue);

  // check read value and original writen value are the same
  EXPECT_EQ(valueToWrite, readValue);
}

void checkWriteRead(const uint64_t valueToWrite)
{
  uint64_t readValue {};

  // writes value in output stream buffer
  TestWriter writer;
  OutputStream out(writer);
  out.writeUInt64(valueToWrite);

  // create reader using writer's buffer
  auto buff = writer.getBuffer();
  TestReader reader(buff);

  // read value from input stream buffer
  InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  in.readUInt64(readValue);

  // check read value and original writen value are the same
  EXPECT_EQ(valueToWrite, readValue);
}

void checkWriteRead(const float32_t valueToWrite)
{
  float32_t readValue {};

  // writes value in output stream buffer
  TestWriter writer;
  OutputStream out(writer);
  out.writeFloat32(valueToWrite);

  // create reader using writer's buffer
  auto buff = writer.getBuffer();
  TestReader reader(buff);

  // read value from input stream buffer
  InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  in.readFloat32(readValue);

  // check read value and original writen value are the same
  EXPECT_EQ(valueToWrite, readValue);
}

void checkWriteRead(const float64_t valueToWrite)
{
  float64_t readValue {};

  // writes value in output stream buffer
  TestWriter writer;
  OutputStream out(writer);
  out.writeFloat64(valueToWrite);

  // create reader using writer's buffer
  auto buff = writer.getBuffer();
  TestReader reader(buff);

  // read value from input stream buffer
  InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  in.readFloat64(readValue);

  // check read value and original writen value are the same
  EXPECT_EQ(valueToWrite, readValue);
}

void checkWriteReadString(const std::string& valueToWrite)
{
  std::string readValue {};

  // writes value in output stream buffer
  TestWriter writer;
  OutputStream out(writer);
  out.writeString(valueToWrite);

  // create reader using writer's buffer
  auto buff = writer.getBuffer();
  TestReader reader(buff);

  // read value from input stream buffer
  InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  in.readString(readValue);

  // check read value and original writen value are the same
  EXPECT_EQ(valueToWrite, readValue);
}

void checkWriteRead(const bool valueToWrite)
{
  bool readValue {};

  // writes value in output stream buffer
  TestWriter writer;
  OutputStream out(writer);
  out.writeBool(valueToWrite);

  // create reader using writer's buffer
  auto buff = writer.getBuffer();
  TestReader reader(buff);

  // read value from input stream buffer
  InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  in.readBool(readValue);

  // check read value and original writen value are the same
  EXPECT_EQ(valueToWrite, readValue);
}

void checkWriteReadTimeStamp(const sen::TimeStamp valueToWrite)
{
  sen::TimeStamp readValue {};

  // writes value in output stream buffer
  TestWriter writer;
  OutputStream out(writer);
  out.writeTimestamp(valueToWrite);

  // create reader using writer's buffer
  auto buff = writer.getBuffer();
  TestReader reader(buff);

  // read value from input stream buffer
  InputStream in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
  in.readTimeStamp(readValue);

  // check read value and original writen value are the same
  EXPECT_EQ(valueToWrite, readValue);
}

template <typename T>
void checkWriteReadNumber()
{
  checkWriteRead(0);
  checkWriteRead(std::numeric_limits<T>::max());
  checkWriteRead(std::numeric_limits<T>::min());
  checkWriteRead(std::numeric_limits<T>::lowest());
}

}  // namespace

/// @test
/// Check correct writing and reading of basic types
/// @requirements(SEN-1051)
TEST(InputOutputStream, numbers)
{
  checkWriteRead(int8_t {-8});
  checkWriteRead(uint8_t {2U});
  checkWriteRead(int16_t {304});
  checkWriteRead(uint16_t {298U});
  checkWriteRead(int32_t {-1025});
  checkWriteRead(uint32_t {1214U});
  checkWriteRead(int64_t {23023});
  checkWriteRead(uint64_t {0U});
  checkWriteRead(float32_t {-3.14159});
  checkWriteRead(float64_t {2.89});
}

/// @test
/// Check correct writing and reading of basic types numeric limits
/// @requirements(SEN-1051)
TEST(InputOutputStream, numberLimits)
{
  checkWriteReadNumber<int8_t>();
  checkWriteReadNumber<uint8_t>();
  checkWriteReadNumber<int16_t>();
  checkWriteReadNumber<uint16_t>();
  checkWriteReadNumber<int32_t>();
  checkWriteReadNumber<uint32_t>();
  checkWriteReadNumber<uint64_t>();
  checkWriteReadNumber<int64_t>();
  checkWriteReadNumber<float32_t>();
  checkWriteReadNumber<float64_t>();
}

/// @test
/// Check correct writing and reading of string types
/// @requirements(SEN-1051)
TEST(InputOutputStream, string)
{
  checkWriteReadString("hello world, this is me");
  checkWriteReadString(std::string({}));
  checkWriteReadString("");
  checkWriteReadString("hello \0 world");              // NOLINT(bugprone-string-literal-with-embedded-nul)
  checkWriteReadString("UPPER CASE minor \0 case\n");  // NOLINT(bugprone-string-literal-with-embedded-nul)
  checkWriteReadString("@#$$%^&*(){}!@#$$#@\\@");
}

/// @test
/// Check correct writing and reading of booleans
/// @requirements(SEN-1051)
TEST(InputOutputStream, boolean)
{
  checkWriteRead(false);
  checkWriteRead(true);
}

/// @test
/// Check correct writing and reading of sen timestamps types
/// @requirements(SEN-1051)
TEST(InputOutputStream, timestamps)
{
  checkWriteReadTimeStamp(sen::TimeStamp {sen::Duration(0)});
  checkWriteReadTimeStamp(sen::TimeStamp {sen::Duration(-1000)});
  checkWriteReadTimeStamp(sen::TimeStamp {sen::Duration(10000)});
  checkWriteReadTimeStamp(sen::TimeStamp {sen::Duration(99999)});
  checkWriteReadTimeStamp(sen::TimeStamp {sen::Duration({})});
  checkWriteReadTimeStamp(sen::TimeStamp {sen::Duration(0.223)});
  checkWriteReadTimeStamp(sen::TimeStamp {std::numeric_limits<sen::Duration::ValueType>::max()});
  checkWriteReadTimeStamp(sen::TimeStamp {std::numeric_limits<sen::Duration::ValueType>::min()});
  checkWriteReadTimeStamp(sen::TimeStamp {std::numeric_limits<sen::Duration::ValueType>::lowest()});
}

/// @test
/// Check correct writing and reading of some types in big endian
/// @requirements(SEN-1051)
TEST(InputOutputStream, bigEndian)
{
  // uint8_t
  {
    uint8_t valueToWrite = 32U;
    uint8_t readValue {};

    // writes value in output stream buffer
    TestWriter writer;
    OutputStream out(writer);
    out.writeUInt8(valueToWrite);

    // create reader using writer's buffer
    auto buff = writer.getBuffer();
    TestReader reader(buff);

    // read value from input stream buffer
    sen::InputStreamTemplate<sen::BigEndian> in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
    in.readUInt8(readValue);

    // check read value and original writen value are the same
    EXPECT_EQ(sen::impl::swapBytes(valueToWrite), readValue);
  }

  // int64_t
  {
    int64_t valueToWrite = 73823;
    int64_t readValue {};

    // writes value in output stream buffer
    TestWriter writer;
    OutputStream out(writer);
    out.writeInt64(valueToWrite);

    // create reader using writer's buffer
    auto buff = writer.getBuffer();
    TestReader reader(buff);

    // read value from input stream buffer
    sen::InputStreamTemplate<sen::BigEndian> in(sen::makeConstSpan<uint8_t>(reader.getBuffer()));
    in.readInt64(readValue);

    // check read value and original writen value are the same
    EXPECT_EQ(sen::impl::swapBytes(valueToWrite), readValue);
  }
}
