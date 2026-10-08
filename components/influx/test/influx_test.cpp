// === influx_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// components
#include "data_point.h"
#include "database.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"

// google test
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace influx::test
{

using sen::components::influx::Database;
using sen::components::influx::DataPoint;
using sen::components::influx::Transport;

/// A transport that keeps what it was asked to send, so a test can read the line protocol the
/// database produced instead of a socket.
class RecordingTransport final: public Transport
{
public:
  explicit RecordingTransport(std::vector<std::string>& sink): sink_(sink) {}

  void send(std::string&& message) override { sink_.emplace_back(std::move(message)); }

private:
  std::vector<std::string>& sink_;
};

/// Nanoseconds since the epoch, so every expectation can name the timestamp it ends with.
constexpr std::int64_t fixedTime = 1'700'000'000'000'000'000;

[[nodiscard]] DataPoint makePoint(std::string measurement = "cpu")
{
  return DataPoint {std::move(measurement), sen::TimeStamp {sen::Duration {fixedTime}}};
}

[[nodiscard]] std::string timeSuffix() { return " " + std::to_string(fixedTime); }

/// @test
/// A point with a tag and a field becomes one line of influx line protocol.
TEST(InfluxLineProtocolTest, APointBecomesOneLine)
{
  std::vector<std::string> sent;
  Database database {std::make_unique<RecordingTransport>(sent)};

  DataPoint point = makePoint();
  point.addTag("host", "a").addField("value", DataPoint::FieldValue {std::int32_t {7}});
  database.write(std::move(point));

  ASSERT_EQ(sent.size(), 1U);
  EXPECT_EQ(sent.front(), "cpu,host=a value=7i" + timeSuffix());
}

/// @test
/// A point with no tags has no comma after the measurement.
TEST(InfluxLineProtocolTest, NoTagsLeavesNoComma)
{
  std::vector<std::string> sent;
  Database database {std::make_unique<RecordingTransport>(sent)};

  DataPoint point = makePoint();
  point.addField("value", DataPoint::FieldValue {std::int32_t {7}});
  database.write(std::move(point));

  ASSERT_EQ(sent.size(), 1U);
  EXPECT_EQ(sent.front(), "cpu value=7i" + timeSuffix());
}

/// @test
/// A point carrying nothing but a measurement is still a line, not an empty string.
TEST(InfluxLineProtocolTest, AMeasurementOnPointsOwnIsALine)
{
  std::vector<std::string> sent;
  Database database {std::make_unique<RecordingTransport>(sent)};

  database.write(makePoint());

  ASSERT_EQ(sent.size(), 1U);
  EXPECT_EQ(sent.front(), "cpu" + timeSuffix());
}

/// @test
/// Integers carry the suffix that tells influx their signedness.
TEST(InfluxLineProtocolTest, IntegerFieldsCarryASignednessSuffix)
{
  std::vector<std::string> sent;
  Database database {std::make_unique<RecordingTransport>(sent)};

  DataPoint point = makePoint();
  point.addField("i8", DataPoint::FieldValue {std::uint8_t {1}})
    .addField("i16", DataPoint::FieldValue {std::int16_t {-2}})
    .addField("u16", DataPoint::FieldValue {std::uint16_t {3}})
    .addField("i32", DataPoint::FieldValue {std::int32_t {-4}})
    .addField("u32", DataPoint::FieldValue {std::uint32_t {5}})
    .addField("i64", DataPoint::FieldValue {std::int64_t {-6}})
    .addField("u64", DataPoint::FieldValue {std::uint64_t {7}});
  database.write(std::move(point));

  ASSERT_EQ(sent.size(), 1U);
  EXPECT_EQ(sent.front(), "cpu i8=1u,i16=-2i,u16=3u,i32=-4i,u32=5u,i64=-6i,u64=7u" + timeSuffix());
}

/// @test
/// A bool is a bare keyword, a string is quoted, and a time is nanoseconds.
TEST(InfluxLineProtocolTest, TheOtherFieldTypesFormatAsInfluxExpects)
{
  std::vector<std::string> sent;
  Database database {std::make_unique<RecordingTransport>(sent)};

  DataPoint point = makePoint();
  point.addField("on", DataPoint::FieldValue {true})
    .addField("off", DataPoint::FieldValue {false})
    .addField("name", DataPoint::FieldValue {std::string {"abc"}})
    .addField("period", DataPoint::FieldValue {sen::Duration {1500}})
    .addField("at", DataPoint::FieldValue {sen::TimeStamp {sen::Duration {42}}});
  database.write(std::move(point));

  ASSERT_EQ(sent.size(), 1U);
  EXPECT_EQ(sent.front(), R"(cpu on=true,off=false,name="abc",period=1500,at=42)" + timeSuffix());
}

/// @test
/// A comma or a space in the measurement is escaped, because either would end it.
TEST(InfluxLineProtocolTest, TheMeasurementIsEscaped)
{
  std::vector<std::string> sent;
  Database database {std::make_unique<RecordingTransport>(sent)};

  DataPoint point = makePoint("cpu load,avg");
  point.addField("value", DataPoint::FieldValue {std::int32_t {7}});
  database.write(std::move(point));

  ASSERT_EQ(sent.size(), 1U);
  EXPECT_EQ(sent.front(), R"(cpu\ load\,avg value=7i)" + timeSuffix());
}

/// @test
/// A tag with no name, a tag with no value, and a field with no name are all dropped rather
/// than written as empty keys.
TEST(InfluxDataPointTest, AnUnnamedTagOrFieldIsDropped)
{
  DataPoint point = makePoint();
  point.addTag("", "a").addTag("host", "").addField("", DataPoint::FieldValue {std::int32_t {7}});

  EXPECT_TRUE(point.getTagSet().empty());
  EXPECT_TRUE(point.getFieldSet().empty());
}

/// @test
/// Without batching every point is sent on its own.
TEST(InfluxBatchingTest, WithoutBatchingEachPointIsSentAlone)
{
  std::vector<std::string> sent;
  Database database {std::make_unique<RecordingTransport>(sent)};

  database.write(makePoint("one"));
  database.write(makePoint("two"));

  ASSERT_EQ(sent.size(), 2U);
  EXPECT_EQ(sent.at(0), "one" + timeSuffix());
  EXPECT_EQ(sent.at(1), "two" + timeSuffix());
}

/// @test
/// A full batch is sent as one message of newline-separated lines, with no trailing newline.
TEST(InfluxBatchingTest, AFullBatchIsOneMessage)
{
  std::vector<std::string> sent;
  Database database {std::make_unique<RecordingTransport>(sent)};
  database.batchOf(3);

  database.write(makePoint("one"));
  database.write(makePoint("two"));
  EXPECT_TRUE(sent.empty()) << "a partial batch was sent before it filled";

  database.write(makePoint("three"));

  ASSERT_EQ(sent.size(), 1U);
  EXPECT_EQ(sent.front(), "one" + timeSuffix() + "\ntwo" + timeSuffix() + "\nthree" + timeSuffix());
}

/// @test
/// Flushing sends what has accumulated and leaves the batch empty.
TEST(InfluxBatchingTest, FlushingSendsAPartialBatchOnce)
{
  std::vector<std::string> sent;
  Database database {std::make_unique<RecordingTransport>(sent)};
  database.batchOf(10);

  database.write(makePoint("one"));
  database.flushBatch();
  database.flushBatch();

  ASSERT_EQ(sent.size(), 1U) << "the second flush sent the same points again";
  EXPECT_EQ(sent.front(), "one" + timeSuffix());
}

/// @test
/// Flushing an empty batch sends nothing.
///
/// The guard matters beyond the empty message: joinLineProtocolBatch strips the trailing newline with
/// erase(prev(end())), which on an empty string is undefined.
TEST(InfluxBatchingTest, FlushingAnEmptyBatchSendsNothing)
{
  std::vector<std::string> sent;
  Database database {std::make_unique<RecordingTransport>(sent)};
  database.batchOf(10);

  database.flushBatch();

  EXPECT_TRUE(sent.empty());
}

/// @test
/// Each of a sample of the scalar types Sen carries, from bool to TimeStamp, becomes a field,
/// so more value shapes are covered than the few the line-protocol tests above happen to use.
TEST(InfluxValueConversionTest, EveryScalarTypeBecomesAField)
{
  const sen::TimeStamp time {sen::Duration {fixedTime}};
  const std::vector<std::pair<sen::ConstTypeHandle<>, sen::Var>> values {
    {sen::BoolType::get(), sen::Var(true)},
    {sen::UInt8Type::get(), sen::Var(std::uint8_t {8})},
    {sen::Int16Type::get(), sen::Var(std::int16_t {-16})},
    {sen::UInt16Type::get(), sen::Var(std::uint16_t {16})},
    {sen::Int64Type::get(), sen::Var(std::int64_t {-64})},
    {sen::UInt64Type::get(), sen::Var(std::uint64_t {64})},
    {sen::DurationType::get(), sen::Var(sen::Duration {1000})},
    {sen::TimestampType::get(), sen::Var(time)},
  };

  for (const auto& entry: values)
  {
    std::vector<std::string> sent;
    Database database {std::make_unique<RecordingTransport>(sent)};
    DataPoint point = makePoint();

    sen::components::influx::varToFields(point, entry.first.type(), entry.second, time, "field");
    database.write(std::move(point));

    ASSERT_EQ(sent.size(), 1U) << "nothing was sent for a " << entry.first->getName();
    EXPECT_NE(sent.front().find("field="), std::string::npos)
      << "no field came out of a " << entry.first->getName() << ": " << sent.front();
  }
}

/// @test
/// A value whose type is not the one it is being read as is refused rather than reinterpreted,
/// which is what stops a recording carrying a number nothing measured.
TEST(InfluxValueConversionTest, RefusesAValueOfTheWrongType)
{
  const sen::TimeStamp time {sen::Duration {fixedTime}};
  DataPoint point = makePoint();

  EXPECT_THROW(sen::components::influx::varToFields(
                 point, sen::Int64Type::get().type(), sen::Var(std::string {"not a number"}), time, "field"),
               std::exception);
}

}  // namespace influx::test
