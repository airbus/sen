// === database.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "database.h"

#include "data_point.h"
#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/scope_guard.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"

// asio
#include <asio/io_context.hpp>
#include <asio/post.hpp>

// zlib
#include <zconf.h>
#include <zlib.h>

// std
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace sen::components::influx
{

void Transport::setCompressionEnabled(bool enabled, int level)
{
  // deflateInit2 refuses anything outside this range, and would do so on every send rather than
  // once here.
  if (enabled && (level < Z_DEFAULT_COMPRESSION || level > Z_BEST_COMPRESSION))
  {
    throw std::runtime_error("gzip compression level " + std::to_string(level) + " is outside -1 to 9");
  }

  compressionEnabled_ = enabled;
  compressionLevel_ = level;
}

bool Transport::getCompressionEnabled() const noexcept { return compressionEnabled_; }

std::vector<char> Transport::compressGzip(std::string& str, int compressionLevel)
{
  // zlib takes the input length as a 32-bit count, so a larger batch would be silently truncated.
  if (str.size() > std::numeric_limits<uInt>::max())
  {
    throw std::runtime_error("batch too large to compress");
  }

  z_stream zs = {};

  // windowBits = 15 + 16 forces zlib to write a standard gzip header/trailer
  if (deflateInit2(&zs, compressionLevel, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
  {
    throw std::runtime_error("deflateInit2 failed while initializing gzip.");
  }

  // Appending a block can throw, and the stream owns heap of its own until it is ended.
  const auto endStream = sen::makeScopeGuard([&zs]() { deflateEnd(&zs); });

  zs.next_in = reinterpret_cast<Bytef*>(str.data());  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
  zs.avail_in = static_cast<uInt>(str.size());

  constexpr std::size_t maxBufferSize = 32768;

  int ret = Z_OK;
  std::vector<char> outBuffer(maxBufferSize);
  std::vector<char> outputData;

  // Retrieve the compressed data blocks
  do
  {
    zs.next_out = reinterpret_cast<Bytef*>(outBuffer.data());  // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
    zs.avail_out = static_cast<uInt>(outBuffer.size());

    ret = deflate(&zs, Z_FINISH);

    const auto produced = outBuffer.size() - zs.avail_out;
    outputData.insert(
      outputData.end(), outBuffer.begin(), std::next(outBuffer.begin(), static_cast<std::ptrdiff_t>(produced)));
  } while (ret == Z_OK);

  if (ret != Z_STREAM_END)
  {
    throw std::runtime_error("Exception occurred while compressing to gzip");
  }

  return outputData;
}

void Transport::send(std::string&& message) { sendLines(std::move(message)); }

void Transport::sendLines(std::string lines)
{
  // The separator belongs to the line protocol, so it is added whether or not what follows is
  // compressed. It is appended in place, which is why this takes the string and not a view.
  const auto lineBytes = lines.size();
  if (requiresLineSeparator())
  {
    lines.append("\n");
  }

  if (!compressionEnabled_)
  {
    if (maxMessageBytes_ != 0U && lines.size() > maxMessageBytes_)
    {
      sendHalves(std::string_view {lines}.substr(0U, lineBytes));
      return;
    }

    doSend(makeConstSpan(lines.data(), lines.size()));
    return;
  }

  std::vector<char> payload;

  // zlib refusing a message is not a reason to end the recording. This runs on the kernel's work
  // queue, where an exception costs the rest of the batch and usually the process.
  try
  {
    payload = compressGzip(lines, compressionLevel_);
  }
  catch (const std::exception& error)
  {
    if (!compressionFailed_)
    {
      getLogger()->error("a message could not be compressed and is dropped: {}", error.what());
      compressionFailed_ = true;
    }

    return;
  }

  compressionFailed_ = false;

  // Compression only reveals the size once it is done, so the bound is checked here rather than
  // on the way in, and a payload over it is cut at a line boundary and compressed again.
  if (maxMessageBytes_ != 0U && payload.size() > maxMessageBytes_)
  {
    sendHalves(std::string_view {lines}.substr(0U, lineBytes));
    return;
  }

  doSend(makeConstSpan(payload));
}

void Transport::sendHalves(std::string_view lines)
{
  // The first line boundary past the midpoint: a message has to be whole lines.
  const auto middle = lines.find('\n', lines.size() / 2U);

  if (middle == std::string_view::npos)
  {
    // One line that does not fit cannot be sent at all, so name it here.
    getLogger()->error(
      "a single line of {} bytes does not fit the {} byte message limit, dropping it", lines.size(), maxMessageBytes_);
    return;
  }

  sendLines(std::string {lines.substr(0U, middle)});
  sendLines(std::string {lines.substr(middle + 1U)});
}

//----------------------------------------------------------------------------------------------------------------------
// Database
//----------------------------------------------------------------------------------------------------------------------

void Database::sendThrough(asio::io_context& io, std::size_t bound)
{
  SEN_ASSERT(bound != 0U);

  waiting_ = std::make_shared<Waiting>(io, bound);
}

std::size_t Database::droppedSoFar() const
{
  if (!waiting_)
  {
    return 0U;
  }

  const std::scoped_lock held {waiting_->guard};

  return waiting_->dropped;
}

void Database::transmit(std::string&& point)
{
  if (!waiting_)
  {
    transport_->send(std::move(point));
    return;
  }

  std::size_t dropsToName = 0U;
  bool startDraining = false;

  {
    const std::scoped_lock held {waiting_->guard};

    if (waiting_->messages.size() >= waiting_->bound)
    {
      waiting_->messages.pop_front();
      ++waiting_->dropped;

      constexpr std::size_t everySoOften = 100U;
      if (waiting_->dropped == 1U || waiting_->dropped % everySoOften == 0U)
      {
        dropsToName = waiting_->dropped;
      }
    }

    waiting_->messages.emplace_back(std::move(point));

    // A drain already in flight will take this message too, so only the first of a burst posts.
    startDraining = !waiting_->draining;
    waiting_->draining = true;
  }

  // On this thread on purpose, and the only thing here that is. The drain is what would otherwise
  // carry it, and the drain is not running at the moment this matters. Outside the lock, so a
  // blocked sink holds neither the queue nor the sending thread.
  if (dropsToName != 0U)
  {
    getLogger()->warn("the sending queue is full, so the oldest points are dropped; {} dropped so far", dropsToName);
  }

  if (startDraining)
  {
    asio::post(waiting_->sending, [waiting = waiting_, transport = transport_] { sendOneWaiting(waiting, transport); });
  }
}

void Database::sendOneWaiting(const std::shared_ptr<Waiting>& waiting, const std::shared_ptr<Transport>& transport)
{
  std::string message;
  std::size_t recovered = 0U;

  {
    const std::scoped_lock held {waiting->guard};

    if (waiting->messages.empty())
    {
      waiting->draining = false;
      return;
    }

    message = std::move(waiting->messages.front());
    waiting->messages.pop_front();

    // Said once the queue has actually emptied rather than on each send. A consumer that is slow
    // but moving would otherwise clear the count between drops and report every one of them.
    if (waiting->messages.empty())
    {
      recovered = std::exchange(waiting->dropped, std::size_t {0U});
    }
  }

  // Lodged before the send rather than after it. send() can throw -- getOrCreateLogger rethrows
  // spdlog_ex, and the HTTP answer parsing can throw out_of_range -- and a throw past this point
  // would leave the drain latched with a queue nobody comes back for.
  asio::post(waiting->sending, [waiting, transport] { sendOneWaiting(waiting, transport); });

  if (recovered != 0U)
  {
    getLogger()->info("the sending queue has emptied, after {} points were dropped", recovered);
  }

  transport->send(std::move(message));
}

std::string Database::joinLineProtocolBatch() const
{
  std::string joinedBatch;
  joinedBatch.reserve(batchBytes_);

  for (const auto& line: lineBatch_)
  {
    if (!joinedBatch.empty())
    {
      joinedBatch.append("\n");
    }

    joinedBatch.append(line);
  }

  return joinedBatch;
}

std::string Database::formatTags(const DataPoint& point)
{
  if (point.getTagSet().empty())
  {
    return "";
  }

  bool addComma = false;
  std::string result;
  result.reserve(point.getTagSet().size() * 32);

  for (const auto& [first, second]: point.getTagSet())
  {
    if (addComma)
    {
      result.append(",");
    }

    result.append(escapeStringElement(ElementType::tagKey, first));
    result.append("=");
    result.append(escapeStringElement(ElementType::tagValue, second));
    addComma = true;
  }

  return result;
}

std::string Database::formatFields(const DataPoint& point)
{
  std::string result;
  result.reserve(point.getFieldSet().size() * 32);

  bool addComma = false;
  for (const auto& [fieldName, fieldVal]: point.getFieldSet())
  {
    if (addComma)
    {
      result.append(",");
    }

    result.append(escapeStringElement(ElementType::fieldKey, fieldName));
    result.append("=");

    std::visit(sen::Overloaded {[&result](int32_t value)
                                {
                                  result.append(std::to_string(value));
                                  result.append("i");
                                },
                                [&result](uint32_t value)
                                {
                                  result.append(std::to_string(value));
                                  result.append("u");
                                },
                                [&result](int64_t value)
                                {
                                  result.append(std::to_string(value));
                                  result.append("i");
                                },
                                [&result](uint64_t value)
                                {
                                  result.append(std::to_string(value));
                                  result.append("u");
                                },
                                [&result](float32_t value) { result.append(std::to_string(value)); },
                                [&result](float64_t value) { result.append(std::to_string(value)); },
                                [&result](Duration value) { result.append(std::to_string(value.getNanoseconds())); },
                                [&result](TimeStamp value)
                                { result.append(std::to_string(value.sinceEpoch().getNanoseconds())); },
                                [&result](uint8_t value)
                                {
                                  result.append(std::to_string(value));
                                  result.append("u");
                                },
                                [&result](int16_t value)
                                {
                                  result.append(std::to_string(value));
                                  result.append("i");
                                },
                                [&result](uint16_t value)
                                {
                                  result.append(std::to_string(value));
                                  result.append("u");
                                },
                                [&result](bool value) { result.append(value ? "true" : "false"); },
                                [&result](const std::string& value)  // NOSONAR
                                {
                                  result.append("\"");
                                  result.append(escapeStringElement(ElementType::fieldValue, value));
                                  result.append("\"");
                                }},
               fieldVal);
    addComma = true;
  }

  return result;
}

std::string Database::escapeCharacters(std::string_view input, std::string_view escapedChars)
{
  static const std::string escapeCharacter {"\\"};
  std::string output;
  output.reserve(input.size());

  std::size_t searchStartPos {0};
  std::size_t escapedCharacterPos {input.find_first_of(escapedChars, searchStartPos)};
  while (escapedCharacterPos != std::string::npos)
  {
    output.append(input, searchStartPos, escapedCharacterPos - searchStartPos);
    output.append(escapeCharacter).append(1, input[escapedCharacterPos]);
    searchStartPos = escapedCharacterPos + 1;
    escapedCharacterPos = input.find_first_of(escapedChars, searchStartPos);
  }
  output.append(input, searchStartPos);

  return output;
}

std::string Database::stripRecordBreaks(ElementType type, std::string_view element)
{
  if (element.find_first_of("\n\r") == std::string_view::npos)
  {
    return std::string {element};
  }

  // A newline is the record terminator and the line protocol gives no way to escape one, so a
  // value carrying it cannot be written faithfully: left in, it ends the record early and whatever
  // followed is parsed as another record, silently and on every transport but HTTP. Removing it
  // keeps the point, which is still a true point, rather than dropping data over a defect in it.
  std::string cleaned {element};
  cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), [](char c) { return c == '\n' || c == '\r'; }),
                cleaned.end());

  static std::atomic<bool> reported {false};
  if (!reported.exchange(true))
  {
    // Named, and carrying what is left of the text, because "a value had a line break" leaves an
    // operator unable to find which series it came from. Said once per run.
    getLogger()->warn(
      "a {} contained a line break, which line protocol cannot carry, so it is removed. "
      "With the breaks taken out the value begins '{}'. Said once per run.",
      nameOf(type),
      cleaned.substr(0U, std::min<std::size_t>(cleaned.size(), 60U)));
  }

  return cleaned;
}

std::string_view Database::nameOf(ElementType type) noexcept
{
  switch (type)
  {
    case ElementType::measurement:
      return "measurement name";
    case ElementType::tagKey:
      return "tag key";
    case ElementType::tagValue:
      return "tag value";
    case ElementType::fieldKey:
      return "field key";
    case ElementType::fieldValue:
      return "string field value";
  }

  return "value";
}

std::string Database::escapeStringElement(ElementType type, std::string_view element)
{
  // Before escaping, because this one is not an escape: there is no sequence to escape it to.
  const auto cleaned = stripRecordBreaks(type, element);

  // https://docs.influxdata.com/influxdb/cloud/reference/syntax/line-protocol/#special-characters
  static const std::string commaAndSpace = ", ";
  static const std::string commaEqualsAndSpace = ",= ";
  static const std::string doubleQuoteAndBackslash = R"("\)";

  switch (type)
  {
    case ElementType::measurement:
      return escapeCharacters(cleaned, commaAndSpace);
    case ElementType::tagKey:
    case ElementType::tagValue:
    case ElementType::fieldKey:
      return escapeCharacters(cleaned, commaEqualsAndSpace);
    case ElementType::fieldValue:
      return escapeCharacters(cleaned, doubleQuoteAndBackslash);
  }
  return cleaned;
}

}  // namespace sen::components::influx
