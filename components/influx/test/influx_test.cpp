// === influx_test.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// components
#include "bounded_write.h"
#include "data_point.h"
#include "database.h"
#include "http.h"
#include "io_thread.h"
#include "tcp.h"
#include "udp.h"
#include "unix_datagram.h"
#include "unix_stream.h"

// sen
#include "sen/core/base/duration.h"
#include "sen/core/base/scope_guard.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/time_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/var.h"

// asio
#include <asio/buffer.hpp>
#include <asio/buffers_iterator.hpp>
#include <asio/error.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <asio/local/datagram_protocol.hpp>
#include <asio/local/stream_protocol.hpp>
#include <asio/posix/stream_descriptor.hpp>
#include <asio/post.hpp>
#include <asio/read.hpp>        // NOLINT(misc-include-cleaner): declares asio::read, used below
#include <asio/read_until.hpp>  // NOLINT(misc-include-cleaner): declares asio::read_until, used below
#include <asio/streambuf.hpp>
#include <asio/write.hpp>  // NOLINT(misc-include-cleaner): declares asio::write, used below

// zlib
#include <zconf.h>
#include <zlib.h>

// google test
#include <gtest/gtest.h>

// std
#include <unistd.h>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <future>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace influx::test

// asio's umbrella headers forward to impl headers, so misc-include-cleaner cannot map asio::read,
// asio::write, asio::read_until or asio::transfer_exactly to the header declaring them. One block
// for the file: a suppression per line gets rewrapped away from its target.
// NOLINTBEGIN(misc-include-cleaner)
{

using sen::components::influx::batchBoundFor;
using sen::components::influx::Database;
using sen::components::influx::datagramCeilingFor;
using sen::components::influx::DataPoint;
using sen::components::influx::HttpTransport;
using sen::components::influx::IoThread;
using sen::components::influx::IpFamily;
using sen::components::influx::TCP;
using sen::components::influx::Transport;
using sen::components::influx::UDP;
#if defined(ASIO_HAS_LOCAL_SOCKETS)
using sen::components::influx::UnixDatagram;
using sen::components::influx::UnixStream;
#endif

/// A transport that keeps what it was asked to send, so a test can read the line protocol the
/// database produced instead of a socket.
class RecordingTransport: public Transport
{
public:
  explicit RecordingTransport(std::vector<std::string>& sink): sink_(sink) {}

protected:
  void doSend(sen::Span<const char> message) override { sink_.emplace_back(message.data(), message.size()); }

private:
  std::vector<std::string>& sink_;
};

/// The same, for a transport whose protocol delimits records, which is what TCP does. A named type
/// rather than a flag on the one above, so a test that means to exercise the separator says so.
class SeparatingTransport final: public RecordingTransport
{
public:
  using RecordingTransport::RecordingTransport;

protected:
  [[nodiscard]] bool requiresLineSeparator() const noexcept override { return true; }
};

/// Inflates a gzip stream, so a test can read what a compressed transport actually sent.
[[nodiscard]] std::string inflateGzip(const std::string& compressed)
{
  z_stream zs = {};

  // windowBits = 15 + 32 lets zlib accept a gzip header
  if (inflateInit2(&zs, 15 + 32) != Z_OK)
  {
    throw std::runtime_error("inflateInit2 failed");
  }

  const auto endStream = sen::makeScopeGuard([&zs]() { inflateEnd(&zs); });

  zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressed.data()));  // NOLINT
  zs.avail_in = static_cast<uInt>(compressed.size());

  std::string out;
  std::vector<char> buffer(32768);
  int ret = Z_OK;

  do
  {
    zs.next_out = reinterpret_cast<Bytef*>(buffer.data());  // NOLINT
    zs.avail_out = static_cast<uInt>(buffer.size());
    ret = inflate(&zs, Z_NO_FLUSH);
    out.append(buffer.data(), buffer.size() - zs.avail_out);
  } while (ret == Z_OK);

  if (ret != Z_STREAM_END)
  {
    throw std::runtime_error("inflate did not reach the end of the stream");
  }

  return out;
}

/// Nanoseconds since the epoch, so every expectation can name the timestamp it ends with.
constexpr std::int64_t fixedTime = 1'700'000'000'000'000'000;

/// Runs one asynchronous operation on the acceptor's own context and gives up after a deadline.
/// Without this, a test that never gets its connection hangs instead of failing, and a hang in CI
/// says far less than a failure does.
template <typename Start>
[[nodiscard]] asio::error_code awaitBounded(asio::io_context& context, Start&& start)
{
  // Long enough that a loaded machine does not fail a working test, short enough to be a failure
  // rather than a stall.
  constexpr auto testTimeout = std::chrono::seconds {5};

  asio::error_code result = asio::error::would_block;
  start([&result](const asio::error_code& error, auto&&...) { result = error; });

  context.restart();
  std::ignore = context.run_for(testTimeout);

  return result == asio::error::would_block ? asio::error::timed_out : result;
}

[[nodiscard]] DataPoint makePoint(std::string measurement = "cpu")
{
  return DataPoint {std::move(measurement), sen::TimeStamp {sen::Duration {fixedTime}}};
}

[[nodiscard]] std::string timeSuffix() { return " " + std::to_string(fixedTime); }

/// A point carrying one field, so a line long enough to be worth compressing comes out of it.
/// A value deflate cannot shrink much, so a batch of these is over the bound compressed as well as
/// uncompressed. Sixteen near-identical lines gzip to less than the bound, and are then correctly
/// sent whole, which is not what the partitioning is there to do.
[[nodiscard]] std::string incompressibleText(std::uint32_t seed)
{
  static constexpr std::string_view alphabet {"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"};

  std::mt19937 generator {seed};
  std::string text;
  for (auto i = 0; i < 24; ++i)
  {
    text.push_back(alphabet[generator() % alphabet.size()]);
  }

  return text;
}

[[nodiscard]] DataPoint makePointWithAField()
{
  DataPoint point = makePoint();
  point.addTag("host", "a").addField("value", DataPoint::FieldValue {std::int32_t {7}});
  return point;
}

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

/// @test
/// A transport whose protocol delimits records gets the separator whether or not the payload is
/// compressed. The batch leaves the database without a trailing newline, so a transport that
/// dropped it would run the next write onto the end of the last record.
TEST(InfluxCompressionTest, TheLineSeparatorIsAddedWhetherOrNotTheDataIsCompressed)
{
  {
    std::vector<std::string> sent;
    auto transport = std::make_unique<SeparatingTransport>(sent);
    Database database {std::move(transport)};

    database.write(makePointWithAField());

    ASSERT_EQ(sent.size(), 1U);
    EXPECT_EQ(sent.front().back(), '\n') << sent.front();
  }

  {
    std::vector<std::string> sent;
    auto transport = std::make_unique<SeparatingTransport>(sent);
    transport->setCompressionEnabled(true, Z_DEFAULT_COMPRESSION);
    Database database {std::move(transport)};

    database.write(makePointWithAField());

    ASSERT_EQ(sent.size(), 1U);
    EXPECT_EQ(inflateGzip(sent.front()).back(), '\n') << "the compressed payload lost its separator";
  }
}

/// @test
/// A compressed transport sends a gzip stream that inflates back to the line protocol, and sends
/// it as bytes rather than as text: nothing in the payload may be read as NUL-terminated.
TEST(InfluxCompressionTest, ACompressedPayloadInflatesToTheLineProtocol)
{
  std::vector<std::string> plain;
  std::vector<std::string> compressed;

  {
    auto transport = std::make_unique<RecordingTransport>(plain);
    Database database {std::move(transport)};
    database.write(makePointWithAField());
  }

  {
    auto transport = std::make_unique<RecordingTransport>(compressed);
    transport->setCompressionEnabled(true, Z_DEFAULT_COMPRESSION);
    Database database {std::move(transport)};
    database.write(makePointWithAField());
  }

  ASSERT_EQ(plain.size(), 1U);
  ASSERT_EQ(compressed.size(), 1U);

  EXPECT_NE(compressed.front(), plain.front());
  ASSERT_GE(compressed.front().size(), 3U);
  EXPECT_EQ(static_cast<unsigned char>(compressed.front()[0]), 0x1FU) << "no gzip magic";
  EXPECT_EQ(static_cast<unsigned char>(compressed.front()[1]), 0x8BU) << "no gzip magic";
  EXPECT_EQ(inflateGzip(compressed.front()), plain.front());
}

/// @test
/// Compression is off until it is asked for, so a configuration that selects none sends the line
/// protocol as it stands.
TEST(InfluxCompressionTest, CompressionIsOffUntilItIsEnabled)
{
  std::vector<std::string> sent;
  auto transport = std::make_unique<RecordingTransport>(sent);

  EXPECT_FALSE(transport->getCompressionEnabled());

  Database database {std::move(transport)};
  database.write(makePointWithAField());

  ASSERT_EQ(sent.size(), 1U);
  EXPECT_EQ(sent.front().find("cpu"), 0U) << sent.front();
}

/// @test
/// A compression level deflate would refuse is reported where the configuration can still be
/// named, rather than on every send for the life of the run.
TEST(InfluxCompressionTest, AnImpossibleCompressionLevelIsRefused)
{
  std::vector<std::string> sent;
  RecordingTransport transport {sent};

  EXPECT_THROW(transport.setCompressionEnabled(true, Z_BEST_COMPRESSION + 1), std::runtime_error);
  EXPECT_THROW(transport.setCompressionEnabled(true, Z_DEFAULT_COMPRESSION - 1), std::runtime_error);

  EXPECT_NO_THROW(transport.setCompressionEnabled(true, Z_DEFAULT_COMPRESSION));
  EXPECT_NO_THROW(transport.setCompressionEnabled(true, Z_NO_COMPRESSION));
  EXPECT_NO_THROW(transport.setCompressionEnabled(true, Z_BEST_COMPRESSION));
}

/// @test
/// A line break inside a recorded value cannot be escaped, because it is the record terminator
/// itself. Left in, it would end the record early and have the remainder parsed as another one, so
/// it is removed and one message still carries exactly one line.
TEST(InfluxLineProtocolTest, ALineBreakInsideAValueCannotSplitTheRecord)
{
  std::vector<std::string> sent;
  Database database {std::make_unique<RecordingTransport>(sent)};

  DataPoint point = makePoint("cpu");
  point.addTag("host", "a\nb");
  point.addField("value", DataPoint::FieldValue {std::string {"first\nsecond\r\nthird"}});
  database.write(std::move(point));

  ASSERT_EQ(sent.size(), 1U);
  const auto& line = sent.front();

  EXPECT_EQ(line.find('\n'), std::string::npos) << "a line break survived into the record: " << line;
  EXPECT_EQ(line.find('\r'), std::string::npos) << "a carriage return survived into the record: " << line;
  EXPECT_NE(line.find("host=ab"), std::string::npos) << line;
  EXPECT_NE(line.find(R"(value="firstsecondthird")"), std::string::npos) << line;
}

/// @test
/// Line protocol gives commas, spaces and equals signs a meaning in keys and tag values, and a
/// double quote or backslash a meaning inside a quoted string field. Each is escaped on the way
/// out, or the record Telegraf parses is not the record that was recorded.
TEST(InfluxLineProtocolTest, EveryElementIsEscapedWhereTheProtocolNeedsIt)
{
  std::vector<std::string> sent;
  Database database {std::make_unique<RecordingTransport>(sent)};

  DataPoint point = makePoint("cpu load,busy");
  point.addTag("host name", "a=b,c");
  point.addField("field key", DataPoint::FieldValue {std::string {R"(a "quoted" \ value)"}});
  database.write(std::move(point));

  ASSERT_EQ(sent.size(), 1U);
  const auto& line = sent.front();

  // The measurement escapes a comma and a space but not an equals sign; keys and tag values escape
  // all three; a string field value escapes the quote and the backslash instead.
  EXPECT_NE(line.find(R"(cpu\ load\,busy)"), std::string::npos) << line;
  EXPECT_NE(line.find(R"(host\ name=a\=b\,c)"), std::string::npos) << line;
  EXPECT_NE(line.find(R"(field\ key=)"), std::string::npos) << line;
  EXPECT_NE(line.find(R"(a \"quoted\" \\ value)"), std::string::npos) << line;
}

/// @test
/// A payload over the message bound is cut at line boundaries into several messages, each within
/// the bound, and the lines arrive once each and in order. Compression only reveals the size once
/// it has run, so the cut cannot be made before it.
TEST(InfluxMessageBoundTest, APayloadOverTheBoundIsCutIntoWholeLines)
{
  for (const bool compressed: {false, true})
  {
    std::vector<std::string> sent;
    auto transport = std::make_unique<RecordingTransport>(sent);
    transport->setMaxMessageSize(120U);
    if (compressed)
    {
      transport->setCompressionEnabled(true, Z_DEFAULT_COMPRESSION);
    }

    Database database {std::move(transport)};
    database.batchOf(16);

    for (int i = 0; i < 16; ++i)
    {
      DataPoint point = makePoint("m" + std::to_string(i));
      point.addField("value", DataPoint::FieldValue {incompressibleText(static_cast<std::uint32_t>(i))});
      database.write(std::move(point));
    }

    ASSERT_GT(sent.size(), 1U) << "the payload was not cut at all, compressed=" << compressed;

    std::string rejoined;
    for (const auto& message: sent)
    {
      EXPECT_LE(message.size(), 120U) << "a message was over the bound, compressed=" << compressed;

      const auto lines = compressed ? inflateGzip(message) : message;
      if (!rejoined.empty())
      {
        rejoined.append("\n");
      }
      rejoined.append(lines);
    }

    for (int i = 0; i < 16; ++i)
    {
      const auto expected =
        "m" + std::to_string(i) + " value=\"" + incompressibleText(static_cast<std::uint32_t>(i)) + "\"";
      EXPECT_NE(rejoined.find(expected), std::string::npos) << expected << " did not arrive, compressed=" << compressed;
    }
  }
}

/// @test
/// A single line that does not fit the bound cannot be sent at all, so it is dropped rather than
/// handed to a socket that would refuse it.
TEST(InfluxMessageBoundTest, ASingleLineOverTheBoundIsDropped)
{
  std::vector<std::string> sent;
  auto transport = std::make_unique<RecordingTransport>(sent);
  transport->setMaxMessageSize(8U);

  Database database {std::move(transport)};
  database.write(makePointWithAField());

  EXPECT_TRUE(sent.empty()) << "a line too large for one message was sent anyway: " << sent.front();
}

/// @test
/// The batch is bounded in bytes as well as in points, so a message stays within the bound however
/// wide the points turn out to be. Counting points alone could not do that.
TEST(InfluxMessageBoundTest, TheBatchIsFlushedBeforeItPassesTheBound)
{
  std::vector<std::string> sent;
  auto transport = std::make_unique<RecordingTransport>(sent);
  Database database {std::move(transport)};

  // A batch of points far larger than the bound allows, so the bound is what decides.
  database.batchOf(1000);
  database.messageOf(100U);

  for (int i = 0; i < 20; ++i)
  {
    DataPoint point = makePoint("measure" + std::to_string(i));
    point.addField("value", DataPoint::FieldValue {std::int32_t {i}});
    database.write(std::move(point));
  }

  database.flushBatch();

  ASSERT_GT(sent.size(), 1U) << "the bound never flushed the batch";
  for (const auto& message: sent)
  {
    EXPECT_LE(message.size(), 100U) << message;
  }
}

#if defined(ASIO_HAS_LOCAL_SOCKETS)

/// @test
/// A Unix datagram socket carries a message to a listener bound to the same path. Telegraf is not
/// needed for this: the test binds the socket itself and reads what arrives.
TEST(InfluxUnixTransportTest, ADatagramArrivesAtASocketBoundToThePath)
{
  const auto path = std::filesystem::temp_directory_path() / "sen_influx_unixgram_test.sock";
  std::filesystem::remove(path);

  asio::io_context io;
  asio::local::datagram_protocol::socket listener {io, asio::local::datagram_protocol::endpoint {path.string()}};

  Database database {std::make_unique<UnixDatagram>(io, path.string())};
  database.write(makePointWithAField());

  std::array<char, 1024> buffer {};
  const auto receiveError = awaitBounded(
    io,
    [&](auto&& handler) { listener.async_receive(asio::buffer(buffer), std::forward<decltype(handler)>(handler)); });
  ASSERT_FALSE(receiveError) << receiveError.message();

  // The buffer starts zeroed and line protocol carries no NUL, so what arrived ends at the first.
  const std::string message {buffer.data()};
  EXPECT_NE(message.find("cpu,host=a value=7i"), std::string::npos) << message;

  asio::error_code error;
  std::ignore = listener.close(error);
  std::filesystem::remove(path);
}

/// @test
/// A Unix stream socket carries a message, with the line separator a stream listener needs to tell
/// one record from the next.
TEST(InfluxUnixTransportTest, AStreamArrivesWithItsSeparator)
{
  const auto path = std::filesystem::temp_directory_path() / "sen_influx_unix_test.sock";
  std::filesystem::remove(path);

  asio::io_context io;
  asio::local::stream_protocol::acceptor acceptor {io, asio::local::stream_protocol::endpoint {path.string()}};

  // The transport dials on its first send, not when it is built, so the write comes before the
  // accept: the connection waits in the acceptor's backlog until it is taken.
  Database database {std::make_unique<UnixStream>(io, path.string())};
  database.write(makePointWithAField());

  asio::local::stream_protocol::socket peer {io};
  const auto acceptError =
    awaitBounded(io, [&](auto&& handler) { acceptor.async_accept(peer, std::forward<decltype(handler)>(handler)); });
  ASSERT_FALSE(acceptError) << acceptError.message();

  // Up to the separator a stream listener splits on.
  asio::streambuf incoming;
  const auto readError = awaitBounded(
    io,
    [&](auto&& handler) { asio::async_read_until(peer, incoming, '\n', std::forward<decltype(handler)>(handler)); });
  ASSERT_FALSE(readError) << readError.message();

  const std::string message {asio::buffers_begin(incoming.data()), asio::buffers_end(incoming.data())};
  EXPECT_EQ(message.back(), '\n') << message;
  EXPECT_NE(message.find("cpu,host=a value=7i"), std::string::npos) << message;

  asio::error_code closing;
  std::ignore = acceptor.close(closing);
  std::filesystem::remove(path);
}

#endif  // defined(ASIO_HAS_LOCAL_SOCKETS)

/// Serves one request on another thread. The transport waits for its answer before returning, so
/// a test that served the request after writing would be waiting for itself.
[[nodiscard]] std::future<std::string> serveInBackground(asio::io_context& context,
                                                         asio::ip::tcp::acceptor& acceptor,
                                                         std::string_view status);

[[nodiscard]] std::string serveOneRequest(asio::io_context& context,
                                          asio::ip::tcp::acceptor& acceptor,
                                          std::string_view status)
{
  asio::ip::tcp::socket peer {context};
  if (awaitBounded(context,
                   [&](auto&& handler) { acceptor.async_accept(peer, std::forward<decltype(handler)>(handler)); }))
  {
    return {};
  }

  asio::streambuf incoming;
  if (awaitBounded(context,
                   [&](auto&& handler)
                   {
                     asio::async_read_until(
                       peer, incoming, std::string {"\r\n\r\n"}, std::forward<decltype(handler)>(handler));
                   }))
  {
    return {};
  }

  std::string request {asio::buffers_begin(incoming.data()), asio::buffers_end(incoming.data())};

  // The body follows the headers, and Content-Length says how much of it has not arrived yet.
  const auto marker = request.find("Content-Length: ");
  if (marker != std::string::npos)
  {
    const auto length = std::stoul(request.substr(marker + 16U));
    const auto headerEnd = request.find("\r\n\r\n") + 4U;
    const auto have = request.size() - headerEnd;
    if (have < length)
    {
      if (awaitBounded(
            context,
            [&](auto&& handler)
            {
              asio::async_read(
                peer, incoming, asio::transfer_exactly(length - have), std::forward<decltype(handler)>(handler));
            }))
      {
        return {};
      }

      request.assign(asio::buffers_begin(incoming.data()), asio::buffers_end(incoming.data()));
    }
  }

  asio::error_code error;
  const std::string answer {std::string {status} + "\r\nContent-Length: 0\r\n\r\n"};
  std::ignore = asio::write(peer, asio::buffer(answer), error);

  return request;
}

[[nodiscard]] std::future<std::string> serveInBackground(asio::io_context& context,
                                                         asio::ip::tcp::acceptor& acceptor,
                                                         std::string_view status)
{
  return std::async(std::launch::async,
                    [&context, &acceptor, status] { return serveOneRequest(context, acceptor, status); });
}

/// @test
/// The HTTP transport posts the line protocol to the path it was given, with a Host header and a
/// Content-Length that matches the body. The body is framed by that length, so no separator and no
/// datagram bound apply.
TEST(InfluxHttpTransportTest, APostCarriesTheLineProtocolAndItsLength)
{
  asio::io_context listenerIo;
  asio::ip::tcp::acceptor acceptor {listenerIo, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto port = acceptor.local_endpoint().port();
  auto served = serveInBackground(listenerIo, acceptor, "HTTP/1.1 204 No Content");

  Database database {std::make_unique<HttpTransport>("127.0.0.1", port, "/write")};
  database.write(makePointWithAField());

  const auto request = served.get();

  ASSERT_FALSE(request.empty());
  EXPECT_EQ(request.rfind("POST /write HTTP/1.1", 0U), 0U) << request;
  EXPECT_NE(request.find("Host: 127.0.0.1:" + std::to_string(port)), std::string::npos) << request;
  EXPECT_NE(request.find("cpu,host=a value=7i"), std::string::npos) << request;
  EXPECT_EQ(request.find("Content-Encoding"), std::string::npos) << "compression was not asked for";

  const auto marker = request.find("Content-Length: ");
  ASSERT_NE(marker, std::string::npos) << request;
  const auto declared = std::stoul(request.substr(marker + 16U));
  const auto body = request.substr(request.find("\r\n\r\n") + 4U);
  EXPECT_EQ(declared, body.size()) << "the length did not match the body";
}

/// @test
/// A compressed body is announced with Content-Encoding, which is how Telegraf's listener knows to
/// inflate it, and it inflates back to the line protocol.
TEST(InfluxHttpTransportTest, ACompressedPostAnnouncesItsEncoding)
{
  asio::io_context listenerIo;
  asio::ip::tcp::acceptor acceptor {listenerIo, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto port = acceptor.local_endpoint().port();
  auto served = serveInBackground(listenerIo, acceptor, "HTTP/1.1 204 No Content");

  auto transport = std::make_unique<HttpTransport>("127.0.0.1", port, "/write");
  transport->setCompressionEnabled(true, Z_DEFAULT_COMPRESSION);
  Database database {std::move(transport)};
  database.write(makePointWithAField());

  const auto request = served.get();

  ASSERT_FALSE(request.empty());
  EXPECT_NE(request.find("Content-Encoding: gzip"), std::string::npos) << request;

  const auto body = request.substr(request.find("\r\n\r\n") + 4U);
  EXPECT_NE(inflateGzip(body).find("cpu,host=a value=7i"), std::string::npos);
}

/// @test
/// A listener that refuses the batch says so, and the transport reports it. This is the one thing
/// the socket transports cannot do: there, a rejected batch is indistinguishable from an accepted
/// one.
TEST(InfluxHttpTransportTest, ARefusedBatchIsReported)
{
  asio::io_context listenerIo;
  asio::ip::tcp::acceptor acceptor {listenerIo, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto port = acceptor.local_endpoint().port();
  auto served = serveInBackground(listenerIo, acceptor, "HTTP/1.1 400 Bad Request");

  Database database {std::make_unique<HttpTransport>("127.0.0.1", port, "/write")};

  testing::internal::CaptureStderr();
  database.write(makePointWithAField());
  const auto request = served.get();
  const auto reported = testing::internal::GetCapturedStderr();

  ASSERT_FALSE(request.empty());
  EXPECT_NE(reported.find("400"), std::string::npos) << "a refusal was not reported: " << reported;
}

/// @test
/// Telegraf's reason for not taking a batch whole arrives in the X-Influxdb-Error header, and is
/// reported. The status alone cannot distinguish a clean refusal from a batch whose well-formed
/// lines were kept, so this text is the only thing that can.
///
/// The colon in the reason phrase is legal, and covered here as an input rather than as a guard:
/// the header that matters is found either way.
TEST(InfluxHttpTransportTest, TheListenersReasonIsReportedAndAColonInTheReasonPhraseDoesNotHideIt)
{
  asio::io_context listenerIo;
  asio::ip::tcp::acceptor acceptor {listenerIo, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto port = acceptor.local_endpoint().port();

  // serveOneRequest appends the blank line and a zero length, so extra field lines go here.
  auto served = serveInBackground(listenerIo,
                                  acceptor,
                                  "HTTP/1.1 400 Bad Request: malformed\r\n"
                                  "X-Influxdb-Error: unable to parse 'cpu value': invalid field format");

  Database database {std::make_unique<HttpTransport>("127.0.0.1", port, "/write")};

  testing::internal::CaptureStderr();
  database.write(makePointWithAField());
  const auto request = served.get();
  const auto reported = testing::internal::GetCapturedStderr();

  ASSERT_FALSE(request.empty()) << "the request never arrived, so this proves nothing";

  EXPECT_NE(reported.find("invalid field format"), std::string::npos)
    << "the listener's reason was not reported: " << reported;
  EXPECT_NE(reported.find("400"), std::string::npos) << reported;

  // A 400 usually means the well-formed lines were kept, so the report must not call it a refusal.
  EXPECT_EQ(reported.find("refused"), std::string::npos)
    << "a 400 was reported as a refusal, which it usually is not: " << reported;
}

/// @test
/// An answer whose lines end in a bare LF is still read. RFC 7230 asks a recipient to accept them,
/// and a peer that uses them never produces a CRLF CRLF at all -- so a transport looking only for
/// the conformant terminator reads until its deadline on every single answer and drops every batch,
/// for the life of the run. Telegraf is not such a peer; this file puts one in scope anyway.
TEST(InfluxHttpTransportTest, AnAnswerWithBareLineFeedsIsStillRead)
{
  asio::io_context listenerIo;
  asio::ip::tcp::acceptor acceptor {listenerIo, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto port = acceptor.local_endpoint().port();

  auto served = std::async(
    std::launch::async,
    [&listenerIo, &acceptor]
    {
      auto& context = listenerIo;

      asio::ip::tcp::socket peer {context};
      if (awaitBounded(context,
                       [&](auto&& handler) { acceptor.async_accept(peer, std::forward<decltype(handler)>(handler)); }))
      {
        return 0;
      }

      auto seen = 0;
      for (auto i = 0; i < 2; ++i)
      {
        asio::streambuf incoming;
        if (awaitBounded(context,
                         [&](auto&& handler)
                         {
                           asio::async_read_until(
                             peer, incoming, std::string {"\r\n\r\n"}, std::forward<decltype(handler)>(handler));
                         }))
        {
          break;
        }

        ++seen;

        asio::error_code writing;
        const std::string answer {"HTTP/1.1 204 No Content\n\n"};
        std::ignore = asio::write(peer, asio::buffer(answer), writing);
        if (writing)
        {
          break;
        }
      }

      return seen;
    });

  Database database {std::make_unique<HttpTransport>("127.0.0.1", port, "/write")};
  database.write(makePointWithAField());
  database.write(makePointWithAField());

  EXPECT_EQ(served.get(), 2) << "a bare-LF answer was not read, so the connection was abandoned";
}

/// @test
/// An answer asking for the connection to close gets it closed. Go sends Connection: close on the
/// paths where its handler leaves the body unread -- a 413, or a gzip body it could not open -- so
/// a transport that ignores it sends the next batch on a socket the server has finished with.
///
/// The listener holds its first socket open after answering, so only the transport can end that
/// connection. A listener that closed it would make the assertion pass either way.
TEST(InfluxHttpTransportTest, AnAnswerAskingToCloseIsNotReused)
{
  asio::io_context listenerIo;
  asio::ip::tcp::acceptor acceptor {listenerIo, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto port = acceptor.local_endpoint().port();

  auto served = std::async(
    std::launch::async,
    [&listenerIo, &acceptor]
    {
      auto& context = listenerIo;

      // Held for the whole lambda, so nothing but the transport can end this connection.
      asio::ip::tcp::socket first {context};
      if (awaitBounded(context,
                       [&](auto&& handler) { acceptor.async_accept(first, std::forward<decltype(handler)>(handler)); }))
      {
        return 0;
      }

      asio::streambuf incoming;
      if (awaitBounded(context,
                       [&](auto&& handler)
                       {
                         asio::async_read_until(
                           first, incoming, std::string {"\r\n\r\n"}, std::forward<decltype(handler)>(handler));
                       }))
      {
        return 0;
      }

      asio::error_code writing;
      const std::string answer {"HTTP/1.1 204 No Content\r\nConnection: close\r\nContent-Length: 0\r\n\r\n"};
      std::ignore = asio::write(first, asio::buffer(answer), writing);

      // Honoured, and the second batch dials again; ignored, and it goes out on `first` and nothing
      // arrives here.
      asio::ip::tcp::socket second {context};
      if (awaitBounded(
            context, [&](auto&& handler) { acceptor.async_accept(second, std::forward<decltype(handler)>(handler)); }))
      {
        return 1;
      }

      return 2;
    });

  Database database {std::make_unique<HttpTransport>("127.0.0.1", port, "/write")};
  database.write(makePointWithAField());
  database.write(makePointWithAField());

  EXPECT_EQ(served.get(), 2) << "the second batch reused a connection the answer asked to close";
}

/// @test
/// A 204 with no Content-Length keeps the connection. That is how Telegraf answers a successful
/// write -- it calls WriteHeader(StatusNoContent) and Go omits the length, because a 204 cannot
/// carry a body -- so a transport that treated a missing length as an unknown body extent would
/// reconnect on every healthy batch.
///
/// Every other test here sends an explicit "Content-Length: 0", which would hide it.
TEST(InfluxHttpTransportTest, ASuccessWithNoDeclaredLengthKeepsTheConnection)
{
  asio::io_context listenerIo;
  asio::ip::tcp::acceptor acceptor {listenerIo, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto port = acceptor.local_endpoint().port();

  // One connection, two requests, each answered as Go answers a 204: status line and nothing else.
  auto served = std::async(
    std::launch::async,
    [&listenerIo, &acceptor]
    {
      auto& context = listenerIo;

      asio::ip::tcp::socket peer {context};
      if (awaitBounded(context,
                       [&](auto&& handler) { acceptor.async_accept(peer, std::forward<decltype(handler)>(handler)); }))
      {
        return 0;
      }

      auto seen = 0;
      for (auto i = 0; i < 2; ++i)
      {
        asio::streambuf incoming;
        if (awaitBounded(context,
                         [&](auto&& handler)
                         {
                           asio::async_read_until(
                             peer, incoming, std::string {"\r\n\r\n"}, std::forward<decltype(handler)>(handler));
                         }))
        {
          break;
        }

        ++seen;

        asio::error_code writing;
        const std::string answer {"HTTP/1.1 204 No Content\r\n\r\n"};
        std::ignore = asio::write(peer, asio::buffer(answer), writing);
        if (writing)
        {
          break;
        }
      }

      return seen;
    });

  Database database {std::make_unique<HttpTransport>("127.0.0.1", port, "/write")};
  database.write(makePointWithAField());
  database.write(makePointWithAField());

  EXPECT_EQ(served.get(), 2) << "the second batch did not arrive on the same connection, so a 204 "
                                "without a declared length is being read as an unknown body extent";
}

/// @test
/// An answer that is merely slow is not evidence that Telegraf never read the request. It answers
/// only once its parse loop has ended and treats ten seconds as slow, so a batch it took and is
/// still working through looks, from here, exactly like a connection it never read. Sending that
/// again delivers every point in it twice, silently.
///
/// The first batch is answered promptly to leave a connection open: the misclassification needs a
/// reused one, so a single batch on a freshly dialled connection would prove nothing.
TEST(InfluxHttpTransportTest, ASlowAnswerOnAReusedConnectionIsNotTakenForAnUnreadRequest)
{
  asio::io_context listenerIo;
  asio::ip::tcp::acceptor acceptor {listenerIo, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto port = acceptor.local_endpoint().port();

  // One connection: the first request answered at once, the second only after the transport's
  // per-operation deadline has passed.
  auto served = std::async(
    std::launch::async,
    [&listenerIo, &acceptor]
    {
      auto& context = listenerIo;

      asio::ip::tcp::socket peer {context};
      if (awaitBounded(context,
                       [&](auto&& handler) { acceptor.async_accept(peer, std::forward<decltype(handler)>(handler)); }))
      {
        return 0;
      }

      auto seen = 0;
      for (auto i = 0; i < 2; ++i)
      {
        asio::streambuf incoming;
        if (awaitBounded(context,
                         [&](auto&& handler)
                         {
                           asio::async_read_until(
                             peer, incoming, std::string {"\r\n\r\n"}, std::forward<decltype(handler)>(handler));
                         }))
        {
          break;
        }

        ++seen;

        if (i == 1)
        {
          std::this_thread::sleep_for(std::chrono::milliseconds {1400});
        }

        asio::error_code writing;
        const std::string answer {"HTTP/1.1 204 No Content\r\nContent-Length: 0\r\n\r\n"};
        std::ignore = asio::write(peer, asio::buffer(answer), writing);
        if (writing)
        {
          break;
        }
      }

      return seen;
    });

  Database database {std::make_unique<HttpTransport>("127.0.0.1", port, "/write")};

  // The first leaves the connection open, so the second goes out on a reused one.
  database.write(makePointWithAField());

  testing::internal::CaptureStderr();
  database.write(makePointWithAField());
  std::ignore = testing::internal::GetCapturedStderr();

  EXPECT_EQ(served.get(), 2) << "both batches should have arrived on the one connection";

  // A second connection would mean the slow answer was read as "never seen" and the batch resent.
  asio::ip::tcp::socket again {listenerIo};
  asio::error_code accepted = asio::error::would_block;
  acceptor.async_accept(again, [&accepted](const asio::error_code& error) { accepted = error; });
  listenerIo.restart();
  std::ignore = listenerIo.run_for(std::chrono::milliseconds {300});

  EXPECT_EQ(accepted, asio::error::would_block)
    << "a slow answer on a reused connection was taken for an unread request and the batch was resent";
}

/// @test
/// A batch Telegraf refuses leaves the connection usable. Its answer carries both the reason header
/// and a body, and the two have different lengths, so code that mistakes the header's length for a
/// count of body bytes asks the socket for bytes that are not coming -- a deadline burnt on the
/// recording cycle and the connection torn down, for every refused batch.
///
/// The assertion is that the second batch arrives on the same connection, which no existing test
/// noticed: all of them stop after one request.
TEST(InfluxHttpTransportTest, ARefusedBatchLeavesTheConnectionUsable)
{
  asio::io_context listenerIo;
  asio::ip::tcp::acceptor acceptor {listenerIo, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto port = acceptor.local_endpoint().port();

  // One connection, two requests, each answered with a reason header and a differently sized body.
  auto served = std::async(
    std::launch::async,
    [&listenerIo, &acceptor]
    {
      auto& context = listenerIo;

      asio::ip::tcp::socket peer {context};
      if (awaitBounded(context,
                       [&](auto&& handler) { acceptor.async_accept(peer, std::forward<decltype(handler)>(handler)); }))
      {
        return 0;
      }

      auto seen = 0;
      for (auto i = 0; i < 2; ++i)
      {
        asio::streambuf incoming;
        if (awaitBounded(context,
                         [&](auto&& handler)
                         {
                           asio::async_read_until(
                             peer, incoming, std::string {"\r\n\r\n"}, std::forward<decltype(handler)>(handler));
                         }))
        {
          break;
        }

        ++seen;

        // The body is longer than the header value, as Telegraf's own is.
        const std::string reason {"unable to parse 'cpu x': invalid field"};
        std::string body {R"({"error":")"};
        body.append(reason).append(R"("})");

        std::string answer {"HTTP/1.1 400 Bad Request\r\nX-Influxdb-Error: "};
        answer.append(reason)
          .append("\r\nContent-Length: ")
          .append(std::to_string(body.size()))
          .append("\r\n\r\n")
          .append(body);

        asio::error_code writing;
        std::ignore = asio::write(peer, asio::buffer(answer), writing);
        if (writing)
        {
          break;
        }
      }

      return seen;
    });

  Database database {std::make_unique<HttpTransport>("127.0.0.1", port, "/write")};

  testing::internal::CaptureStderr();
  database.write(makePointWithAField());
  database.write(makePointWithAField());
  std::ignore = testing::internal::GetCapturedStderr();

  EXPECT_EQ(served.get(), 2) << "the second batch did not arrive on the same connection, so a refused "
                                "batch is tearing the connection down";
}

/// @test
/// A request that went out whole and got no answer is dropped, not sent again. Telegraf may
/// already hold the batch, and for an output that counts what it receives a duplicate is worse
/// than a gap.
TEST(InfluxHttpTransportTest, AnUnansweredBatchIsNotSentAgain)
{
  asio::io_context listenerIo;
  asio::ip::tcp::acceptor acceptor {listenerIo, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto port = acceptor.local_endpoint().port();

  // Takes the request and closes without answering, which is the one case where the component
  // cannot tell acceptance from loss.
  auto received = std::async(
    std::launch::async,
    [&listenerIo, &acceptor]
    {
      auto& context = listenerIo;

      asio::ip::tcp::socket peer {context};
      if (awaitBounded(context,
                       [&](auto&& handler) { acceptor.async_accept(peer, std::forward<decltype(handler)>(handler)); }))
      {
        return std::string {};
      }

      asio::streambuf incoming;
      if (awaitBounded(context,
                       [&](auto&& handler)
                       {
                         asio::async_read_until(
                           peer, incoming, std::string {"\r\n\r\n"}, std::forward<decltype(handler)>(handler));
                       }))
      {
        return std::string {};
      }

      return std::string {asio::buffers_begin(incoming.data()), asio::buffers_end(incoming.data())};
    });

  Database database {std::make_unique<HttpTransport>("127.0.0.1", port, "/write")};

  testing::internal::CaptureStderr();
  database.write(makePointWithAField());
  const auto request = received.get();
  const auto reported = testing::internal::GetCapturedStderr();

  ASSERT_FALSE(request.empty()) << "the request never arrived, so this proves nothing";

  // A second connection would mean the batch went out twice.
  asio::ip::tcp::socket again {listenerIo};
  asio::error_code accepted = asio::error::would_block;
  acceptor.async_accept(again, [&accepted](const asio::error_code& error) { accepted = error; });
  listenerIo.restart();
  std::ignore = listenerIo.run_for(std::chrono::milliseconds {300});

  EXPECT_EQ(accepted, asio::error::would_block) << "the batch was sent a second time";
  EXPECT_NE(reported.find("twice"), std::string::npos) << "the drop was not reported: " << reported;
}

/// @test
/// An empty path becomes /write, which is what influxdb_listener serves, and a path given without
/// its leading slash gets one. Either way the alternative is a request line Go refuses with a 400,
/// which this transport would report as though Telegraf had judged the batch.
/// @test
/// The listener closing an idle keep-alive connection is the case a second attempt used to recover,
/// and `connectionUnusable()` is now the whole of what covers it: the probe reads the close before
/// the request is written, so the batch goes out on a fresh dial first time. Without the probe the
/// write lands in the local send buffer and succeeds, the read that follows fails, and the batch is
/// reported dropped -- once, with nothing behind it. This is the half of the argument for deleting
/// the retry that had no test.
///
/// The close is synchronised rather than slept on: the listener publishes it after `close()` has
/// returned and the second batch is not written until that is observed. What is left is the kernel
/// delivering the close over loopback, and the assertion is what measures it -- a close that had
/// not arrived would leave the transport reusing a dead socket and never dialling, so the second
/// accept would time out and `served` would be one.
TEST(InfluxHttpTransportTest, AConnectionTheListenerClosedWhileIdleCostsNoBatch)
{
  asio::io_context listenerIo;
  asio::ip::tcp::acceptor acceptor {listenerIo, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto port = acceptor.local_endpoint().port();

  std::promise<void> closed;
  auto closedFirst = closed.get_future();

  auto served = std::async(
    std::launch::async,
    [&listenerIo, &acceptor, &closed]
    {
      auto& context = listenerIo;
      auto seen = 0;

      // Two connections: the first answered and then dropped while idle, the second the one the
      // transport has to dial for itself.
      for (auto connection = 0; connection < 2; ++connection)
      {
        asio::ip::tcp::socket peer {context};
        if (awaitBounded(
              context, [&](auto&& handler) { acceptor.async_accept(peer, std::forward<decltype(handler)>(handler)); }))
        {
          break;
        }

        asio::streambuf incoming;
        if (awaitBounded(context,
                         [&](auto&& handler)
                         {
                           asio::async_read_until(
                             peer, incoming, std::string {"\r\n\r\n"}, std::forward<decltype(handler)>(handler));
                         }))
        {
          break;
        }

        ++seen;

        // No Connection: close, so nothing in the answer tells the transport this socket is
        // finished. The probe is the only way it can find out.
        asio::error_code writing;
        const std::string answer {"HTTP/1.1 204 No Content\r\n\r\n"};
        std::ignore = asio::write(peer, asio::buffer(answer), writing);
        if (writing)
        {
          break;
        }

        asio::error_code closing;
        std::ignore = peer.close(closing);

        if (connection == 0)
        {
          closed.set_value();
        }
      }

      return seen;
    });

  Database database {std::make_unique<HttpTransport>("127.0.0.1", port, "/write")};

  testing::internal::CaptureStderr();
  database.write(makePointWithAField());

  ASSERT_EQ(closedFirst.wait_for(std::chrono::seconds {5}), std::future_status::ready)
    << "the listener never closed the first connection, so there was nothing for the probe to find";

  database.write(makePointWithAField());
  const auto arrived = served.get();
  const auto reported = testing::internal::GetCapturedStderr();

  EXPECT_EQ(arrived, 2) << "the second batch never reached the listener: the transport wrote it into a connection "
                           "that had been closed, and with no retry behind it that batch is gone. "
                        << reported;
  EXPECT_TRUE(reported.empty()) << "two batches were accepted, so there was nothing to report: " << reported;
}

/// @test
/// A byte arriving unsolicited on an idle keep-alive connection must not cost the next batch. Go
/// sends nothing there, so the byte means the previous answer was mis-framed, and two mechanisms
/// between them make it harmless: `connectionUnusable`'s probe retires the connection when the byte
/// is still in the socket, and `readAnswer` consumes both buffers before each exchange when it has
/// already been drained into one.
///
/// The byte shares the answer's single write, so `async_read_until` takes it into `answerBuffer_`
/// with the field block and leaves the socket empty. Measured, by reporting the buffer's size at
/// each step: the buffer holds one byte after the headers are parsed, the probe reports the
/// connection usable, and it is reused.
///
/// **This test has no single-point red arm, and that is a property of the code rather than a gap
/// here.** What makes the byte harmless on this path is `bodyInHand`, which reads the buffer to its
/// end on every answer that completes; remove that and `readAnswer`'s consume clears it at the next
/// exchange instead, and remove the consume and `bodyInHand` has already taken it. Two independent
/// mechanisms give the same correct outcome, so no one plant can fail this, and an earlier version
/// of this comment claiming it pinned the consume was wrong -- the arm said so by passing. It is
/// kept as a regression test on the behaviour, with the two guards named so the next person does
/// not go looking for the arm it cannot have.
///
/// The consume's own guarantee is pinned by `AnAnswerThatCouldNotBeReadDoesNotPoisonTheNextConnection`,
/// and the probe's by `AConnectionTheListenerClosedWhileIdleCostsNoBatch`.
TEST(InfluxHttpTransportTest, AStaleByteOnAnIdleConnectionDoesNotCostTheNextBatch)
{
  asio::io_context listenerIo;
  asio::ip::tcp::acceptor acceptor {listenerIo, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto port = acceptor.local_endpoint().port();

  std::promise<void> strayed;
  auto straySent = strayed.get_future();

  auto served = std::async(
    std::launch::async,
    [&listenerIo, &acceptor, &strayed]
    {
      auto& context = listenerIo;
      auto seen = 0;

      asio::ip::tcp::socket peer {context};
      if (awaitBounded(context,
                       [&](auto&& handler) { acceptor.async_accept(peer, std::forward<decltype(handler)>(handler)); }))
      {
        return 0;
      }

      // The first answer carries a byte nothing asked for. The second request should arrive on this
      // same connection, since the socket is left empty and the probe therefore reports it usable;
      // the re-accept below is a safety net for the case where the byte reaches the socket after
      // all, which would retire the connection and is also correct.
      for (auto request = 0; request < 2; ++request)
      {
        if (request == 1 && !peer.is_open())
        {
          if (awaitBounded(context,
                           [&](auto&& handler)
                           { acceptor.async_accept(peer, std::forward<decltype(handler)>(handler)); }))
          {
            break;
          }
        }

        asio::streambuf incoming;
        if (awaitBounded(context,
                         [&](auto&& handler)
                         {
                           asio::async_read_until(
                             peer, incoming, std::string {"\r\n\r\n"}, std::forward<decltype(handler)>(handler));
                         }))
        {
          break;
        }

        ++seen;

        // The stray byte goes out in the *same* write as the answer, and that is the whole
        // arrangement. Sent as a second write it arrives after the transport has already drained
        // the answer, so it waits in the socket where the probe finds it, the connection is retired
        // and this test passes without the consume ever mattering -- measured, not assumed: with the
        // consume removed the earlier form of this test still passed. One write puts it in
        // answerBuffer_ behind the field block and leaves the socket empty, so the probe reports the
        // connection usable, it is reused, and the consume is the only thing standing between a
        // stale byte and the next status line.
        asio::error_code writing;
        const std::string answer = request == 0 ? std::string {"HTTP/1.1 204 No Content\r\n\r\nX"}
                                                : std::string {"HTTP/1.1 204 No Content\r\n\r\n"};
        std::ignore = asio::write(peer, asio::buffer(answer), writing);
        if (writing)
        {
          break;
        }

        if (request == 0)
        {
          strayed.set_value();
        }
      }

      return seen;
    });

  Database database {std::make_unique<HttpTransport>("127.0.0.1", port, "/write")};

  testing::internal::CaptureStderr();
  database.write(makePointWithAField());

  ASSERT_EQ(straySent.wait_for(std::chrono::seconds {5}), std::future_status::ready)
    << "the stray byte was never sent, so this exercises an ordinary reuse and not its subject";

  database.write(makePointWithAField());
  const auto arrived = served.get();
  const auto reported = testing::internal::GetCapturedStderr();

  EXPECT_EQ(arrived, 2) << "the second batch never reached the listener. " << reported;
  EXPECT_TRUE(reported.empty())
    << "a batch the listener answered was given up on, so a stale byte is being read as the head of an answer: "
    << reported;
}

/// @test
/// An answer this transport cannot read leaves bytes in `answerBuffer_` and closes the socket, and
/// the buffer outlives the socket: it is a member, so a freshly dialled connection inherits them
/// unless something clears them. `readAnswer` consumes both buffers before each exchange, which is
/// the only thing standing between a dead connection's leftovers and the head of the next answer.
/// Without it the transport reads "LEFTOVER" plus the new status line as one token, fails the same
/// way again, and goes on failing on every connection it dials for the rest of the run -- from one
/// bad answer, silently, with every batch afterwards reported as unanswered.
///
/// The seven early returns in `readAnswer` all share this shape and all close the socket; a status
/// line that does not begin with "HTTP/" is the cheapest of them to provoke.
///
/// The recovery line is the witness rather than the absence of a complaint: the first batch is
/// *expected* to be reported, so "nothing on stderr" would be wrong here. Telegraf answering again
/// after a batch that was not confirmed is the thing that only happens if the second exchange
/// parsed, and it names the count it recovered from.
TEST(InfluxHttpTransportTest, AnAnswerThatCouldNotBeReadDoesNotPoisonTheNextConnection)
{
  asio::io_context listenerIo;
  asio::ip::tcp::acceptor acceptor {listenerIo, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto port = acceptor.local_endpoint().port();

  auto served = std::async(
    std::launch::async,
    [&listenerIo, &acceptor]
    {
      auto& context = listenerIo;
      auto seen = 0;

      for (auto connection = 0; connection < 2; ++connection)
      {
        asio::ip::tcp::socket peer {context};
        if (awaitBounded(
              context, [&](auto&& handler) { acceptor.async_accept(peer, std::forward<decltype(handler)>(handler)); }))
        {
          break;
        }

        asio::streambuf incoming;
        if (awaitBounded(context,
                         [&](auto&& handler)
                         {
                           asio::async_read_until(
                             peer, incoming, std::string {"\r\n\r\n"}, std::forward<decltype(handler)>(handler));
                         }))
        {
          break;
        }

        ++seen;

        // The first answer has a complete field block and an unreadable status line, with bytes
        // after the blank line. That is what reaches the early return with the buffer non-empty:
        // the version token does not begin with "HTTP/", so readAnswer gives up before bodyInHand
        // would have drained what is left.
        asio::error_code writing;
        const std::string answer = connection == 0 ? std::string {"GARBAGE 200 OK\r\n\r\nLEFTOVER"}
                                                   : std::string {"HTTP/1.1 204 No Content\r\n\r\n"};
        std::ignore = asio::write(peer, asio::buffer(answer), writing);
        if (writing)
        {
          break;
        }
      }

      return seen;
    });

  Database database {std::make_unique<HttpTransport>("127.0.0.1", port, "/write")};

  testing::internal::CaptureStderr();
  database.write(makePointWithAField());
  database.write(makePointWithAField());
  const auto arrived = served.get();
  const auto reported = testing::internal::GetCapturedStderr();

  EXPECT_EQ(arrived, 2) << "the second batch never reached the listener. " << reported;
  EXPECT_NE(reported.find("answering again"), std::string::npos)
    << "the second batch was not confirmed either, so the first answer's leftovers are being read as the head of "
       "the second on a connection that never carried them: "
    << reported;
}

TEST(InfluxHttpTransportTest, AnEmptyPathBecomesTheWriteEndpoint)
{
  asio::io_context listenerIo;
  asio::ip::tcp::acceptor acceptor {listenerIo, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto port = acceptor.local_endpoint().port();
  auto served = serveInBackground(listenerIo, acceptor, "HTTP/1.1 204 No Content");

  Database database {std::make_unique<HttpTransport>("127.0.0.1", port, "")};
  database.write(makePointWithAField());

  const auto request = served.get();

  ASSERT_FALSE(request.empty());
  EXPECT_EQ(request.rfind("POST /write HTTP/1.1", 0U), 0U) << request;

  // And a configured path missing its slash gets one, rather than sending a target Go refuses
  // with a 400 that would read as a complaint about the batch.
  asio::io_context secondIo;
  asio::ip::tcp::acceptor second {secondIo, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto secondPort = second.local_endpoint().port();
  auto servedSecond = serveInBackground(secondIo, second, "HTTP/1.1 204 No Content");

  Database slashless {std::make_unique<HttpTransport>("127.0.0.1", secondPort, "telegraf")};
  slashless.write(makePointWithAField());

  const auto secondRequest = servedSecond.get();

  ASSERT_FALSE(secondRequest.empty());
  EXPECT_EQ(secondRequest.rfind("POST /telegraf HTTP/1.1", 0U), 0U) << secondRequest;
}

/// @test
/// Nothing listening is not a failure: the transport is built, the send reports once, and the
/// component carries on. Telegraf starting after Sen is ordinary.
TEST(InfluxHttpTransportTest, NothingListeningDoesNotStopTheComponent)
{
  asio::io_context io;

  // A port nothing is on: bound and closed, so the number is known to be free.
  asio::ip::tcp::acceptor probe {io, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto port = probe.local_endpoint().port();
  asio::error_code error;
  std::ignore = probe.close(error);

  EXPECT_NO_THROW({
    Database database {std::make_unique<HttpTransport>("127.0.0.1", port, "/write")};
    database.write(makePointWithAField());
    database.write(makePointWithAField());
  });
}

/// @test
/// A datagram needs nobody listening to be sent, so a closed port is not a failure. The send used
/// to throw on the ICMP answer a closed port gives, which ended the component for the ordinary
/// case of Telegraf not being up.
TEST(InfluxUdpTransportTest, NothingListeningDoesNotStopTheComponent)
{
  asio::io_context io;

  // A port nothing is on: bound and closed, so the number is known to be free.
  asio::ip::udp::socket probe {io, asio::ip::udp::endpoint {asio::ip::udp::v4(), 0}};
  const auto port = probe.local_endpoint().port();
  asio::error_code error;
  std::ignore = probe.close(error);

  EXPECT_NO_THROW({
    Database database {std::make_unique<UDP>(io, "127.0.0.1", port, IpFamily::v4)};

    // More than one: the error a closed port causes arrives on a later send, not the first.
    for (int i = 0; i < 5; ++i)
    {
      database.write(makePointWithAField());
    }
  });
}

/// @test
/// A stream transport is built before anything listens, and sends once a listener appears. The
/// constructor used to dial, so a Telegraf started after Sen stopped the component.
TEST(InfluxTcpTransportTest, AListenerThatAppearsLaterIsUsed)
{
  asio::io_context io;
  asio::ip::tcp::acceptor acceptor {io, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), 0}};
  const auto port = acceptor.local_endpoint().port();

  asio::error_code error;
  std::ignore = acceptor.close(error);

  Database database {std::make_unique<TCP>(io, "127.0.0.1", port, IpFamily::v4)};
  EXPECT_NO_THROW(database.write(makePointWithAField()));

  // Now a listener on the same port, and the next write reaches it.
  asio::ip::tcp::acceptor listener {io, asio::ip::tcp::endpoint {asio::ip::tcp::v4(), port}};
  database.write(makePointWithAField());

  asio::ip::tcp::socket peer {io};
  const auto acceptError =
    awaitBounded(io, [&](auto&& handler) { listener.async_accept(peer, std::forward<decltype(handler)>(handler)); });
  ASSERT_FALSE(acceptError) << acceptError.message();

  asio::streambuf incoming;
  const auto readError = awaitBounded(
    io,
    [&](auto&& handler) { asio::async_read_until(peer, incoming, '\n', std::forward<decltype(handler)>(handler)); });
  ASSERT_FALSE(readError) << readError.message();

  const std::string message {asio::buffers_begin(incoming.data()), asio::buffers_end(incoming.data())};
  EXPECT_NE(message.find("cpu,host=a value=7i"), std::string::npos) << message;
}

/// @test
/// A configuration written before `family` existed keeps the behaviour it had. An absent field is
/// value-initialised, so what this really pins is `any` being the first enumerator: ordering it
/// after v4 would make every such configuration silently resolve v4-only, with no error anywhere.
TEST(InfluxTransportSpecTest, AConfigurationThatNamesNoFamilyResolvesInAny)
{
  const sen::components::influx::Tcp tcp {};
  const sen::components::influx::Udp udp {};

  EXPECT_EQ(tcp.family, IpFamily::any) << "an existing Tcp configuration changed meaning";
  EXPECT_EQ(udp.family, IpFamily::any) << "an existing Udp configuration changed meaning";
}

/// @test
/// The ceiling a datagram bound is measured against depends on the family, and the unspecified
/// form has to take the smaller of the two because it may resolve to either. What this guards is
/// the IPv4 figure being copied onto the IPv6 alternative, which reads as working configuration
/// and which nothing outside the component would notice.
TEST(InfluxDatagramCeilingTest, TheFamilyDecidesTheCeilingAndTheUnspecifiedFormTakesTheSmaller)
{
  EXPECT_EQ(datagramCeilingFor(IpFamily::v4), 65507U);
  EXPECT_EQ(datagramCeilingFor(IpFamily::v6), 65527U);

  EXPECT_GT(datagramCeilingFor(IpFamily::v6), datagramCeilingFor(IpFamily::v4))
    << "an IPv6 datagram carries more than an IPv4 one, so the two cannot be the same figure";

  EXPECT_EQ(datagramCeilingFor(IpFamily::any), datagramCeilingFor(IpFamily::v4))
    << "the unspecified form may resolve to IPv4, so it cannot be held to the larger ceiling";
}

/// @test
/// The family hint reaches the resolver and changes the outcome: an address of one family cannot be
/// resolved as the other. Without this, a v4 and v6 swap in the transport visitor would read as a
/// working configuration. No listener is involved and TCP opens no socket here.
TEST(InfluxIpFamilyTest, AFamilyHintRefusesAnAddressOfTheOtherFamily)
{
  asio::io_context io;

  // Through a helper, so the commas do not split the macro's arguments.
  const auto resolveAs = [&io](const char* host, IpFamily family)
  { return std::make_unique<TCP>(io, host, 18099, family); };

  EXPECT_NO_THROW(resolveAs("127.0.0.1", IpFamily::v4));
  EXPECT_NO_THROW(resolveAs("127.0.0.1", IpFamily::any));
  EXPECT_THROW(resolveAs("127.0.0.1", IpFamily::v6), std::exception)
    << "an IPv4 literal was accepted as IPv6, so the family hint is not reaching the resolver";

  EXPECT_NO_THROW(resolveAs("::1", IpFamily::v6));
  EXPECT_THROW(resolveAs("::1", IpFamily::v4), std::exception)
    << "an IPv6 literal was accepted as IPv4, so the family hint is not reaching the resolver";
}

/// @test
/// The bound put on the batch equals the bound on one message only when nothing compresses it
/// afterwards. This is the decision itself rather than its effect, because the effect is a
/// difference in packet counts that no unit test observes.
TEST(InfluxBatchBoundTest, CompressionRaisesTheBatchBoundAndZeroStaysUnbounded)
{
  // Uncompressed, the lines are the payload, so the two bounds are the same number.
  EXPECT_EQ(batchBoundFor(1472U, false), 1472U);
  EXPECT_EQ(batchBoundFor(65507U, false), 65507U);

  // Compressed, the batch is allowed to grow past one message, since it will shrink.
  EXPECT_GT(batchBoundFor(1472U, true), 1472U);
  EXPECT_EQ(batchBoundFor(1472U, true), 1472U * 16U);

  // Zero means unbounded either way, and must not become a bound by being multiplied.
  EXPECT_EQ(batchBoundFor(0U, false), 0U);
  EXPECT_EQ(batchBoundFor(0U, true), 0U);

  // A bound near the top of the range must not wrap into a small one.
  EXPECT_GE(batchBoundFor(std::numeric_limits<std::size_t>::max(), true), std::numeric_limits<std::size_t>::max() / 2U);
  EXPECT_GE(batchBoundFor(std::numeric_limits<std::size_t>::max() / 4U, true),
            std::numeric_limits<std::size_t>::max() / 4U);
}

/// @test
/// The component holds a compressing batch to sixteen times the message bound, on the grounds that
/// Sen's line protocol compresses by more than that: the same measurement and tags repeat with only
/// the value changing. This records the measurement the constant rests on, so a change in what Sen
/// emits that invalidated it fails here rather than quietly costing packets.
///
/// Asserted only where the cap governs. At 32 points the batch flushes on point count long before
/// any byte cap is near, so that ratio is recorded and not asserted.
TEST(InfluxCompressionRatioTest, SenLineProtocolCompressesPastTheBatchMultiple)
{
  // The multiple component.cpp uses. The transport has to split a compressed payload only where
  // this exceeds the real ratio.
  constexpr auto batchBoundMultiple = 16.0;

  for (const std::size_t points: {32U, 256U, 2048U})
  {
    std::vector<std::string> plain;
    std::vector<std::string> gzipped;

    for (const bool compressed: {false, true})
    {
      auto transport = std::make_unique<RecordingTransport>(compressed ? gzipped : plain);
      if (compressed)
      {
        transport->setCompressionEnabled(true, Z_DEFAULT_COMPRESSION);
      }

      Database database {std::move(transport)};
      database.batchOf(points);

      for (std::size_t i = 0; i < points; ++i)
      {
        DataPoint point = makePoint("my.tutorial.SomeClass.someProperty");
        point.addTag("object", "instance_" + std::to_string(i % 8U));
        point.addTag("payload_type", "property");
        point.addField("value", DataPoint::FieldValue {static_cast<std::int32_t>(i)});
        database.write(std::move(point));
      }
      database.flushBatch();
    }

    ASSERT_EQ(plain.size(), 1U);
    ASSERT_EQ(gzipped.size(), 1U);

    const auto ratio = static_cast<double>(plain.front().size()) / static_cast<double>(gzipped.front().size());

    // Measured when the constant was chosen: 13.6 over 32 points, 27.4 over 256, 38.1 over 2048.
    // Loosely for every size, because 32 points do not reach the multiple and never did; tightly
    // from 256 up, which is where the constant has to hold.
    EXPECT_GT(ratio, batchBoundMultiple * 0.75)
      << points << " points compressed only " << ratio << "x, far enough under the batch multiple of "
      << batchBoundMultiple << " to suggest Sen's line protocol has changed shape";

    if (points >= 256U)
    {
      EXPECT_GT(ratio, batchBoundMultiple)
        << points << " points compressed only " << ratio << "x, under the batch multiple of " << batchBoundMultiple
        << ", so the batch now costs recursion in Transport::sendHalves";
    }
  }
}

//----------------------------------------------------------------------------------------------------------------------
// IoThread
//----------------------------------------------------------------------------------------------------------------------

/// Long enough that a loaded machine does not fail a working test, short enough that a broken one
/// is a failure rather than a stall.
constexpr auto ioThreadWait = std::chrono::seconds {5};

/// @test
/// Work posted after a stretch with nothing to do still runs, on the same thread as the work
/// before it. The idle below is load-bearing, not decoration: the first future becomes ready
/// inside the handler, so only the wait that follows it leaves the work count settled at zero.
TEST(InfluxIoThreadTest, WorkPostedAfterAnIdleStretchStillRuns)
{
  asio::io_context io;
  std::promise<std::thread::id> first;
  std::promise<std::thread::id> second;
  auto ranFirst = first.get_future();
  auto ranSecond = second.get_future();

  {
    const IoThread sending {io};

    asio::post(io, [&first]() { first.set_value(std::this_thread::get_id()); });

    // Asserted rather than expected: the reads below would block for ever on a future that never
    // became ready, and a hang says far less than a failure does.
    ASSERT_EQ(ranFirst.wait_for(ioThreadWait), std::future_status::ready)
      << "the thread never served the first handler at all";

    // Do not remove this. Without it the second post can land inside the window where a missing
    // work guard has not bitten yet, and the test passes against a context with no guard at all.
    std::this_thread::sleep_for(std::chrono::milliseconds {200});

    asio::post(io, [&second]() { second.set_value(std::this_thread::get_id()); });

    ASSERT_EQ(ranSecond.wait_for(ioThreadWait), std::future_status::ready)
      << "the thread served one handler and then stopped serving the context";
  }

  const auto whereFirst = ranFirst.get();
  const auto whereSecond = ranSecond.get();

  EXPECT_EQ(whereFirst, whereSecond);
  EXPECT_NE(whereSecond, std::this_thread::get_id());
}

/// @test
/// A handler that throws does not end the thread; letting it out reaches std::terminate. That it
/// is also reported goes unchecked, because no sink in this binary can be read back.
TEST(InfluxIoThreadTest, AHandlerThatThrowsDoesNotTakeTheThreadWithIt)
{
  asio::io_context io;
  std::promise<void> ran;
  auto hasRun = ran.get_future();

  {
    const IoThread sending {io};

    asio::post(io, []() { throw std::runtime_error("a handler that throws"); });
    // Not derived from std::exception, so the second catch arm is the one that takes it. Without
    // this the arm whose only job is to stop std::terminate is itself never reached.
    asio::post(io, []() { throw 42; });  // NOLINT(hicpp-exception-baseclass): that is the point
    asio::post(io, [&ran]() { ran.set_value(); });

    EXPECT_EQ(hasRun.wait_for(ioThreadWait), std::future_status::ready)
      << "the work posted after a throwing handler never ran";
  }
}

/// @test
/// The destructor leaves work that is still queued unrun, waits for the one already running, and
/// returns. The elapsed time is asserted at both ends, since each bound alone is satisfied by a
/// different broken destructor.
TEST(InfluxIoThreadTest, DestructionLeavesQueuedWorkUnrunAndStillReturns)
{
  // The first handler holds the thread while the destructor waits for it. The second would hold it
  // far longer, so the elapsed time tells waiting for one apart from draining both.
  constexpr auto heldFor = std::chrono::milliseconds {300};
  constexpr auto farLonger = std::chrono::milliseconds {2000};

  asio::io_context io;
  std::promise<void> entered;
  auto hasEntered = entered.get_future();
  const auto queuedRan = std::make_shared<std::atomic<bool>>(false);
  // Shared rather than captured by reference, so a destructor that failed to wait leaves a test
  // that fails rather than one that writes to a dead frame.
  const auto ranToTheEnd = std::make_shared<std::atomic<bool>>(false);

  std::optional<IoThread> sending;
  sending.emplace(io);

  asio::post(io,
             [&entered, ranToTheEnd, heldFor]()
             {
               entered.set_value();
               std::this_thread::sleep_for(heldFor);
               ranToTheEnd->store(true);
             });
  asio::post(io,
             [queuedRan, farLonger]()
             {
               queuedRan->store(true);
               std::this_thread::sleep_for(farLonger);
             });

  ASSERT_EQ(hasEntered.wait_for(ioThreadWait), std::future_status::ready);

  // Destroyed on another thread so that a destructor which never returns is named in the log
  // before the test stalls, rather than only stalling. It does still stall: the future blocks in
  // its own destructor, and that is what keeps the captured reference below valid meanwhile.
  const auto before = std::chrono::steady_clock::now();
  auto shutdown = std::async(std::launch::async, [&sending]() { sending.reset(); });
  ASSERT_EQ(shutdown.wait_for(ioThreadWait), std::future_status::ready) << "the destructor did not return";
  const auto elapsed = std::chrono::steady_clock::now() - before;

  EXPECT_FALSE(queuedRan->load()) << "the destructor ran work that was still queued";
  EXPECT_LT(elapsed, farLonger) << "the destructor waited for work it was supposed to leave";
  // Both bounds, because the upper one alone is satisfied by a destructor that waited for nothing:
  // detaching rather than joining passes the two above at no elapsed time at all.
  EXPECT_GE(elapsed, heldFor) << "the destructor returned without waiting for the running handler";
  // The same property observed rather than timed, which no coincidence of scheduling can satisfy.
  EXPECT_TRUE(ranToTheEnd->load()) << "the destructor returned while a handler was still running";
}

//----------------------------------------------------------------------------------------------------------------------
// The sending queue
//----------------------------------------------------------------------------------------------------------------------

/// A transport a test can hold mid-send: it records what arrived and on which thread, and each
/// send waits for release() so the queue behind it can be filled on purpose.
class HeldTransport final: public Transport
{
public:
  void holdSends()
  {
    const std::scoped_lock held {guard_};
    holding_ = true;
  }

  void release()
  {
    const std::scoped_lock held {guard_};
    holding_ = false;
    changed_.notify_all();
  }

  /// Makes every send take this long, so a shutdown that waits for a backlog is slower than one
  /// that waits for the message in hand.
  void delayEachSend(std::chrono::milliseconds each)
  {
    const std::scoped_lock held {guard_};
    delay_ = each;
  }

  /// True once at least this many sends have arrived.
  [[nodiscard]] bool awaitArrivals(std::size_t count)
  {
    std::unique_lock held {guard_};
    return changed_.wait_for(held, ioThreadWait, [&]() { return arrived_.size() >= count; });
  }

  /// The measurement name of each arrival, which is all these tests need to tell them apart.
  [[nodiscard]] std::vector<std::string> names() const
  {
    const std::scoped_lock held {guard_};

    std::vector<std::string> result;
    result.reserve(arrived_.size());
    for (const auto& line: arrived_)
    {
      result.push_back(line.substr(0U, line.find(' ')));
    }

    return result;
  }

  [[nodiscard]] std::vector<std::thread::id> threads() const
  {
    const std::scoped_lock held {guard_};
    return threads_;
  }

protected:
  void doSend(sen::Span<const char> message) override
  {
    std::chrono::milliseconds waitAfter {0};

    {
      std::unique_lock held {guard_};
      arrived_.emplace_back(message.data(), message.size());
      threads_.push_back(std::this_thread::get_id());
      waitAfter = delay_;
      changed_.notify_all();
      std::ignore = changed_.wait_for(held, ioThreadWait, [this]() { return !holding_; });
    }

    if (waitAfter.count() != 0)
    {
      std::this_thread::sleep_for(waitAfter);
    }
  }

private:
  mutable std::mutex guard_;
  std::condition_variable changed_;
  std::vector<std::string> arrived_;
  std::vector<std::thread::id> threads_;
  std::chrono::milliseconds delay_ {0};
  bool holding_ = false;
};

/// @test
/// Everything written below the bound arrives, once each and in the order it was written.
TEST(InfluxSendingQueueTest, EveryMessageBelowTheBoundArrivesInOrder)
{
  constexpr std::size_t written = 20U;

  asio::io_context io;
  auto transport = std::make_shared<HeldTransport>();

  Database database {transport};
  database.sendThrough(io, 64U);

  std::vector<std::string> expected;
  {
    const IoThread sending {io};

    for (std::size_t i = 0U; i < written; ++i)
    {
      expected.push_back("m" + std::to_string(i));
      database.write(makePoint(expected.back()));
    }

    ASSERT_TRUE(transport->awaitArrivals(written)) << "only " << transport->names().size() << " arrived";
  }

  EXPECT_EQ(transport->names(), expected);
  EXPECT_EQ(database.droppedSoFar(), 0U);

  // Every one, not just the first: a defect that sent the first inline and posted the rest would
  // pass a test that only looked at one.
  for (const auto& where: transport->threads())
  {
    EXPECT_NE(where, std::this_thread::get_id()) << "a send ran on the thread that wrote the point";
  }
}

/// @test
/// At the bound the oldest waiting message goes, not the newest. The transport is held mid-send so
/// the queue is genuinely full when the last write lands.
TEST(InfluxSendingQueueTest, AFullQueueDropsTheOldestWaitingMessage)
{
  asio::io_context io;
  auto transport = std::make_shared<HeldTransport>();
  transport->holdSends();

  Database database {transport};
  database.sendThrough(io, 2U);

  {
    const IoThread sending {io};

    database.write(makePoint("first"));
    // Held inside doSend now, so nothing drains while the next three are written.
    ASSERT_TRUE(transport->awaitArrivals(1U)) << "the first send never started";

    database.write(makePoint("oldest"));
    database.write(makePoint("middle"));
    database.write(makePoint("newest"));

    EXPECT_EQ(database.droppedSoFar(), 1U) << "the bound cost nothing, so it did not hold";

    transport->release();

    ASSERT_TRUE(transport->awaitArrivals(3U)) << "the queue did not drain after the hold was lifted";
  }

  const std::vector<std::string> expected {"first", "middle", "newest"};
  EXPECT_EQ(transport->names(), expected) << "the wrong message was dropped";
  // The queue emptied on the way out, which is when the count clears and the recovery is said.
  EXPECT_EQ(database.droppedSoFar(), 0U) << "the drop count survived the queue emptying";
}

/// @test
/// Without sendThrough the send still happens inline, on the calling thread. This is the shape
/// every other test in this file uses, so it is stated once rather than assumed.
TEST(InfluxSendingQueueTest, WithoutAQueueTheSendIsOnTheCallingThread)
{
  auto transport = std::make_shared<HeldTransport>();
  Database database {transport};

  database.write(makePoint("m1"));

  const std::vector<std::string> expected {"m1"};
  EXPECT_EQ(transport->names(), expected);
  ASSERT_EQ(transport->threads().size(), 1U);
  EXPECT_EQ(transport->threads().front(), std::this_thread::get_id());
  EXPECT_EQ(database.droppedSoFar(), 0U);
}

/// @test
/// A shutdown while the transport is slow waits for the message in hand, not for everything behind
/// it. One drain is posted per burst and re-posted per message, so stopping the context ends the
/// chain; posting one handler per point instead puts the whole backlog in the strand's ready queue,
/// which asio runs to completion without re-reading its stopped flag.
TEST(InfluxSendingQueueTest, ShutdownWaitsForTheMessageInHandNotTheBacklog)
{
  constexpr auto eachSend = std::chrono::milliseconds {200};
  constexpr std::size_t written = 12U;
  // Two and a half times one send, and less than half of all of them.
  constexpr auto allowed = std::chrono::milliseconds {1000};

  asio::io_context io;
  auto transport = std::make_shared<HeldTransport>();
  transport->delayEachSend(eachSend);

  Database database {transport};
  database.sendThrough(io, 64U);

  std::optional<IoThread> sending;
  sending.emplace(io);

  for (std::size_t i = 0U; i < written; ++i)
  {
    database.write(makePoint("m" + std::to_string(i)));
  }

  // Two, not one. A strand holds everything behind the running handler in its waiting queue and
  // moves the lot to its ready queue only when that handler's invoker exits. Stopping during the
  // first send therefore proves nothing: the backlog has not reached the queue that gets run
  // without a stop check. By the second arrival it has.
  ASSERT_TRUE(transport->awaitArrivals(2U)) << "the sends never got going";

  const auto before = std::chrono::steady_clock::now();
  auto shutdown = std::async(std::launch::async, [&sending]() { sending.reset(); });
  ASSERT_EQ(shutdown.wait_for(ioThreadWait), std::future_status::ready) << "the shutdown did not finish";
  const auto elapsed = std::chrono::steady_clock::now() - before;

  EXPECT_LT(elapsed, allowed) << "the shutdown waited for " << transport->names().size() << " of " << written
                              << " messages instead of the one in hand";
}

//----------------------------------------------------------------------------------------------------------------------
// The bounded stream write
//----------------------------------------------------------------------------------------------------------------------

#if defined(ASIO_HAS_POSIX_STREAM_DESCRIPTOR)

/// @test
/// A write to something that is not being read gives up on its deadline and says how much it
/// handed over. A pipe stands in for a peer that has stopped reading: the buffer fills, and asio's
/// own synchronous write would then poll with no deadline at all, which is the whole reason this
/// function exists.
TEST(InfluxBoundedWriteTest, AStalledReaderCostsTheDeadlineAndNotTheWholeMessage)
{
  constexpr auto bound = std::chrono::milliseconds {50};
  // Larger than any pipe buffer, so the write cannot finish without the read end draining.
  constexpr std::size_t tooMuch = 4U * 1024U * 1024U;

  std::array<int, 2> ends {-1, -1};
  ASSERT_EQ(::pipe(ends.data()), 0) << "could not make a pipe";
  // The descriptor below takes the write end and closes it; the read end is ours, and is
  // deliberately never read from.
  const auto closeReadEnd = sen::makeScopeGuard([&ends]() { std::ignore = ::close(ends[0]); });

  asio::io_context io;
  asio::posix::stream_descriptor writing {io, ends[1]};

  asio::error_code setting;
  std::ignore = writing.non_blocking(true, setting);
  ASSERT_FALSE(setting) << "could not put the pipe in non-blocking mode, so asio would swallow the deadline";

  const std::vector<char> message(tooMuch, 'x');

  asio::error_code error;
  const auto before = std::chrono::steady_clock::now();
  const auto sent = sen::components::influx::writeBounded(writing, sen::makeConstSpan(message), bound, error);
  const auto elapsed = std::chrono::steady_clock::now() - before;

  EXPECT_EQ(error, asio::error::timed_out) << "gave up for some other reason than the deadline";
  EXPECT_GT(sent, 0U) << "nothing reached the pipe at all, so the bound was not what stopped it";
  EXPECT_LT(sent, message.size()) << "the whole message went, so the reader was not actually stalled";
  EXPECT_LT(elapsed, std::chrono::seconds {2}) << "it waited well past its own deadline";
}

#endif  // defined(ASIO_HAS_POSIX_STREAM_DESCRIPTOR)

// NOLINTEND(misc-include-cleaner)

}  // namespace influx::test
