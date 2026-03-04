// === util.cpp ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "util.h"

// generated code
#include "stl/configuration.stl.h"

// asio
#include <asio/error_code.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/multicast.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>

// spdlog
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

// std
#include <stdexcept>
#include <tuple>

#ifdef __linux
#  include <sys/socket.h>
#endif

// std
#include <memory>

namespace sen::components::ether
{

std::shared_ptr<spdlog::logger> getLogger()
{
  constexpr auto* loggerName = "ether";
  static auto logger = spdlog::get(loggerName);
  if (!logger)
  {
    logger = spdlog::stdout_color_mt(loggerName);
  }
  return logger;
}

void configureMulticastSocket(asio::ip::udp::socket& socket,
                              asio::ip::udp::endpoint multicastEndpoint,
                              const MaybeDeviceName& deviceName,
                              const Configuration& config,
                              spdlog::logger* logger)
{
  {
    asio::error_code error;
    std::ignore = socket.open(multicastEndpoint.protocol(), error);

    if (error)
    {
      logger->error("could not open multicast socket to {}:{} : {}",
                    multicastEndpoint.address().to_string(),
                    multicastEndpoint.port(),
                    error.message());

      throw std::runtime_error(error.message());
    }
  }

  // we use any interface by default
  asio::ip::address_v4 interfaceAddress = asio::ip::address_v4::any();

  // receive messages from other processes in this host. This will create some unwanted
  // traffic from ourselves, but it's a trade-off to be able to communicate using this channel.
  //
  // Idea for the future: replace multicast by some other IPC mechanism for same-host Sen processes.
  {
    asio::error_code error;
    std::ignore = socket.set_option(asio::ip::multicast::enable_loopback(true), error);
    if (error)
    {
      logger->error("could not enable loopback multicast for socket to {}:{} : {}",
                    multicastEndpoint.address().to_string(),
                    multicastEndpoint.port(),
                    error.message());

      throw std::runtime_error(error.message());
    }
  }

  // do some special handling if we need to bind the socket to a given interface
  if (deviceName.has_value())
  {
    const std::string& device = deviceName.value();
    bool interfaceFound = false;

    // find the interface that corresponds to the given device name
    static auto interfaces = getLocalInterfaces(config);
    for (const auto& elem: interfaces)
    {
      if (elem.deviceName == device)
      {
        interfaceAddress = elem.address.to_v4();
        interfaceFound = true;
        break;
      }
    }

    // if no interface is found, complain
    if (!interfaceFound)
    {
      logger->error("did not find any interface corresponding to device '{}'", device);
      throw std::runtime_error("did not find network device");
    }
  }

  // reuse address and bind the socket to the interface
  {
    asio::error_code error;
    std::ignore = socket.set_option(asio::ip::udp::socket::reuse_address(true), error);

    if (error)
    {
      logger->error("could not open set the reuse_address option to multicast socket on {}:{} : {}",
                    multicastEndpoint.address().to_string(),
                    multicastEndpoint.port(),
                    error.message());

      throw std::runtime_error(error.message());
    }
  }

  // bind the socket to the multicast endpoint
  {
    asio::error_code error;

#ifdef _WIN32
    const auto bindAddress = asio::ip::address_v4::any();  // Windows doesn't allow binding to multicast groups
#elif defined __linux
    const auto bindAddress = multicastEndpoint.address();
#endif

    std::ignore = socket.bind({bindAddress, multicastEndpoint.port()}, error);

    if (error)
    {
      logger->error("could bind a multicast socket to port {} : {}", multicastEndpoint.port(), error.message());

      throw std::runtime_error(error.message());
    }
  }

  // join the multicast group.
  {
    asio::error_code error;

    std::ignore =
      socket.set_option(asio::ip::multicast::join_group(multicastEndpoint.address().to_v4(), interfaceAddress), error);

    if (error)
    {
      logger->error(
        "could not join multicast group. If you don't have any network interface, please ensure that\n"
        "you have the loop-back interface enabled and with multicast support. In linux, you can try with: \n"
        "   sudo ip link set lo multicast on\n"
        "   sudo ip route add 224.0.0.0/4 dev lo\n"
        "   sudo ip route add ff00::/8 dev lo\n"
        "\n"
        "error message: {}",
        error.message());

      throw std::runtime_error(error.message());
    }
  }

  if (deviceName.has_value())
  {
    asio::error_code error;
    std::ignore = socket.set_option(asio::ip::multicast::outbound_interface(interfaceAddress), error);

    if (error)
    {
      logger->error("could not set the multicast interface {}: {}", interfaceAddress.to_string(), error.message());
      throw std::runtime_error(error.message());
    }

#ifdef __linux__
    if (::setsockopt(socket.native_handle(),
                     SOL_SOCKET,       // NOLINT(misc-include-cleaner)
                     SO_BINDTODEVICE,  // NOLINT(misc-include-cleaner)
                     deviceName.value().data(),
                     deviceName.value().size() + 1) < 0)
    {
      std::string err;
      err.append("could not bind multicast socket of group ")
        .append(interfaceAddress.to_string())
        .append(" to device ")
        .append(deviceName.value());

      throw std::runtime_error(err);
    }
#endif
  }
}

void configureTcpSocket(asio::ip::tcp::socket& socket, const Configuration& config)
{
  if (config.networkDevice.has_value())
  {
#ifdef __linux__
    if (::setsockopt(socket.native_handle(),
                     SOL_SOCKET,       // NOLINT(misc-include-cleaner)
                     SO_BINDTODEVICE,  // NOLINT(misc-include-cleaner)
                     config.networkDevice.value().data(),
                     config.networkDevice.value().size() + 1) < 0)
    {
      throw std::runtime_error(std::string("could not bind tcp socket to ").append(config.networkDevice.value()));
    }
#endif
  }
}

}  // namespace sen::components::ether
