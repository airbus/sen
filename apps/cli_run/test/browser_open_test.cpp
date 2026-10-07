// === browser_open_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "browser_open.h"

// gtest
#include <gtest/gtest.h>

// std
#include <atomic>
#include <chrono>
#include <string>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

namespace
{

/// A socket listening on a port the operating system chooses, so two tests running at once
/// cannot pick the same one.
class Listener
{
public:
  Listener()
  {
#ifdef _WIN32
    WSADATA data;
    WSAStartup(MAKEWORD(2, 2), &data);
#endif
    socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    ::bind(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address));  // NOLINT

    sockaddr_in bound {};
#ifdef _WIN32
    int length = sizeof(bound);
#else
    socklen_t length = sizeof(bound);
#endif
    ::getsockname(socket_, reinterpret_cast<sockaddr*>(&bound), &length);  // NOLINT
    port_ = ::ntohs(bound.sin_port);
    ::listen(socket_, 1);
  }

  ~Listener()
  {
#ifdef _WIN32
    ::closesocket(socket_);
    WSACleanup();
#else
    ::close(socket_);
#endif
  }

  Listener(const Listener&) = delete;
  Listener& operator=(const Listener&) = delete;
  Listener(Listener&&) = delete;
  Listener& operator=(Listener&&) = delete;

  [[nodiscard]] int port() const { return port_; }

private:
#ifdef _WIN32
  SOCKET socket_ {};
#else
  int socket_ {};
#endif
  int port_ {};
};

/// @test
/// Returns true once the port accepts a connection, which is what delays the browser until
/// the explorer is reachable.
TEST(BrowserOpen, ReportsAPortThatAccepts)
{
  const Listener listener;
  const std::atomic<bool> cancelled {false};

  EXPECT_TRUE(
    sen::cli_run::waitForTcpListening("127.0.0.1", listener.port(), std::chrono::milliseconds {2000}, cancelled));
}

/// @test
/// Returns false when nothing ever listens, so a browser is not opened onto a port that
/// never came up.
TEST(BrowserOpen, GivesUpOnAPortThatNeverAccepts)
{
  const std::atomic<bool> cancelled {false};

  // Port 1 needs privilege to bind and nothing in a test environment listens on it, so the
  // connect fails for the whole window rather than racing with something that might.
  EXPECT_FALSE(sen::cli_run::waitForTcpListening("127.0.0.1", 1, std::chrono::milliseconds {300}, cancelled));
}

/// @test
/// Returns false at once when the caller is already shutting down, rather than holding the
/// shutdown for the length of the timeout.
TEST(BrowserOpen, StopsWhenTheCallerIsCancelled)
{
  const std::atomic<bool> cancelled {true};

  const auto started = std::chrono::steady_clock::now();
  EXPECT_FALSE(sen::cli_run::waitForTcpListening("127.0.0.1", 1, std::chrono::milliseconds {10000}, cancelled));

  EXPECT_LT(std::chrono::steady_clock::now() - started, std::chrono::milliseconds {5000});
}

}  // namespace
