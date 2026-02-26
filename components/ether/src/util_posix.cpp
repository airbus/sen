// === util_posix.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// sen
#include "sen/core/base/class_helpers.h"  // NOLINT(misc-include-cleaner)
#include "sen/core/base/numbers.h"

// generated code
#include "stl/configuration.stl.h"

// asio
#include <asio/ip/address.hpp>     // NOLINT(misc-include-cleaner)
#include <asio/ip/address_v4.hpp>  // NOLINT(misc-include-cleaner)
#include <asio/ip/multicast.hpp>   // NOLINT(misc-include-cleaner)
#include <asio/ip/udp.hpp>         // NOLINT(misc-include-cleaner)

// std
#include <utility>
#include <variant>
#include <vector>

#if defined(__APPLE__) || defined(__linux__)

// system
#  include "util.h"

// os
#  include <ifaddrs.h>     // NOLINT(misc-include-cleaner)
#  include <net/if.h>      // NOLINT(misc-include-cleaner)
#  include <sys/socket.h>  // NOLINT(misc-include-cleaner)

namespace sen::components::ether
{

std::vector<NetworkInterfaceInfo> getLocalInterfaces(const Configuration& config)
{
  std::vector<NetworkInterfaceInfo> res;

  ifaddrs* ifs;
  if (getifaddrs(&ifs) != 0)
  {
    return res;
  }

  const auto allowVirtualInterfaces =
    std::visit(::sen::Overloaded {[](const auto& value) { return value.allowVirtualInterfaces; }}, config.discovery);

  for (auto* addr = ifs; addr != nullptr; addr = addr->ifa_next)
  {
    // No address? Skip.
    if (addr->ifa_addr == nullptr)
    {
      continue;
    }

    // if we have to use a specific network device, ignore the others
    if (config.networkDevice.has_value() && config.networkDevice.value() != addr->ifa_name)
    {
      continue;
    }

    // require INET
    if (addr->ifa_addr->sa_family != AF_INET)
    {
      continue;
    }

    // require active interface
    if ((addr->ifa_flags & IFF_UP) == 0)
    {
      continue;
    }

    // require multicast support
    if ((addr->ifa_flags & IFF_MULTICAST) == 0)
    {
      continue;
    }

    // check if virtual and not allowed
    constexpr u32 iffLowerUp = 0x10000;
    if (!allowVirtualInterfaces && (addr->ifa_flags & iffLowerUp) == 0)
    {
      continue;
    }

    // fill in the result
    NetworkInterfaceInfo info;
    {
      auto* ifaAddr = reinterpret_cast<sockaddr_in*>(addr->ifa_addr);             // NOLINT NOSONAR
      info.address = asio::ip::make_address_v4(ntohl(ifaAddr->sin_addr.s_addr));  // NOLINT(misc-include-cleaner)
      info.deviceName = addr->ifa_name;
    }

    res.emplace_back(std::move(info));
  }

  freeifaddrs(ifs);
  return res;
}

}  // namespace sen::components::ether

#endif  // defined(__APPLE__) || defined(__linux__)
