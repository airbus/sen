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

#if defined(_WIN32)
#  include <winsock2.h>
#else
#  include <poll.h>

#  include <cerrno>
#endif

namespace sen::components::influx
{

/// How long a stream transport may spend handing one message to the kernel. The same figure
/// Telegraf's own influxdb output defaults its write timeout to, and far longer than a healthy
/// peer needs: a write only waits at all once the far end has stopped draining.
constexpr auto streamWriteBound = std::chrono::milliseconds {5000};

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

}  // namespace sen::components::influx

#endif  // SEN_COMPONENTS_INFLUX_SRC_BOUNDED_WRITE_H
