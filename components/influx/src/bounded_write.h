// === bounded_write.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_INFLUX_SRC_BOUNDED_WRITE_H
#define SEN_COMPONENTS_INFLUX_SRC_BOUNDED_WRITE_H

// sen
#include "sen/core/base/span.h"

// asio
#include <asio/buffer.hpp>
#include <asio/error.hpp>
#include <asio/error_code.hpp>

// std
#include <chrono>
#include <cstddef>
#include <thread>

#if defined(_WIN32)
#  include <winsock2.h>
#else
#  include <poll.h>
#  include <sys/socket.h>

#  include <cerrno>
#endif

namespace sen::components::influx
{

/// How long a stream transport may spend handing one message to the kernel. The same figure
/// Telegraf's own influxdb output defaults its write timeout to, and far longer than a healthy
/// peer needs: a write only waits at all once the far end has stopped draining.
constexpr auto streamWriteBound = std::chrono::milliseconds {5000};

/// How long a stream transport may spend dialling one endpoint. A listener on the same host
/// completes a connect in well under a millisecond and one on the same network in a few, so a
/// second is already generous and past it the far end is not coming. The same figure http.h gives
/// its own connect, and together with streamWriteBound it is what bounds a shutdown: a send dials
/// at most twice and writes at most twice, so doSend returns within twelve seconds against a
/// listener that is dead rather than merely slow.
constexpr auto streamConnectBound = std::chrono::milliseconds {1000};

namespace detail
{

/// Waits for the socket to take more bytes, or for the time to run out. Returns false on the
/// deadline and on a failure to wait at all, which the caller treats the same way.
template <typename Socket>
[[nodiscard]] inline bool waitUntilWritable(Socket& socket, std::chrono::milliseconds within)
{
  const auto milliseconds = static_cast<int>(within.count());

#if defined(_WIN32)
  WSAPOLLFD waiting {socket.native_handle(), POLLWRNORM, 0};

  return ::WSAPoll(&waiting, 1, milliseconds) > 0;
#else
  pollfd waiting {socket.native_handle(), POLLOUT, 0};

  // A signal can interrupt the wait, which is not the deadline and not a failure.
  int ready = 0;
  do
  {
    ready = ::poll(&waiting, 1, milliseconds);
  } while (ready < 0 && errno == EINTR);

  return ready > 0;
#endif
}

/// Whatever the last socket call set, as an asio error_code so it can be compared against
/// asio::error:: names on either platform.
[[nodiscard]] inline asio::error_code lastSocketError()
{
#if defined(_WIN32)
  return asio::error_code {::WSAGetLastError(), asio::error::get_system_category()};
#else
  return asio::error_code {errno, asio::error::get_system_category()};
#endif
}

/// Asks the operating system to dial, bypassing asio's own connect.
///
/// asio::basic_socket::connect goes through socket_ops::sync_connect, which on in_progress or
/// would_block calls poll_connect with a timeout of -1 and waits as long as the operating system
/// does. Unlike sync_send it takes no state carrying the socket's non-blocking flag, so there is no
/// mode to put the socket in that makes it hand the would_block back. It then reads SO_ERROR, which
/// for a local socket the kernel never queued is zero, and so reports a connection that does not
/// exist.
template <typename Socket, typename Endpoint>
[[nodiscard]] inline bool startDial(Socket& socket, const Endpoint& endpoint)
{
#if defined(_WIN32)
  const auto length = static_cast<int>(endpoint.size());
#else
  const auto length = static_cast<socklen_t>(endpoint.size());
#endif

  return ::connect(socket.native_handle(), endpoint.data(), length) == 0;
}

/// What a finished dial left on the socket. A non-blocking connect announces completion by making
/// the socket writable and leaves its verdict in SO_ERROR, which is the only place the cause can be
/// read: dialling a second time answers EISCONN once it worked and something platform-specific once
/// it did not, so neither says what went wrong.
template <typename Socket>
[[nodiscard]] inline asio::error_code dialOutcome(Socket& socket)
{
  int pending = 0;

#if defined(_WIN32)
  int length = static_cast<int>(sizeof(pending));

  if (::getsockopt(socket.native_handle(), SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&pending), &length) != 0)
  {
    return lastSocketError();
  }
#else
  socklen_t length = sizeof(pending);

  if (::getsockopt(socket.native_handle(), SOL_SOCKET, SO_ERROR, &pending, &length) != 0)
  {
    return lastSocketError();
  }
#endif

  return asio::error_code {pending, asio::error::get_system_category()};
}

}  // namespace detail

/// Writes the whole of `data` to a stream socket, giving up after `bound`, and answers how much
/// reached the kernel.
///
/// asio's own synchronous write cannot be bounded and a socket send timeout does not help: on
/// EAGAIN `socket_ops::sync_send` polls with a timeout of -1 and loops, swallowing both
/// (asio/detail/impl/socket_ops.ipp, "Wait for socket to become ready"). The socket has to be in
/// non-blocking mode for asio to hand the would_block back at all, and then the waiting is ours to
/// do and ours to bound.
///
/// `error` is `timed_out` when the time ran out. A stream peer then holds a partial record, so the
/// caller has to end the connection rather than write the rest or send the message again.
template <typename Socket>
[[nodiscard]] inline std::size_t writeBounded(Socket& socket,
                                              sen::Span<const char> data,
                                              std::chrono::milliseconds bound,
                                              asio::error_code& error)
{
  error = asio::error_code {};

  const auto deadline = std::chrono::steady_clock::now() + bound;
  std::size_t sent = 0U;

  while (sent < data.size())
  {
    asio::error_code writing;
    sent += socket.write_some(asio::buffer(data.data() + sent, data.size() - sent), writing);

    if (!writing)
    {
      continue;
    }

    if (writing != asio::error::would_block && writing != asio::error::try_again)
    {
      error = writing;
      return sent;
    }

    const auto left =
      std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());

    if (left.count() <= 0 || !detail::waitUntilWritable(socket, left))
    {
      error = asio::error::timed_out;
      return sent;
    }
  }

  return sent;
}

/// How long to leave a full listener backlog before asking it again. A local dial that the kernel
/// refused to queue has nothing to wait on, so this is a wait for room rather than for a verdict.
constexpr auto backlogRetry = std::chrono::milliseconds {10};

/// Dials `endpoint`, giving up after `bound`, and leaves the socket non-blocking because that is
/// what writeBounded needs of it.
///
/// The dial goes to the operating system rather than through asio, for the reason detail::startDial
/// gives, and the waiting is then ours to do and ours to bound. Unbounded, a host that drops SYNs
/// rather than refusing them holds the caller for the operating system's whole SYN budget, which is
/// over a minute on both Linux and macOS, and a handler sitting in that wait is what a shutdown
/// join waits for.
///
/// `error` is `timed_out` when the time ran out. Whatever was on the socket before is closed, so a
/// caller reconnecting need not close it first.
template <typename Socket, typename Endpoint>
[[nodiscard]] inline bool connectBounded(Socket& socket,
                                         const Endpoint& endpoint,
                                         std::chrono::milliseconds bound,
                                         asio::error_code& error)
{
  // Closing a socket the far end has already dropped can fail, and what that says does not matter:
  // what the caller is told is whether the dial below answered.
  asio::error_code ignored;
  std::ignore = socket.close(ignored);

  socket.open(endpoint.protocol(), error);

  if (error)
  {
    return false;
  }

  socket.non_blocking(true, error);

  if (error)
  {
    return false;
  }

  const auto deadline = std::chrono::steady_clock::now() + bound;
  const auto left = [&deadline]()
  { return std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()); };

  while (true)
  {
    if (detail::startDial(socket, endpoint))
    {
      error = asio::error_code {};
      return true;
    }

    error = detail::lastSocketError();

    // A dial already completed by an earlier pass round this loop.
    if (error == asio::error::already_connected)
    {
      error = asio::error_code {};
      return true;
    }

    // The kernel has it, so there is a verdict coming and the socket going writable is what
    // announces it.
    if (error == asio::error::in_progress || error == asio::error::already_started)
    {
      if (left().count() <= 0 || !detail::waitUntilWritable(socket, left()))
      {
        error = asio::error::timed_out;
        return false;
      }

      error = detail::dialOutcome(socket);

      return !error;
    }

    // Anything else is an answer: refused, no such socket file, no route.
    if (error != asio::error::would_block && error != asio::error::try_again)
    {
      return false;
    }

    // would_block from a dial means the listener's backlog is full and this was not queued at all.
    // There is no completion to wait for, so waiting for writability would return at once and spin;
    // the socket is writable without a peer. Leave it a moment and ask again.
    if (left() <= backlogRetry)
    {
      error = asio::error::timed_out;
      return false;
    }

    std::this_thread::sleep_for(backlogRetry);
  }
}

/// Dials the endpoints in turn until one answers. `bound` covers the whole walk rather than each
/// attempt, so a name with four addresses behind it does not cost four deadlines.
template <typename Socket, typename Endpoints>
[[nodiscard]] inline bool connectAnyBounded(Socket& socket,
                                            const Endpoints& endpoints,
                                            std::chrono::milliseconds bound,
                                            asio::error_code& error)
{
  const auto deadline = std::chrono::steady_clock::now() + bound;

  // What an empty range would mean. The transports refuse one at construction, so this is the
  // answer to a question nobody asks rather than a case to handle.
  error = asio::error::host_unreachable;

  for (const auto& entry: endpoints)
  {
    const auto left =
      std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());

    if (left.count() <= 0)
    {
      error = asio::error::timed_out;
      return false;
    }

    if (connectBounded(socket, entry.endpoint(), left, error))
    {
      return true;
    }
  }

  return false;
}

}  // namespace sen::components::influx

#endif  // SEN_COMPONENTS_INFLUX_SRC_BOUNDED_WRITE_H
