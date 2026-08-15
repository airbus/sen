// === browser_open.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "browser_open.h"

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <netdb.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

namespace sen::cli_run
{

namespace
{

constexpr auto pollInterval = std::chrono::milliseconds(100);

[[nodiscard]] bool tryConnect(const std::string& host, int port)
{
  addrinfo hints {};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* res = nullptr;
  if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0)
  {
    return false;
  }

  bool ok = false;
  for (auto* p = res; p != nullptr; p = p->ai_next)
  {
#ifdef _WIN32
    const auto sock = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sock == INVALID_SOCKET)
      continue;
    if (::connect(sock, p->ai_addr, static_cast<int>(p->ai_addrlen)) == 0)
    {
      ok = true;
    }
    ::closesocket(sock);
#else
    const auto sock = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sock < 0)
      continue;
    if (::connect(sock, p->ai_addr, p->ai_addrlen) == 0)
    {
      ok = true;
    }
    ::close(sock);
#endif
    if (ok)
      break;
  }
  freeaddrinfo(res);
  return ok;
}

}  // namespace

bool waitForTcpListening(const std::string& host, int port, std::chrono::milliseconds timeout)
{
#ifdef _WIN32
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    return false;
#endif

  const auto deadline = std::chrono::steady_clock::now() + timeout;
  bool ready = false;
  while (std::chrono::steady_clock::now() < deadline)
  {
    if (tryConnect(host, port))
    {
      ready = true;
      break;
    }
    std::this_thread::sleep_for(pollInterval);
  }

#ifdef _WIN32
  WSACleanup();
#endif
  return ready;
}

bool openInBrowser(const std::string& url)
{
  std::string cmd;
#if defined(__APPLE__)
  cmd = "open '" + url + "' >/dev/null 2>&1";
#elif defined(_WIN32)
  // `start` is a cmd builtin; the empty "" is the window title (start treats the first
  // quoted arg as a title).
  cmd = "cmd /c start \"\" \"" + url + "\"";
#else
  cmd = "xdg-open '" + url + "' >/dev/null 2>&1";
#endif
  return std::system(cmd.c_str()) == 0;
}

}  // namespace sen::cli_run
