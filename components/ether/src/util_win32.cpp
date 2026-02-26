// === util_win32.cpp ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#if defined(_WIN32)

#  include "util.h"

// os
#  undef UNICODE
#  include <iphlpapi.h>
#  include <winsock2.h>
#  include <ws2ipdef.h>
#  pragma comment(lib, "IPHLPAPI.lib")

namespace sen::components::ether
{

namespace
{

/// Transforms a wide-char UTF-16 string to a UTF-8 string
std::string utf16ToUtf8(const wchar_t* input)
{
  const auto n = ::WideCharToMultiByte(CP_UTF8, 0, input, -1, nullptr, 0, nullptr, nullptr);

  std::string result(n - 1, '\0');

  ::WideCharToMultiByte(CP_UTF8, 0, input, -1, result.data(), n, nullptr, nullptr);

  return result;
}

}  // namespace

std::vector<NetworkInterfaceInfo> getLocalInterfaces(const Configuration& config)
{
  std::ignore = config;

  std::vector<NetworkInterfaceInfo> res;

  using Addr = IP_ADAPTER_UNICAST_ADDRESS_LH;
  using AddrList = IP_ADAPTER_ADDRESSES*;

  // from Windows doc:
  //   "The recommended method of calling the GetAdaptersAddresses
  //    function is to pre-allocate a 15KB working buffer pointed
  //    to by the AdapterAddresses parameter."
  DWORD outBufLen = 15000;  // NOLINT
  AddrList ifaddrs = (AddrList) new char[outBufLen];

  if (GetAdaptersAddresses(AF_UNSPEC,
                           GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER |  // NOLINT
                             GAA_FLAG_SKIP_FRIENDLY_NAME,                                                // NOLINT
                           nullptr,
                           ifaddrs,
                           &outBufLen) == 0)
  {
    for (AddrList addr = ifaddrs; addr != nullptr; addr = addr->Next)
    {
      if (addr->OperStatus == IfOperStatusUp)
      {
        for (Addr* uaddr = addr->FirstUnicastAddress; uaddr != nullptr; uaddr = uaddr->Next)
        {
          SOCKADDR* pSockAddr = uaddr->Address.lpSockaddr;
          if (pSockAddr->sa_family == AF_INET)
          {
            auto pInAddr = reinterpret_cast<sockaddr_in*>(pSockAddr);  // NOLINT NOSONAR

            NetworkInterfaceInfo info;
            info.address = asio::ip::make_address_v4(ntohl(pInAddr->sin_addr.s_addr));  // NOLINT NOSONAR
            info.deviceName = utf16ToUtf8(addr->FriendlyName);

            // if we have to use a specific network device, ignore the others
            if (!config.networkDevice.has_value() || config.networkDevice.value() == info.deviceName)
            {
              res.emplace_back(std::move(info));
              res.emplace_back();  // NOLINT NOSONAR
            }
            break;
          }
        }
      }
    }
  }

  delete[] ((char*)ifaddrs);  // NOLINT
  return res;
}

}  // namespace sen::components::ether

#endif
