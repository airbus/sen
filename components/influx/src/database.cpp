// === database.cpp ====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "database.h"

#include "data_point.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/timestamp.h"

// zlib
#include <zlib.h>

// std
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <string>
#include <string_view>
#include <variant>

namespace sen::components::influx
{

void Transport::setCompressionEnabled(bool enabled, int level) noexcept
{
  compressionEnabled_ = enabled;
  compressionLevel_ = level;
}

bool Transport::getCompressionEnabled() const noexcept { return compressionEnabled_; }

std::vector<char> Transport::compressGzip(std::string& str, int compressionLevel)
{
  z_stream zs = {};
  std::memset(&zs, 0, sizeof(zs));

  // windowBits = 15 + 16 forces zlib to write a standard gzip header/trailer
  if (deflateInit2(&zs, compressionLevel, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
  {
    throw std::runtime_error("deflateInit2 failed while initializing gzip.");
  }

  zs.next_in = reinterpret_cast<Bytef*>(str.data());  // NOLINT
  zs.avail_in = str.size();

  constexpr std::size_t maxBufferSize = 32768;

  int ret;
  char outBuffer[maxBufferSize];
  std::vector<char> outputData;

  // Retrieve the compressed data blocks
  do
  {
    zs.next_out = reinterpret_cast<Bytef*>(outBuffer);  // NOLINT
    zs.avail_out = sizeof(outBuffer);

    ret = deflate(&zs, Z_FINISH);

    if (outputData.size() < zs.total_out)
    {
      // Append the new compressed block to our container NOLINTNEXTLINE
      outputData.insert(outputData.end(), outBuffer, outBuffer + sizeof(outBuffer) - zs.avail_out);
    }
  } while (ret == Z_OK);

  deflateEnd(&zs);

  if (ret != Z_STREAM_END)
  {
    throw std::runtime_error("Exception occurred while compressing to gzip");
  }

  return outputData;
}

void Transport::send(std::string&& message)
{
  if (compressionEnabled_)
  {
    if (requiresLineSeparator())
    {
      message.append("\n");
    }

    doSend(makeConstSpan(compressGzip(message, compressionLevel_)));
  }
  else
  {
    doSend(makeConstSpan(message.data(), message.size()));
  }
}

//----------------------------------------------------------------------------------------------------------------------
// Database
//----------------------------------------------------------------------------------------------------------------------

std::string Database::joinLineProtocolBatch() const
{
  std::string joinedBatch;

  for (const auto& point: pointBatch_)
  {
    joinedBatch.append(makeLine(point));
    joinedBatch.append("\n");
  }

  joinedBatch.erase(std::prev(joinedBatch.end()));
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

    result.append(first);
    result.append("=");
    result.append(second);
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

    result.append(fieldName);
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
                                  result.append(value);
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

std::string Database::escapeStringElement(ElementType type, std::string_view element)
{
  // https://docs.influxdata.com/influxdb/cloud/reference/syntax/line-protocol/#special-characters
  static const std::string commaAndSpace = ", ";
  static const std::string commaEqualsAndSpace = ",= ";
  static const std::string doubleQuoteAndBackslash = R"("\)";

  switch (type)
  {
    case ElementType::measurement:
      return escapeCharacters(element, commaAndSpace);
    case ElementType::tagKey:
    case ElementType::tagValue:
    case ElementType::fieldKey:
      return escapeCharacters(element, commaEqualsAndSpace);
    case ElementType::fieldValue:
      return escapeCharacters(element, doubleQuoteAndBackslash);
  }
  return std::string(element);
}

}  // namespace sen::components::influx
