// === database.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_INFLUX_SRC_DATABASE_H
#define SEN_COMPONENTS_INFLUX_SRC_DATABASE_H

#include "data_point.h"

// sen
#include "sen/core/base/span.h"

// generated code
#include "stl/configuration.stl.h"

// asio
#include <asio/io_context.hpp>
#include <asio/strand.hpp>

// std
#include <deque>
#include <limits>
#include <memory>
#include <mutex>
#include <string_view>

namespace sen::components::influx
{

//----------------------------------------------------------------------------------------------------------------------
// Transport
//----------------------------------------------------------------------------------------------------------------------

/// The byte bound to put on the batch, given the bound on one message. They are the same number
/// only where nothing compresses the batch afterwards: the batch bound counts the lines themselves,
/// while the message bound applies to what the transport finally sends.
///
/// With compression the batch bound is raised to a multiple of it. At the message bound it would
/// cap the uncompressed batch at the size of one datagram and waste most of the packet, since this
/// line protocol compresses by more than ten times; removed altogether it would leave the
/// transport's own split as the only limit, re-compressing each half as it recurses, with nothing
/// bounding how large the batch had grown first. Zero stays zero, meaning unbounded.
[[nodiscard]] inline std::size_t batchBoundFor(std::size_t messageBound, bool compressing) noexcept
{
  // Measured over Sen's own output, where a measurement and its tags repeat with only the value
  // changing: 13x over 32 points, 27x over 256, 38x over 2048. The transport has to split a
  // compressed payload only where this multiple exceeds the real ratio, so 16 usually costs
  // nothing and is bounded at four levels of splitting even for text that does not compress.
  constexpr std::size_t multiple = 16U;

  if (!compressing || messageBound == 0U)
  {
    return messageBound;
  }

  // A bound near the top of the range would otherwise wrap into a small one.
  constexpr auto headroom = std::numeric_limits<std::size_t>::max() / multiple;

  return messageBound > headroom ? std::numeric_limits<std::size_t>::max() : messageBound * multiple;
}

/// What a datagram of this family can carry, which bounds a message where one applies. The
/// unspecified form takes the smaller figure: it may resolve to either family, so the bound it is
/// held to has to be one both can honour.
[[nodiscard]] constexpr std::size_t datagramCeilingFor(IpFamily family) noexcept
{
  constexpr std::size_t overIpv4 = 65507U;
  constexpr std::size_t overIpv6 = 65527U;

  return family == IpFamily::v6 ? overIpv6 : overIpv4;
}

class Transport
{
  SEN_MOVE_ONLY(Transport)

public:
  Transport() = default;
  virtual ~Transport() = default;

public:
  void send(std::string&& message);
  /// Throws when the level is one deflateInit2 would refuse, so a bad configuration is named once.
  void setCompressionEnabled(bool enabled, int level);

  /// Bounds what one message may carry. Zero leaves it unbounded. A payload over the bound is cut
  /// at line boundaries into several messages, because compression only reveals the size afterwards.
  void setMaxMessageSize(std::size_t bytes) noexcept { maxMessageBytes_ = bytes; }

  [[nodiscard]] bool getCompressionEnabled() const noexcept;

protected:
  virtual void doSend(sen::Span<const char> data) = 0;
  [[nodiscard]] virtual bool requiresLineSeparator() const noexcept { return false; }
  [[nodiscard]] static std::vector<char> compressGzip(std::string& str, int compressionLevel);

private:
  /// Sends these lines as one message, cutting them in half and retrying when the payload that
  /// comes out of them is over the bound. Taken by value so send() can hand over its argument
  /// instead of having it copied, and so the separator can be appended in place.
  void sendLines(std::string lines);
  /// Cuts these lines in two at a line boundary and sends each half.
  void sendHalves(std::string_view lines);

private:
  bool compressionEnabled_ = false;
  int compressionLevel_ = 0;
  std::size_t maxMessageBytes_ = 0;
  /// Latched so a message that cannot be compressed is named once rather than on every cycle.
  bool compressionFailed_ = false;
};

//----------------------------------------------------------------------------------------------------------------------
// Database
//----------------------------------------------------------------------------------------------------------------------

class Database
{
  SEN_MOVE_ONLY(Database)

public:
  /// Shared rather than owned outright: a handler on the sending thread holds a copy, so it
  /// cannot be working on a transport this object has already taken with it. A `unique_ptr`
  /// converts on the way in, so a caller that owns one needs no change.
  explicit Database(std::shared_ptr<Transport> transport): transport_(std::move(transport))
  {
    SEN_ASSERT(transport_.get() != nullptr);
  }
  ~Database() = default;

public:
  /// Writes a point
  void write(DataPoint&& point);

  /// Flushes points batched (this can also happens when buffer is full)
  void flushBatch();

  /// Enables points batching
  void batchOf(std::size_t size = 32)
  {
    batchSize_ = size;
    isBatchingActivated_ = true;
  }

  /// Bounds a message in bytes as well as in points, for a transport whose message is a datagram.
  /// Zero leaves it unbounded. A batch is flushed before a line would carry it past the bound, so
  /// the bound holds however wide the points turn out to be.
  void messageOf(std::size_t bytes) noexcept { maxMessageBytes_ = bytes; }

  /// Hands each message to `io` instead of sending it on the calling thread, keeping at most
  /// `bound` of them waiting. Once that many are waiting the oldest goes: this feeds a dashboard,
  /// where a gap in history costs less than points that are no longer current, and a listener that
  /// comes back gets current data rather than a replay. `bound` must not be zero.
  void sendThrough(asio::io_context& io, std::size_t bound);

  /// Messages the bound has cost since the queue was last empty. Zero when nothing is queueing.
  [[nodiscard]] std::size_t droppedSoFar() const;

private:
  /// The messages waiting for the sending thread, and what the bound has cost. Held by shared_ptr
  /// so each posted handler owns a piece of it, which is what keeps a handler in flight from
  /// reaching into this object.
  struct Waiting
  {
    Waiting(asio::io_context& io, std::size_t limit): sending(asio::make_strand(io)), bound(limit) {}

    // Everything below is reached through the shared_ptr by the handlers themselves, which is the
    // point of the struct. One block rather than a comment per line, which clang-format rewraps
    // away from its target.
    // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
    /// Keeps two handlers off one transport however many threads come to run the context.
    asio::strand<asio::io_context::executor_type> sending;
    std::mutex guard;
    std::deque<std::string> messages;
    std::size_t bound;
    /// Counted rather than latched: a run that drops continuously would otherwise say so once and
    /// then lose the rest in silence.
    std::size_t dropped = 0U;
    /// One drain in flight at a time. Without it the scheduler's own queue grows by one entry per
    /// point with nothing bounding it, so the bound below stops bounding the memory, and asio runs
    /// every ready handler of a strand without re-reading its stopped flag, so a shutdown waits for
    /// the whole backlog rather than for one message.
    bool draining = false;
    // NOLINTEND(misc-non-private-member-variables-in-classes)
  };

private:
  enum class ElementType
  {
    measurement,
    tagKey,
    tagValue,
    fieldKey,
    fieldValue
  };

private:
  void transmit(std::string&& point);
  /// Sends one waiting message. Static and taking what it needs by shared_ptr, so it carries no
  /// dependence on the Database still being there.
  static void sendOneWaiting(const std::shared_ptr<Waiting>& waiting, const std::shared_ptr<Transport>& transport);
  [[nodiscard]] std::string joinLineProtocolBatch() const;
  void addPointToBatch(DataPoint&& point);
  [[nodiscard]] std::string makeLine(const DataPoint& point) const;
  static void appendIfNotEmpty(std::string& dest, std::string_view value, char separator);
  [[nodiscard]] static std::string escapeCharacters(std::string_view input, std::string_view escapedChars);
  /// Removes the line breaks the protocol has no escape for, saying so once per run. Not an
  /// escape: a newline is the record terminator, so there is nothing to escape it to.
  [[nodiscard]] static std::string stripRecordBreaks(ElementType type, std::string_view element);
  /// What to call an element kind in a message to an operator.
  [[nodiscard]] static std::string_view nameOf(ElementType type) noexcept;
  [[nodiscard]] static std::string escapeStringElement(ElementType type, std::string_view element);
  [[nodiscard]] static std::string formatFields(const DataPoint& point);
  [[nodiscard]] static std::string formatTags(const DataPoint& point);

private:
  std::shared_ptr<Transport> transport_;
  std::deque<std::string> lineBatch_;
  bool isBatchingActivated_ = false;
  std::size_t batchSize_ = 0;
  std::size_t batchBytes_ = 0;
  std::size_t maxMessageBytes_ = 0;
  /// Null until sendThrough is called, which is what decides whether a message is handed over or
  /// sent on the calling thread.
  std::shared_ptr<Waiting> waiting_;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline void Database::write(DataPoint&& point)
{
  if (isBatchingActivated_)
  {
    addPointToBatch(std::move(point));
  }
  else
  {
    transmit(makeLine(point));
  }
}

inline void Database::flushBatch()
{
  if (isBatchingActivated_ && !lineBatch_.empty())
  {
    transmit(joinLineProtocolBatch());
    lineBatch_.clear();
    batchBytes_ = 0U;
  }
}

inline void Database::addPointToBatch(DataPoint&& point)
{
  auto line = makeLine(point);

  // The separator this line will need once it is joined to the ones already waiting.
  const auto added = line.size() + (lineBatch_.empty() ? 0U : 1U);

  if (maxMessageBytes_ != 0U && !lineBatch_.empty() && batchBytes_ + added > maxMessageBytes_)
  {
    flushBatch();
  }

  batchBytes_ += line.size() + (lineBatch_.empty() ? 0U : 1U);
  lineBatch_.emplace_back(std::move(line));

  if (lineBatch_.size() >= batchSize_)
  {
    flushBatch();
  }
}

inline void Database::appendIfNotEmpty(std::string& dest, std::string_view value, char separator)
{
  if (!value.empty())
  {
    dest.append(std::string {separator}).append(value);
  }
}

inline std::string Database::makeLine(const DataPoint& point) const
{
  std::string line {escapeStringElement(ElementType::measurement, point.getName())};
  appendIfNotEmpty(line, formatTags(point), ',');
  appendIfNotEmpty(line, formatFields(point), ' ');

  return line.append(" ").append(std::to_string(point.getTimestamp().sinceEpoch().getNanoseconds()));
}

}  // namespace sen::components::influx

#endif  // SEN_COMPONENTS_INFLUX_SRC_DATABASE_H
