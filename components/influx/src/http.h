// === http.h ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_INFLUX_SRC_HTTP_H
#define SEN_COMPONENTS_INFLUX_SRC_HTTP_H

// component
#include "database.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/base64.h"
#include "sen/core/base/class_helpers.h"

// asio
#include <asio/connect.hpp>  // NOLINT(misc-include-cleaner)
#include <asio/error.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/read.hpp>        // NOLINT(misc-include-cleaner)
#include <asio/read_until.hpp>  // NOLINT(misc-include-cleaner)
#include <asio/streambuf.hpp>
#include <asio/write.hpp>  // NOLINT(misc-include-cleaner)

// std
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <istream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace sen::components::influx
{

/// Telegraf's influxdb_listener over HTTP/1.1. The body is framed by its length, so this transport
/// needs no line separator and no datagram bound, and the answer says whether the batch was taken.
///
/// No TLS, which would put OpenSSL on every consumer of Sen; no redirects, reported as a 3xx; no
/// chunked request body. A chunked answer ends the connection, since its extent cannot be known
/// without parsing its framing.
/// The largest field block and answer body this will read. One figure for the body, used by both
/// the buffer's maximum and the declared-length check, so the two cannot disagree.
constexpr std::size_t httpFieldBlockLimit = 16U * 1024U;
constexpr std::size_t httpAnswerBodyLimit = 64U * 1024U;

class HttpTransport: public Transport
{
  SEN_NOCOPY_NOMOVE(HttpTransport)

public:
  HttpTransport(const std::string& hostname,
                int port,
                std::string path,
                const std::string& username = {},
                const std::string& password = {});
  ~HttpTransport() override = default;

protected:
  void doSend(sen::Span<const char> data) override;

private:
  /// What one request and its answer established. These are not interchangeable: a write that
  /// failed means no complete request reached Telegraf, so the batch may be sent again, while an
  /// answer that could not be read means Telegraf may have taken the batch and simply answered
  /// late, so sending it again would deliver it twice.
  enum class SendOutcome
  {
    delivered,
    notSent,
    fateUnknown
  };

  /// What one bounded operation did. `handedToSocket` is what the kernel accepted, not what
  /// Telegraf received -- a complete write to a peer that closed long ago is ordinary -- but it is
  /// what decides a retry, since the listener stores each line as it parses it.
  struct Bounded
  {
    asio::error_code error;
    std::size_t handedToSocket = 0U;
  };

private:
  /// Runs one asynchronous operation and returns its error, or asio::error::timed_out when the
  /// deadline fired first. Every operation this transport performs goes through here, because all
  /// of them run on the cycle doing the recording and none of them may wait without a bound.
  template <typename Start>
  [[nodiscard]] Bounded runBounded(Start&& start);

  [[nodiscard]] bool connect();
  /// True when an already-open connection cannot be used as it stands: the far end has closed it,
  /// or a byte waiting on an idle connection shows it is out of step. Either way, dial again.
  ///
  /// Asked before writing, the only moment the answer is unambiguous: afterwards an end-of-file
  /// cannot be told from a listener that read the batch and died.
  [[nodiscard]] bool connectionUnusable();
  /// Reads until the delimiter, or exactly `exactly` bytes when no delimiter is given. False when
  /// it failed or ran out of time, and the caller then ends the connection: a half-read answer
  /// would be mistaken for the next one.
  [[nodiscard]] Bounded readBounded(asio::streambuf& into,
                                    std::string_view delimiter,
                                    std::size_t exactly,
                                    bool untilFieldBlockEnds = false);

  /// What became of the answer, which is what decides whether a batch may be sent again.
  enum class AnswerOutcome
  {
    /// A status line was read.
    read,
    /// The far end closed having sent nothing. On its own this does not mean the request was
    /// unseen -- a listener that stored every line and died before answering looks the same -- so
    /// only the caller's knowledge that the connection was an idle reused one justifies a retry.
    peerClosed,
    /// The deadline ran out, or what arrived could not be read. Says nothing about whether the
    /// batch was taken.
    noAnswer
  };

  /// What the field block said, for the four headers that change what happens next.
  struct AnswerHeaders
  {
    std::size_t bodyLength = 0U;
    bool chunked = false;
    bool closeRequested = false;
    bool lengthSeen = false;
    bool lengthUnusable = false;
  };

  /// Reads the field block from the stream the status line was taken from. Separate from
  /// readAnswer because together they were past the size the checks allow, and this half is
  /// self-contained: it touches the socket not at all.
  [[nodiscard]] AnswerHeaders parseAnswerHeaders(std::istream& stream, std::string& reason);

  /// Reads the answer, returning its status code, or zero when none could be read.
  [[nodiscard]] int readAnswer(std::string& reason, AnswerOutcome& outcome);
  [[nodiscard]] std::string makeRequest(std::size_t bodyLength) const;
  [[nodiscard]] SendOutcome exchange(const std::string& request,
                                     sen::Span<const char> data,
                                     bool reusedConnection,
                                     int& status,
                                     std::string& reason);
  void reportStatus(int status, const std::string& reason);
  [[nodiscard]] bool shouldReport();
  void reportUnanswered(const std::string& detail);
  void reportDropping();

private:
  /// Its own, not the component's. This transport is the only one that runs a context rather than
  /// just taking sockets from one, and running a shared context would execute any other user's
  /// handlers on the recording thread. restart() on a context another thread is running is
  /// undefined, so sharing could not be made safe by being careful.
  asio::io_context ioContext_;
  asio::ip::tcp::socket socket_;
  /// Members, not locals: a read handed one of these must not be able to outlive it. Both carry a
  /// maximum, because a deadline alone does not stop a peer that streams bytes steadily; past it
  /// the read fails and the connection ends.
  asio::streambuf answerBuffer_ {httpFieldBlockLimit};
  asio::streambuf bodyBuffer_ {httpAnswerBodyLimit};
  asio::ip::tcp::resolver::results_type endpoints_;
  std::string host_;
  std::string path_;
  /// The whole header line, or nothing when no username was configured.
  std::string authorization_;
  bool connected_ {false};
  /// Counted rather than latched: a configuration that drops every batch would otherwise say so
  /// once and then lose everything in silence.
  std::size_t dropped_ {0U};
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline HttpTransport::HttpTransport(const std::string& hostname,
                                    int port,
                                    std::string path,
                                    const std::string& username,
                                    const std::string& password)
  : socket_(ioContext_), host_(hostname), path_(std::move(path))
{
  host_.append(":").append(std::to_string(port));

  // The listener serves /write. An empty target would send "POST  HTTP/1.1", which Telegraf
  // answers 400 to, and that reads as a complaint about the batch rather than the request line.
  if (path_.empty())
  {
    path_ = "/write";
  }
  else if (path_.front() != '/')
  {
    // "write" rather than "/write" makes the request line "POST write HTTP/1.1", which is not a
    // valid origin-form target: Go answers 400 and this transport then reports a status, which
    // reads as Telegraf judging the batch rather than as a mistake in one character of config.
    path_.insert(path_.begin(), '/');
  }

  if (!username.empty())
  {
    std::string credentials {username};
    credentials.append(":").append(password);

    authorization_ = "Authorization: Basic ";
    authorization_.append(sen::std_util::base64Encode(credentials)).append("\r\n");
  }

  // Synchronous: this runs once while the component loads, not on a recording cycle.
  asio::ip::tcp::resolver resolver(ioContext_);
  endpoints_ = resolver.resolve(hostname, std::to_string(port));

  if (endpoints_.empty())
  {
    throwRuntimeError("no address for '" + hostname + "'");
  }
}

template <typename Start>
inline HttpTransport::Bounded HttpTransport::runBounded(Start&& start)
{
  // Per operation, not per send. A listener on the same host answers in well under a millisecond,
  // so a second is already generous. What the recording cycle feels is the whole of doSend: a
  // connect, one write and two reads, each up to a second plus its drain, over two attempts, so
  // about eleven seconds against a host that accepts nothing and says nothing.
  constexpr auto operationTimeout = std::chrono::seconds {1};
  // The drain below. Closing the socket completes every operation on it, so the handler is already
  // queued and this costs nothing in practice; the bound is here for the case where it is not, and
  // it is short because four rounds of a whole second would dominate the per-call cost. Abandoning
  // an operation that will not retire is safe, since the handler owns the state it writes to.
  constexpr auto drainTimeout = std::chrono::milliseconds {100};
  constexpr auto drainRounds = 4;

  const auto state = std::make_shared<Bounded>();
  state->error = asio::error::would_block;

  // Shared and by value, so an operation that outlives this call writes somewhere alive.
  start(
    [state](const asio::error_code& error, std::size_t bytes)
    {
      state->error = error;
      state->handedToSocket = bytes;
    });

  ioContext_.restart();
  std::ignore = ioContext_.run_for(operationTimeout);

  if (state->error == asio::error::would_block)
  {
    // Closed, not merely cancelled. asio's range connect operation tests is_open() and gives up,
    // where after a bare cancel it finds the socket open, moves to the next endpoint and starts a
    // fresh connect carrying no cancellation -- which this call would then leave running.
    asio::error_code ignored;
    std::ignore = socket_.close(ignored);
    connected_ = false;

    for (auto round = 0; round < drainRounds && state->error == asio::error::would_block; ++round)
    {
      ioContext_.restart();
      if (ioContext_.run_one_for(drainTimeout) == 0U)
      {
        break;
      }
    }

    // The handler may have run during the drain and reported success. Returning timed_out anyway
    // discards a working operation -- on the connect path, a live socket closed and an attempt
    // spent for nothing.
    if (state->error != asio::error::would_block)
    {
      connected_ = !state->error && connected_;
      return *state;
    }

    return Bounded {asio::error::timed_out, state->handedToSocket};
  }

  return *state;
}

inline bool HttpTransport::connect()
{
  asio::error_code closing;
  std::ignore = socket_.close(closing);

  // Bounded like everything else: a host that drops SYNs rather than refusing them would otherwise
  // hold the recording cycle for the operating system's whole SYN budget. The handler is adapted
  // to the one shape runBounded takes, since a connect moves no bytes of its own.
  const auto connected = runBounded(
    [this](auto&& handler)
    {
      asio::async_connect(socket_,
                          endpoints_,
                          [handler](const asio::error_code& error, const asio::ip::tcp::endpoint&) mutable
                          { handler(error, std::size_t {0U}); });
    });

  connected_ = !connected.error;

  return connected_;
}

inline bool HttpTransport::connectionUnusable()
{
  asio::error_code asking;
  std::ignore = socket_.non_blocking(true, asking);

  if (asking)
  {
    // Cannot ask, so claim nothing and let the exchange find out.
    return false;
  }

  char probe = 0;
  std::ignore = socket_.receive(asio::buffer(&probe, 1U), asio::socket_base::message_peek, asking);

  asio::error_code restoring;
  std::ignore = socket_.non_blocking(false, restoring);

  // Go sends nothing unsolicited on an idle keep-alive connection, so a byte waiting here is not
  // health: it means the previous answer was mis-framed and the offsets are no longer known. An
  // error or a zero-byte read is the far end gone. Only would_block says the connection is usable.
  return asking != asio::error::would_block;
}

/// Where a field block ends. CRLF CRLF is the conformant terminator, but RFC 7230 asks a recipient
/// to accept a bare LF as a line ending, and a peer that uses them never produces the conformant
/// form at all -- the read then runs to its deadline on every answer and the batch is dropped.
struct FieldBlockEnd
{
  /// asio accepts a custom match condition only for a function or for a type carrying result_type
  /// (asio::is_match_condition, read_until.hpp:76-77). This is that hook, not a return type.
  using result_type = bool;  // NOLINT(readability-identifier-naming): asio's required spelling

  template <typename Iterator>
  [[nodiscard]] std::pair<Iterator, bool> operator()(Iterator begin, Iterator end) const
  {
    for (auto at = begin; at != end; ++at)
    {
      if (*at != '\n')
      {
        continue;
      }

      auto next = at;
      ++next;
      if (next == end)
      {
        return {begin, false};
      }

      // LF LF, or LF CR LF: either way the block has ended just past it.
      if (*next == '\n')
      {
        return {++next, true};
      }

      if (*next == '\r')
      {
        auto after = next;
        ++after;
        if (after == end)
        {
          return {begin, false};
        }

        if (*after == '\n')
        {
          return {++after, true};
        }
      }
    }

    return {begin, false};
  }
};

inline HttpTransport::Bounded HttpTransport::readBounded(asio::streambuf& into,
                                                         std::string_view delimiter,
                                                         std::size_t exactly,
                                                         bool untilFieldBlockEnds)
{
  if (untilFieldBlockEnds)
  {
    return runBounded(
      [this, &into](auto&& handler)
      { asio::async_read_until(socket_, into, FieldBlockEnd {}, std::forward<decltype(handler)>(handler)); });
  }

  if (delimiter.empty())
  {
    return runBounded(
      [this, &into, exactly](auto&& handler)
      { asio::async_read(socket_, into, asio::transfer_exactly(exactly), std::forward<decltype(handler)>(handler)); });
  }

  return runBounded(
    [this, &into, delimiter](auto&& handler)
    { asio::async_read_until(socket_, into, std::string {delimiter}, std::forward<decltype(handler)>(handler)); });
}

/// True the first time and then rarely, so a run that drops every batch neither floods the log nor
/// goes silent after one line.
inline bool HttpTransport::shouldReport()
{
  ++dropped_;

  constexpr std::size_t everySoOften = 100U;

  return dropped_ == 1U || dropped_ % everySoOften == 0U;
}

inline void HttpTransport::reportDropping()
{
  if (shouldReport())
  {
    std::cerr << "influx: the telegraf listener cannot be reached; points are being dropped; " << dropped_
              << " dropped so far" << std::endl;
  }
}

inline HttpTransport::AnswerHeaders HttpTransport::parseAnswerHeaders(std::istream& stream, std::string& reason)
{
  AnswerHeaders found;

  // A body would have to be drained before the next answer could be read, so any length at all
  // ends the connection rather than guessing where the next one starts. Header names are
  // case-insensitive, so they are folded before they are compared.
  std::string header;
  auto& bodyLength = found.bodyLength;
  auto& chunked = found.chunked;
  auto& closeRequested = found.closeRequested;
  auto& lengthSeen = found.lengthSeen;
  auto& lengthUnusable = found.lengthUnusable;
  while (std::getline(stream, header) && header != "\r")
  {
    // A folded continuation belongs to the header above it, not to one of its own, so a folded
    // value carrying a colon would otherwise become a name and a value.
    if (!header.empty() && (header.front() == ' ' || header.front() == '\t'))
    {
      continue;
    }

    const auto colon = header.find(':');
    if (colon == std::string::npos)
    {
      continue;
    }

    std::string name = header.substr(0U, colon);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });

    std::string value = header.substr(colon + 1U);
    const auto first = value.find_first_not_of(" \t");
    value = (first == std::string::npos) ? std::string {} : value.substr(first);
    const auto last = value.find_last_not_of(" \t\r");
    value = (last == std::string::npos) ? std::string {} : value.substr(0U, last + 1U);

    if (name == "content-length")
    {
      // Parsed strictly. A non-numeric value answers zero, which is indistinguishable from no
      // header at all, and the body would then be left in the socket for the next answer to be
      // read out of. The end pointer is what separates the two.
      char* end = nullptr;
      const auto parsed = std::strtoul(value.c_str(), &end, 10);
      const auto consumedAll = end != nullptr && end != value.c_str() && *end == '\0';

      if (!consumedAll)
      {
        lengthUnusable = true;
      }
      else if (lengthSeen && parsed != bodyLength)
      {
        // Two lengths that disagree cannot both be honoured, and quietly preferring one is how a
        // framing confusion is built.
        lengthUnusable = true;
      }
      else
      {
        bodyLength = parsed;
        lengthSeen = true;
      }
    }
    else if (name == "transfer-encoding")
    {
      // Only chunked framing cannot be drained; any other coding leaves the body where it is.
      chunked = value.find("chunked") != std::string::npos;
    }
    else if (name == "connection")
    {
      // Go sends this on the paths where its handler leaves the body unread, so honouring it is
      // what keeps the next request from going out on a socket the server is finished with.
      std::string folded = value;
      std::transform(folded.begin(), folded.end(), folded.begin(), [](unsigned char c) { return std::tolower(c); });
      closeRequested = folded.find("close") != std::string::npos;
    }
    else if (name == "x-influxdb-error")
    {
      // Set on every path the listener refuses or partly accepts a batch on, and it arrives in the
      // field block already read, so the reason is in hand without touching the body.
      reason = value;
    }
  }

  return found;
}

inline int HttpTransport::readAnswer(std::string& reason, AnswerOutcome& outcome)
{
  outcome = AnswerOutcome::noAnswer;

  asio::error_code error;

  // Whatever an earlier answer left behind, so this one starts at its own first byte.
  answerBuffer_.consume(answerBuffer_.size());
  bodyBuffer_.consume(bodyBuffer_.size());

  // Up to the blank line: the status and the headers, which is all a write needs. Bounded, because
  // this runs on the cycle of the application being recorded and a listener that accepts without
  // answering would otherwise hold it for ever.
  const auto headers = readBounded(answerBuffer_, {}, 0U, true);

  if (headers.error)
  {
    // Only a close with nothing sent says the request was never read. A deadline that ran out is
    // not that, and conflating the two is how a batch Telegraf is still parsing gets sent twice.
    const auto closed = headers.error == asio::error::eof || headers.error == asio::error::connection_reset;
    const auto closedUnread = headers.handedToSocket == 0U && closed;
    outcome = closedUnread ? AnswerOutcome::peerClosed : AnswerOutcome::noAnswer;

    if (closed && !closedUnread)
    {
      // Part of a field block arrived and then the peer closed: not a deadline, and not an unread
      // request either: the listener died mid-answer.
      reason = "the listener closed part way through its answer";
    }

    // Past the buffer's maximum the read completes with not_found rather than a socket error, so
    // the answer did arrive and was too large to hold. Said apart, or it reads as silence.
    if (headers.error == asio::error::not_found)
    {
      reason = "the answer's headers were larger than this transport will hold";
    }

    std::ignore = socket_.close(error);
    connected_ = false;
    return 0;
  }

  std::istream stream(&answerBuffer_);
  std::string version;
  static constexpr std::string_view httpVersionPrefix {"HTTP/"};

  int status = 0;
  stream >> version >> status;

  // The reason phrase is still in the buffer and may legally contain a colon, which would make the
  // tail of the status line parse as a header of its own.
  std::string reasonPhrase;
  std::ignore = std::getline(stream, reasonPhrase);

  if (!stream || version.compare(0U, httpVersionPrefix.size(), httpVersionPrefix) != 0)
  {
    // Something answered, but not a status line this can read, so where the next answer would
    // begin is unknown. Every other failure here ends the connection and so does this one.
    //
    // Named for the operator: a peer that is talking and talking wrong points somewhere entirely
    // different from one that went quiet.
    reason = "the answer did not begin with a status line";
    std::ignore = socket_.close(error);
    connected_ = false;
    return 0;
  }

  // An interim answer -- 100 Continue, 103 Early Hints -- is a complete field block that is not the
  // final one, and nothing here is in a position to read the final one at the right offset
  // afterwards. Telegraf sends none; an intervening proxy can. Rather than fall one response out of
  // step for the life of the connection, this ends it and the batch counts as unconfirmed.
  if (status < 200)
  {
    reason = "an interim answer arrived, which this transport does not follow";
    std::ignore = socket_.close(error);
    connected_ = false;
    return 0;
  }

  const auto found = parseAnswerHeaders(stream, reason);
  const auto bodyLength = found.bodyLength;
  const auto chunked = found.chunked;
  const auto closeRequested = found.closeRequested;
  const auto lengthSeen = found.lengthSeen;
  const auto lengthUnusable = found.lengthUnusable;

  outcome = AnswerOutcome::read;

  // A length that cannot be read, or two that disagree, leaves the body's extent unknown, which is
  // the same problem as chunked framing and gets the same answer.
  if (lengthUnusable)
  {
    reason = "the answer's Content-Length could not be read";
    std::ignore = socket_.close(error);
    connected_ = false;
    return status;
  }

  // No declared length and no chunked framing leaves the body's extent unknown, so what follows
  // would be read as the next answer. 204 and 304 cannot carry a body and must stay exempt:
  // Telegraf answers a successful write with 204 and Go omits the length on one, so without the
  // exemption every healthy connection is torn down.
  constexpr auto noContent = 204;
  constexpr auto notModified = 304;
  if (!lengthSeen && !chunked && status != noContent && status != notModified)
  {
    reason = reason.empty() ? "the answer declared no body length" : reason;
    std::ignore = socket_.close(error);
    connected_ = false;
    return status;
  }

  // A chunked answer cannot be drained without reading its framing, so the connection ends rather
  // than leave the next answer starting at an unknown offset.
  if (chunked)
  {
    std::ignore = socket_.close(error);
    connected_ = false;
    return status;
  }

  // A length this side did not choose: anything beyond what an error message needs is refused
  // rather than buffered, and strtoul answers ULONG_MAX for a negative or oversized field.
  if (bodyLength > httpAnswerBodyLimit)
  {
    std::ignore = socket_.close(error);
    connected_ = false;
    return status;
  }

  // Body bytes already in hand, because the field-block read overshot the blank line. Counted
  // separately from `reason`: that may have come from a header, and a header's length says nothing
  // about how much of the body arrived. Using one as the other asks the socket for bytes that are
  // not coming, and costs a deadline on the recording cycle for every answer carrying a body.
  std::string bodyInHand {std::istreambuf_iterator<char> {stream}, std::istreambuf_iterator<char> {}};

  if (bodyInHand.size() < bodyLength)
  {
    if (readBounded(bodyBuffer_, {}, bodyLength - bodyInHand.size()).error)
    {
      std::ignore = socket_.close(error);
      connected_ = false;
      return status;
    }

    std::istream more(&bodyBuffer_);
    bodyInHand.append(std::istreambuf_iterator<char> {more}, std::istreambuf_iterator<char> {});
  }

  // Drained either way, to leave the connection usable; it supplies the reason only when no header
  // carried one.
  if (reason.empty())
  {
    reason = bodyInHand;
  }

  // The answer is complete and the status stands; the connection does not.
  if (closeRequested)
  {
    std::ignore = socket_.close(error);
    connected_ = false;
  }

  constexpr std::size_t reasonLimit = 200U;
  if (reason.size() > reasonLimit)
  {
    reason.resize(reasonLimit);
  }

  // This text comes from the peer and is derived from the input that failed to parse, which is
  // Sen's own line protocol: a value carrying an escape sequence would be echoed back here and
  // written to an operator's terminal. Anything the terminal would act on is replaced.
  std::replace_if(
    reason.begin(),
    reason.end(),
    [](char c) { return static_cast<unsigned char>(c) < 0x20U || static_cast<unsigned char>(c) > 0x7EU; },
    '?');

  return status;
}

inline std::string HttpTransport::makeRequest(std::size_t bodyLength) const
{
  std::string request;
  request.append("POST ").append(path_).append(" HTTP/1.1\r\n");
  request.append("Host: ").append(host_).append("\r\n");
  request.append("Content-Type: text/plain; charset=utf-8\r\n");
  request.append("Content-Length: ").append(std::to_string(bodyLength)).append("\r\n");

  if (getCompressionEnabled())
  {
    request.append("Content-Encoding: gzip\r\n");
  }

  request.append(authorization_);
  request.append("\r\n");

  return request;
}

inline HttpTransport::SendOutcome HttpTransport::exchange(const std::string& request,
                                                          sen::Span<const char> data,
                                                          bool reusedConnection,
                                                          int& status,
                                                          std::string& reason)
{
  // Headers and body in one operation. Two writes left a window in which a peer that went away
  // decided, by timing alone, whether this looked like a failed write or a successful one with a
  // dead answer -- a race whose outcome was then classified. One write removes it, and the byte
  // count alone says which case this is.
  const std::array<asio::const_buffer, 2> whole {asio::buffer(request), asio::buffer(data.data(), data.size())};

  const auto written = runBounded([this, &whole](auto&& handler)
                                  { asio::async_write(socket_, whole, std::forward<decltype(handler)>(handler)); });

  if (written.error)
  {
    // At or short of the field block, no body byte left this host. Short of it the request was
    // never routed; exactly at it the listener routes the request and then waits on a body whose
    // first line never arrives, so its parser yields nothing and nothing reaches the accumulator.
    // Either way no point can exist. Arithmetic, rather than a claim about the server.
    if (written.handedToSocket <= request.size())
    {
      return SendOutcome::notSent;
    }

    // Past the field block, so part of the body arrived, and influxdb_listener stores each line as
    // its parser yields it.
    return SendOutcome::fateUnknown;
  }

  auto outcome = AnswerOutcome::noAnswer;
  status = readAnswer(reason, outcome);

  if (status != 0)
  {
    return SendOutcome::delivered;
  }

  // A reused connection closed with nothing sent: the listener drops an idle one before reading
  // it, so the request was never seen. A deadline is deliberately not this case -- Telegraf answers
  // only after its parse loop and treats ten seconds as slow, so a batch it is still working
  // through looks identical, and resending it delivers it twice.
  if (outcome == AnswerOutcome::peerClosed && reusedConnection)
  {
    return SendOutcome::notSent;
  }

  // A fresh connection closed with nothing sent is most likely a listener going down, but the same
  // bytes appear if it stored every line and died before answering, so the batch is dropped rather
  // than risk a duplicate. The cause is reported, since the two point at different things.
  if (outcome == AnswerOutcome::peerClosed)
  {
    reason = reason.empty() ? "the listener closed the connection without answering" : reason;
  }

  return SendOutcome::fateUnknown;
}

inline void HttpTransport::reportStatus(int status, const std::string& reason)
{
  // A wrong path, wrong credentials or a body over the listener's limit answers the same way on
  // every batch, so these are rate limited like the socket transports' own messages. Without that,
  // a misconfigured path writes a line per batch for as long as the run lasts.
  if (status >= 300 && status < 400)
  {
    if (shouldReport())
    {
      std::cerr << "influx: telegraf answered " << status << " and this transport does not follow redirects; "
                << dropped_ << " dropped so far" << std::endl;
    }

    return;
  }

  if (status < 200 || status >= 300)
  {
    if (shouldReport())
    {
      // Deliberately not "refused": a 400 most often means the batch was taken apart from the
      // malformed lines in it, and a 503 means it was taken up to the point the listener stopped.
      // Only the reason text separates those from a clean refusal, which is why it is reported.
      std::cerr << "influx: telegraf answered " << status << " for a batch";
      if (!reason.empty())
      {
        std::cerr << ": " << reason;
      }
      std::cerr << "; " << dropped_ << " not confirmed so far" << std::endl;
    }

    return;
  }

  if (dropped_ != 0U)
  {
    std::cerr << "influx: the telegraf listener is answering again, after " << dropped_
              << " batches that were not confirmed" << std::endl;
  }

  dropped_ = 0U;
}

inline void HttpTransport::reportUnanswered(const std::string& detail)
{
  // Not "telegraf took it": whether it did is exactly what is unknown here.
  if (shouldReport())
  {
    std::cerr << "influx: no answer this could read came back for a batch, so it is dropped rather "
                 "than sent again, which could deliver it twice";
    if (!detail.empty())
    {
      std::cerr << " (" << detail << ")";
    }
    std::cerr << "; " << dropped_ << " dropped so far" << std::endl;
  }
}

inline void HttpTransport::doSend(sen::Span<const char> data)
{
  const auto request = makeRequest(data.size());

  // Two attempts, the second only where Telegraf cannot have seen the request. A write succeeds
  // even against a closed connection, because the kernel takes the bytes, so the signal is how much
  // of the request went out and whether an answer started -- not whether the write failed.
  for (auto attempt = 0; attempt < 2; ++attempt)
  {
    // Asked before writing rather than inferred afterwards. A connection the listener closed while
    // it was idle, or one left out of step by a mis-framed answer, is then known here, and costs
    // one dial instead of a wasted exchange -- which, where the listener closes faster than the
    // sampling period, would otherwise be every batch.
    if (connected_ && connectionUnusable())
    {
      connected_ = false;
    }

    // Whether this attempt goes out on a connection that was already open. Derived per attempt, so
    // the one after a reconnect does not inherit the previous answer.
    const auto reusedConnection = connected_;

    if (!connected_ && !connect())
    {
      continue;
    }

    auto status = 0;
    std::string reason;

    switch (exchange(request, data, reusedConnection, status, reason))
    {
      case SendOutcome::delivered:
        reportStatus(status, reason);
        return;

      case SendOutcome::fateUnknown:
        // Telegraf may already hold this batch. A duplicate is worse than a gap for any output
        // that counts what it receives, so this one is not sent again.
        connected_ = false;
        reportUnanswered(reason);
        return;

      case SendOutcome::notSent:
        connected_ = false;
        break;
    }
  }

  reportDropping();
}

}  // namespace sen::components::influx

#endif  // SEN_COMPONENTS_INFLUX_SRC_HTTP_H
