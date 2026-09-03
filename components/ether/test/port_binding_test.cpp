// === port_binding_test.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "network_exclusion.h"
#include "port_binding.h"

// sen
#include "sen/core/base/assert.h"

// generated code
#include "stl/configuration.stl.h"

// asio
#include <asio/error.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <asio/socket_base.hpp>

// gtest
#include <gtest/gtest.h>

// std
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace sen::components::ether
{

namespace
{

[[nodiscard]] Configuration makeDefaultConfiguration(MaybePortConfig portConfig)
{
  return Configuration {DiscoveryConfig {},
                        QueueConfig {},
                        QueueConfig {},
                        QueueConfig {},
                        BusConfig {},
                        PortExclusions {},
                        std::move(portConfig),
                        MaybeDiscoveryHubPort {},
                        MaybeDeviceName {}};
}

[[nodiscard]] PortConfig makePortConfig(PortKind kind, PortBinding binding)
{
  switch (kind)
  {
    case PortKind::tcpAcceptor:
      return PortConfig {std::move(binding), Ephemeral {}, Ephemeral {}};
    case PortKind::udpUnicast:
      return PortConfig {Ephemeral {}, std::move(binding), Ephemeral {}};
    case PortKind::tcpSource:
      return PortConfig {Ephemeral {}, Ephemeral {}, std::move(binding)};
  }
  sen::throwRuntimeError("unknown port kind");
}

[[nodiscard]] Configuration makeConfigurationWithPort(PortKind kind, PortBinding binding)
{
  return makeDefaultConfiguration(MaybePortConfig {makePortConfig(kind, std::move(binding))});
}

[[nodiscard]] asio::error_code bindTcpAcceptor(asio::ip::tcp::acceptor& acceptor, uint16_t port)
{
  asio::error_code error;
  if (!acceptor.is_open())
  {
    std::ignore = acceptor.open(asio::ip::tcp::v4(), error);
  }
  if (!error)
  {
    std::ignore = acceptor.bind(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port), error);
  }
  if (!error)
  {
    std::ignore = acceptor.listen(asio::socket_base::max_listen_connections, error);
  }
  return error;
}

[[nodiscard]] asio::error_code bindTcpSocket(asio::ip::tcp::socket& socket, uint16_t port)
{
  asio::error_code error;
  if (!socket.is_open())
  {
    std::ignore = socket.open(asio::ip::tcp::v4(), error);
  }
  if (!error)
  {
    std::ignore = socket.bind(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port), error);
  }
  return error;
}

[[nodiscard]] asio::error_code bindUdpSocket(asio::ip::udp::socket& socket, uint16_t port)
{
  asio::error_code error;
  if (!socket.is_open())
  {
    std::ignore = socket.open(asio::ip::udp::v4(), error);
  }
  if (!error)
  {
    std::ignore = socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), port), error);
  }
  return error;
}

class TcpPortReservation final
{
public:
  explicit TcpPortReservation(uint16_t port = 0): acceptor_(io_)
  {
    const auto error = bindTcpAcceptor(acceptor_, port);
    if (error)
    {
      sen::throwRuntimeError("failed to reserve TCP port for test: " + error.message());
    }
  }

  [[nodiscard]] uint16_t port() const { return acceptor_.local_endpoint().port(); }

private:
  asio::io_context io_;
  asio::ip::tcp::acceptor acceptor_;
};

/// Helper to get an available TCP port from OS (and then use manually on tests)
[[nodiscard]] uint16_t pickFreeTcpPort()
{
  asio::io_context io;
  asio::ip::tcp::acceptor acceptor(io);
  if (const auto error = bindTcpAcceptor(acceptor, 0))
  {
    sen::throwRuntimeError("failed to pick TCP port for test: " + error.message());
  }

  return acceptor.local_endpoint().port();
}

[[nodiscard]] uint16_t pickFreeUdpPort()
{
  asio::io_context io;
  asio::ip::udp::socket socket(io);
  if (const auto error = bindUdpSocket(socket, 0))
  {
    sen::throwRuntimeError("failed to pick UDP port for test: " + error.message());
  }

  return socket.local_endpoint().port();
}

}  // namespace

/// @test
/// If there is no configuration, uses ephemeral mode
/// @requirements(SEN-909)
TEST(PortBinding, UsesEphemeralByDefault)
{
  const auto config = makeDefaultConfiguration(MaybePortConfig {});
  PortExclusionSources exclusions;
  asio::io_context io;
  asio::ip::tcp::acceptor acceptor(io);

  bindConfiguredPort(config,
                     exclusions,
                     PortKind::tcpAcceptor,
                     [&](uint16_t port)
                     {
                       EXPECT_EQ(port, 0);
                       return bindTcpAcceptor(acceptor, port);
                     });

  EXPECT_GT(acceptor.local_endpoint().port(), 0);
}

/// @test
/// Checks ephemeral mode configuration
/// @requirements(SEN-909)
TEST(PortBinding, BindsEphemeralPort)
{
  const auto configTcpSource = makeConfigurationWithPort(PortKind::tcpSource, Ephemeral {});
  const auto configTcpAcceptor = makeConfigurationWithPort(PortKind::tcpAcceptor, Ephemeral {});
  const auto configUdpUnicast = makeConfigurationWithPort(PortKind::udpUnicast, Ephemeral {});
  PortExclusionSources exclusions;
  asio::io_context ioTcpSource;
  asio::io_context ioTcpAcceptor;
  asio::io_context ioUdpUnicast;
  asio::ip::tcp::socket socketSource(ioTcpSource);
  asio::ip::tcp::acceptor socketAcceptor(ioTcpAcceptor);
  asio::ip::udp::socket socketUdp(ioUdpUnicast);

  bindConfiguredPort(configTcpSource,
                     exclusions,
                     PortKind::tcpSource,
                     [&](uint16_t port)
                     {
                       EXPECT_EQ(port, 0);
                       return bindTcpSocket(socketSource, port);
                     });

  bindConfiguredPort(configTcpAcceptor,
                     exclusions,
                     PortKind::tcpAcceptor,
                     [&](uint16_t port)
                     {
                       EXPECT_EQ(port, 0);
                       return bindTcpAcceptor(socketAcceptor, port);
                     });

  bindConfiguredPort(configUdpUnicast,
                     exclusions,
                     PortKind::udpUnicast,
                     [&](uint16_t port)
                     {
                       EXPECT_EQ(port, 0);
                       return bindUdpSocket(socketUdp, port);
                     });

  EXPECT_GT(socketSource.local_endpoint().port(), 0);
  EXPECT_GT(socketAcceptor.local_endpoint().port(), 0);
  EXPECT_GT(socketUdp.local_endpoint().port(), 0);
}

/// @test
/// Checks pin mode configuration
/// @requirements(SEN-909)
TEST(PortBinding, BindsPinnedPort)
{
  const auto portTcpSource = pickFreeTcpPort();
  const auto portTcpAcceptor = pickFreeTcpPort();
  const auto portUdpUnicast = pickFreeUdpPort();
  const auto configTcpSource = makeConfigurationWithPort(PortKind::tcpSource, PinnedPort {portTcpSource});
  const auto configTcpAcceptor = makeConfigurationWithPort(PortKind::tcpAcceptor, PinnedPort {portTcpAcceptor});
  const auto configUdpUnicast = makeConfigurationWithPort(PortKind::udpUnicast, PinnedPort {portUdpUnicast});
  PortExclusionSources exclusions;
  asio::io_context ioTcpSource;
  asio::io_context ioTcpAcceptor;
  asio::io_context ioUdpUnicast;
  asio::ip::tcp::socket socketSource(ioTcpSource);
  asio::ip::tcp::acceptor socketAcceptor(ioTcpAcceptor);
  asio::ip::udp::socket socketUdp(ioUdpUnicast);

  bindConfiguredPort(
    configTcpSource, exclusions, PortKind::tcpSource, [&](uint16_t port) { return bindTcpSocket(socketSource, port); });

  bindConfiguredPort(configTcpAcceptor,
                     exclusions,
                     PortKind::tcpAcceptor,
                     [&](uint16_t port) { return bindTcpAcceptor(socketAcceptor, port); });

  bindConfiguredPort(
    configUdpUnicast, exclusions, PortKind::udpUnicast, [&](uint16_t port) { return bindUdpSocket(socketUdp, port); });

  EXPECT_GT(socketSource.local_endpoint().port(), 0);
  EXPECT_GT(socketAcceptor.local_endpoint().port(), 0);
  EXPECT_GT(socketUdp.local_endpoint().port(), 0);
}

/// @test
/// Checks probe mode configuration
/// @requirements(SEN-909)
TEST(PortBinding, BindsProbePort)
{
  const auto portTcpSource = pickFreeTcpPort();
  const auto portTcpAcceptor = pickFreeTcpPort();
  const auto portUdpUnicast = pickFreeUdpPort();
  const auto configTcpSource =
    makeConfigurationWithPort(PortKind::tcpSource, ProbePortRange {portTcpSource, portTcpSource});
  const auto configTcpAcceptor =
    makeConfigurationWithPort(PortKind::tcpAcceptor, ProbePortRange {portTcpAcceptor, portTcpAcceptor});
  const auto configUdpUnicast =
    makeConfigurationWithPort(PortKind::udpUnicast, ProbePortRange {portUdpUnicast, portUdpUnicast});
  PortExclusionSources exclusions;
  asio::io_context ioTcpSource;
  asio::io_context ioTcpAcceptor;
  asio::io_context ioUdpUnicast;
  asio::ip::tcp::socket socketSource(ioTcpSource);
  asio::ip::tcp::acceptor socketAcceptor(ioTcpAcceptor);
  asio::ip::udp::socket socketUdp(ioUdpUnicast);

  bindConfiguredPort(
    configTcpSource, exclusions, PortKind::tcpSource, [&](uint16_t port) { return bindTcpSocket(socketSource, port); });

  bindConfiguredPort(configTcpAcceptor,
                     exclusions,
                     PortKind::tcpAcceptor,
                     [&](uint16_t port) { return bindTcpAcceptor(socketAcceptor, port); });

  bindConfiguredPort(
    configUdpUnicast, exclusions, PortKind::udpUnicast, [&](uint16_t port) { return bindUdpSocket(socketUdp, port); });

  EXPECT_EQ(socketSource.local_endpoint().port(), portTcpSource);
}

/// @test
/// Makes a pin configuration for tcpAcceptor but check that udpUnicast is in ephemeral mode
/// @requirements(SEN-909)
TEST(PortBinding, UsesEphemeralForUnsetPorts)
{
  const auto config = makeConfigurationWithPort(PortKind::tcpAcceptor, PinnedPort {pickFreeTcpPort()});
  PortExclusionSources exclusions;
  asio::io_context io;
  asio::ip::udp::socket socket(io);

  bindConfiguredPort(config,
                     exclusions,
                     PortKind::udpUnicast,
                     [&](uint16_t port)
                     {
                       EXPECT_EQ(port, 0);
                       return bindUdpSocket(socket, port);
                     });

  EXPECT_GT(socket.local_endpoint().port(), 0);
}

/// @test
/// Checks that making a pin configuration in an excluded port fails
/// @requirements(SEN-909)
TEST(PortBinding, RejectsExcludedPinnedPort)
{
  const auto pinnedPort = pickFreeTcpPort();
  const auto config = makeConfigurationWithPort(PortKind::tcpSource, PinnedPort {pinnedPort});
  PortExclusionSources exclusions;
  exclusions.configured.add(pinnedPort, pinnedPort);
  asio::io_context io;
  asio::ip::tcp::socket socket(io);
  bool bindCalled = false;

  try
  {
    bindConfiguredPort(config,
                       exclusions,
                       PortKind::tcpSource,
                       [&](uint16_t port)
                       {
                         bindCalled = true;
                         return bindTcpSocket(socket, port);
                       });
    FAIL() << "Expected pinned port binding to fail";
  }
  catch (const std::exception& error)
  {
    const std::string message = error.what();
    EXPECT_NE(message.find("TCP source"), std::string::npos);
    EXPECT_NE(message.find("configured exclusions"), std::string::npos);
  }

  EXPECT_FALSE(bindCalled);
}

/// @test
/// Keep a TCP port busy and check that a pinned bind to the same port fails
/// @requirements(SEN-909)
TEST(PortBinding, FailsWhenPinnedPortIsTaken)
{
  const TcpPortReservation reservedPort;
  const auto config = makeConfigurationWithPort(PortKind::tcpAcceptor, PinnedPort {reservedPort.port()});
  PortExclusionSources exclusions;
  asio::io_context io;
  asio::ip::tcp::acceptor acceptor(io);
  std::vector<uint16_t> attempts;

  EXPECT_THROW(bindConfiguredPort(config,
                                  exclusions,
                                  PortKind::tcpAcceptor,
                                  [&](uint16_t port)
                                  {
                                    attempts.push_back(port);
                                    return bindTcpAcceptor(acceptor, port);
                                  }),
               std::exception);

  ASSERT_EQ(attempts.size(), 1U);
  EXPECT_EQ(attempts.front(), reservedPort.port());
}

/// @test
/// Checks that probe skips ports excluded by configuration
/// @requirements(SEN-909)
TEST(PortBinding, SkipsExcludedProbePort)
{
  const auto expectedPort = pickFreeTcpPort();
  const auto excludedPort = static_cast<uint16_t>(expectedPort - 1U);
  const auto config = makeConfigurationWithPort(PortKind::tcpAcceptor, ProbePortRange {excludedPort, expectedPort});
  PortExclusionSources exclusions;
  exclusions.configured.add(excludedPort, excludedPort);
  asio::io_context io;
  asio::ip::tcp::acceptor acceptor(io);

  bindConfiguredPort(
    config, exclusions, PortKind::tcpAcceptor, [&](uint16_t port) { return bindTcpAcceptor(acceptor, port); });

  EXPECT_EQ(acceptor.local_endpoint().port(), expectedPort);
}

/// @test
/// Checks that probe tries another port when a candidate is already in use
/// @requirements(SEN-909)
TEST(PortBinding, RetriesProbePort)
{
  const auto config = makeConfigurationWithPort(PortKind::tcpAcceptor, ProbePortRange {20000, 20002});
  PortExclusionSources exclusions;
  std::vector<uint16_t> attempts;

  bindConfiguredPort(config,
                     exclusions,
                     PortKind::tcpAcceptor,
                     [&](uint16_t port)
                     {
                       attempts.push_back(port);
                       return port == 20001 ? asio::error_code {} : asio::error::address_in_use;
                     });

  ASSERT_FALSE(attempts.empty());
  EXPECT_EQ(attempts.back(), 20001);
  EXPECT_LE(attempts.size(), 3U);
  for (const auto port: attempts)
  {
    EXPECT_GE(port, 20000);
    EXPECT_LE(port, 20002);
  }
}

/// @test
/// Checks that probe fails when every candidate port is unavailabe
/// @requirements(SEN-909)
TEST(PortBinding, FailsProbeRange)
{
  const TcpPortReservation reservedPort;
  const auto config =
    makeConfigurationWithPort(PortKind::tcpAcceptor, ProbePortRange {reservedPort.port(), reservedPort.port()});
  PortExclusionSources exclusions;
  asio::io_context io;
  asio::ip::tcp::acceptor acceptor(io);
  std::vector<uint16_t> attempts;

  EXPECT_THROW(bindConfiguredPort(config,
                                  exclusions,
                                  PortKind::tcpAcceptor,
                                  [&](uint16_t port)
                                  {
                                    attempts.push_back(port);
                                    return bindTcpAcceptor(acceptor, port);
                                  }),
               std::exception);

  ASSERT_EQ(attempts.size(), 1U);
  EXPECT_EQ(attempts.front(), reservedPort.port());
}

/// @test
/// Checks that probe diagnostics explain excluded ranges
/// @requirements(SEN-909)
TEST(PortBinding, ReportsExcludedProbeRange)
{
  const auto config = makeConfigurationWithPort(PortKind::tcpAcceptor, ProbePortRange {20000, 20002});
  PortExclusionSources exclusions;
  exclusions.configured.add(20000, 20001);
  exclusions.os.add(20002, 20002);
  bool bindCalled = false;
  std::string message;

  try
  {
    bindConfiguredPort(config,
                       exclusions,
                       PortKind::tcpAcceptor,
                       [&](uint16_t port)
                       {
                         std::ignore = port;
                         bindCalled = true;
                         return asio::error_code {};
                       });
    FAIL() << "Expected probe range binding to fail";
  }
  catch (const std::exception& error)
  {
    message = error.what();
  }

  EXPECT_FALSE(bindCalled);
  EXPECT_NE(message.find("TCP acceptor"), std::string::npos);
  EXPECT_NE(message.find("20000-20002"), std::string::npos);
  EXPECT_NE(message.find("all ports are excluded"), std::string::npos);
  EXPECT_NE(message.find("excluded ports: 3"), std::string::npos);
  EXPECT_NE(message.find("configured: 20000-20001"), std::string::npos);
  EXPECT_NE(message.find("OS: 20002"), std::string::npos);
  EXPECT_NE(message.find("bind attempts: 0"), std::string::npos);
}

/// @test
/// Checks that probe diagnostics explain exhausted bind attempts
/// @requirements(SEN-909)
TEST(PortBinding, ReportsUnavailableProbeRange)
{
  const auto config = makeConfigurationWithPort(PortKind::tcpAcceptor, ProbePortRange {20000, 20002});
  PortExclusionSources exclusions;
  exclusions.configured.add(20000, 20000);
  std::vector<uint16_t> attempts;
  std::string message;

  try
  {
    bindConfiguredPort(config,
                       exclusions,
                       PortKind::tcpAcceptor,
                       [&](uint16_t port)
                       {
                         attempts.push_back(port);
                         return asio::error::address_in_use;
                       });
    FAIL() << "Expected probe range binding to fail";
  }
  catch (const std::exception& error)
  {
    message = error.what();
  }

  ASSERT_EQ(attempts.size(), 2U);
  EXPECT_NE(message.find("TCP acceptor"), std::string::npos);
  EXPECT_NE(message.find("20000-20002"), std::string::npos);
  EXPECT_NE(message.find("no port was available"), std::string::npos);
  EXPECT_NE(message.find("bind attempts: 2"), std::string::npos);
  EXPECT_NE(message.find("excluded ports: 1"), std::string::npos);
  EXPECT_NE(message.find("configured: 20000"), std::string::npos);
  EXPECT_NE(message.find("last attempted port: " + std::to_string(attempts.back())), std::string::npos);
  EXPECT_NE(message.find("last error:"), std::string::npos);
}

/// @test
/// Checks that an invalid probe range fails before binding
/// @requirements(SEN-909)
TEST(PortBinding, RejectsInvalidProbeRange)
{
  const auto config = makeConfigurationWithPort(PortKind::tcpSource, ProbePortRange {20002, 20000});
  PortExclusionSources exclusions;
  asio::io_context io;
  asio::ip::tcp::socket socket(io);
  bool bindCalled = false;

  EXPECT_ANY_THROW(bindConfiguredPort(config,
                                      exclusions,
                                      PortKind::tcpSource,
                                      [&](uint16_t port)
                                      {
                                        bindCalled = true;
                                        return bindTcpSocket(socket, port);
                                      }));

  EXPECT_FALSE(bindCalled);
}

/// @test
/// Checks that probe stops on errors that are not caused by an occupied port
/// @requirements(SEN-909)
TEST(PortBinding, StopsProbeOnError)
{
  const auto config = makeConfigurationWithPort(PortKind::udpUnicast, ProbePortRange {20000, 20000});
  PortExclusionSources exclusions;
  std::vector<uint16_t> attempts;

  EXPECT_THROW(bindConfiguredPort(config,
                                  exclusions,
                                  PortKind::udpUnicast,
                                  [&](uint16_t port)
                                  {
                                    attempts.push_back(port);
                                    return asio::error::operation_not_supported;
                                  }),
               std::exception);

  ASSERT_EQ(attempts.size(), 1U);
  EXPECT_EQ(attempts.front(), 20000);
}

}  // namespace sen::components::ether
