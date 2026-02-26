// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "discovery.h"
#include "ether_transport.h"
#include "tcp_discovery_hub.h"
#include "util.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/result.h"
#include "sen/core/meta/var.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/tracer.h"

// generated code
#include "stl/configuration.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <variant>

// asio
#include <asio/io_context.hpp>

namespace sen::components::ether
{

namespace
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

constexpr const char* defaultDiscoveryGroup = "239.255.0.44";
constexpr uint16_t defaultDiscoveryPort = 60543;
constexpr auto defaultBeamingPeriod = std::chrono::milliseconds(1000);
constexpr uint64_t defaultBusWarningLevel = 100U;
constexpr uint16_t defaultMulticastPort = 50985;
constexpr uint8_t multicastMin = 224;
constexpr uint8_t multicastMax = 239;

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

[[nodiscard]] bool charIsEqualsIgnoreCase(char a, char b) noexcept
{
  return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

[[nodiscard]] bool isEqualsIgnoreCase(const std::string& a, const std::string& b) noexcept
{
  return std::equal(a.begin(), a.end(), b.begin(), b.end(), charIsEqualsIgnoreCase);
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// EtherComponent
//--------------------------------------------------------------------------------------------------------------

class EtherComponent: public kernel::Component
{
  SEN_NOCOPY_NOMOVE(EtherComponent)

public:
  EtherComponent() = default;
  ~EtherComponent() override = default;

public:
  kernel::FuncResult preload(kernel::PreloadApi&& api) override
  {
    if (auto result = readConfig(api.getConfig()); result.isError())
    {
      return result;
    }

    discovery_ = DiscoverySystem::make(config_, io_);

    // run the discovery hub if needed
    if (config_.runDiscoveryHub.has_value())
    {
      discoveryHub_ = std::make_unique<TcpDiscoveryHub>(io_, config_);
    }

    const auto& appName = api.getAppName();
    api.installTransportFactory(
      [this, appName](const auto& session, std::unique_ptr<sen::kernel::Tracer> tracer)
      { return std::make_unique<EtherTransport>(config_, session, appName, discovery_, std::move(tracer)); },
      etherProtocolVersion);
    return done();
  }

  kernel::FuncResult run(kernel::RunApi& api) override
  {
    discovery_->prepare(api);
    while (!api.stopRequested())
    {
      io_.run_one_for(std::chrono::seconds(1));
    }
    io_.stop();
    return done();
  }

  kernel::FuncResult unload(kernel::UnloadApi&& api) override
  {
    std::ignore = api;
    return done();
  }

  void postUnloadCleanup() override
  {
    discovery_.reset();
    discoveryHub_.reset();
  }

  [[nodiscard]] bool isRealTimeOnly() const noexcept override { return true; }

private:
  kernel::FuncResult readConfig(const VarMap& params)
  {
    // set default values
    config_.discovery = MulticastDiscovery {};
    config_.busOutQueue.warningLevel = defaultBusWarningLevel;
    config_.busConfig.multicastDisabled = false;
    config_.busConfig.multicastPort = defaultMulticastPort;
    config_.busConfig.multicastRange[0] = ByteRange {multicastMin, multicastMax};
    config_.busConfig.multicastRange[1] = ByteRange {0, 255};
    config_.busConfig.multicastRange[2] = ByteRange {0, 255};
    config_.busConfig.multicastRange[3] = ByteRange {0, 255};

    // read values from config
    VariantTraits<Configuration>::variantToValue(params, config_);

    if (config_.busConfig.multicastRange[0].min < multicastMin)
    {
      return Err(kernel::ExecError {kernel::ErrorCategory::expectationsNotMet,
                                    "invalid multicast address range. First byte should be >= 224"});
    }

    if (config_.busConfig.multicastRange[0].max > multicastMax)
    {
      return Err(kernel::ExecError {kernel::ErrorCategory::expectationsNotMet,
                                    "invalid multicast address range. First byte should be <= 239"});
    }

    for (const auto [min, max]: config_.busConfig.multicastRange)
    {
      if (min > max)
      {
        return Err(
          kernel::ExecError {kernel::ErrorCategory::expectationsNotMet, "invalid multicast address range (min > max)"});
      }
    }

    if (std::holds_alternative<MulticastDiscovery>(config_.discovery))
    {
      auto& multicastDiscoveryConfig = std::get<MulticastDiscovery>(config_.discovery);

      if (multicastDiscoveryConfig.beamPeriod.get() == 0)
      {
        multicastDiscoveryConfig.beamPeriod = defaultBeamingPeriod;
      }

      if (multicastDiscoveryConfig.mcastGroup.empty())
      {
        multicastDiscoveryConfig.mcastGroup = defaultDiscoveryGroup;
      }

      if (multicastDiscoveryConfig.port == 0)
      {
        multicastDiscoveryConfig.port = defaultDiscoveryPort;
      }

      // if no multicast device, assign it from the global config
      if (!multicastDiscoveryConfig.device)
      {
        multicastDiscoveryConfig.device = config_.networkDevice;
      }

      // SEN_ETHER_DISCOVERY_PORT environment variable handling
      if (const auto* envPort = std::getenv("SEN_ETHER_DISCOVERY_PORT"); envPort)
      {
        try
        {
          multicastDiscoveryConfig.port = static_cast<uint16_t>(std::stoul(envPort));
          getLogger()->warn("overriding discovery port with environment variable ({})", multicastDiscoveryConfig.port);
        }
        catch (const std::invalid_argument&)
        {
          getLogger()->warn(
            "Environment variable SEN_ETHER_DISCOVERY_PORT set to non-numeric value. Using default port instead.");
        }
      }
    }
    else
    {
      auto& tcpDiscoveryConfig = std::get<TcpDiscovery>(config_.discovery);
      if (tcpDiscoveryConfig.beamPeriod.getNanoseconds() == 0)
      {
        tcpDiscoveryConfig.beamPeriod = defaultBeamingPeriod;
      }

      if (tcpDiscoveryConfig.hubAddress.host.empty())
      {
        std::string err;
        err.append("invalid (empty) discovery hub host");
        throw std::runtime_error(err);
      }

      if (tcpDiscoveryConfig.hubAddress.port == 0)
      {
        std::string err;
        err.append("invalid (empty) discovery hub port");
        throw std::runtime_error(err);
      }
    }

    // SEN_ETHER_DISABLE_BUS_MULTICAST environment variable handling
    if (const auto* envBusMulticastDisable = std::getenv("SEN_ETHER_DISABLE_BUS_MULTICAST"); envBusMulticastDisable)
    {
      if (const std::string val = envBusMulticastDisable; isEqualsIgnoreCase(val, "true") ||
                                                          isEqualsIgnoreCase(val, "yes") ||
                                                          atoi(envBusMulticastDisable) != 0)  // NOLINT(cert-err34-c)
      {
        config_.busConfig.multicastDisabled = true;
        getLogger()->warn("disallowing multicast bus communication via environment variable");
      }
      else if (isEqualsIgnoreCase(val, "false") || isEqualsIgnoreCase(val, "no") ||
               atoi(envBusMulticastDisable) == 0)  // NOLINT(cert-err34-c)
      {
        config_.busConfig.multicastDisabled = false;
        getLogger()->warn("allowing multicast bus communication via environment variable");
      }
    }

    // SEN_ETHER_ALLOW_VIRTUAL_INTERFACES environment variable handling
    if (const auto* envVirtualInterfaces = std::getenv("SEN_ETHER_ALLOW_VIRTUAL_INTERFACES"); envVirtualInterfaces)
    {
      if (const std::string val = envVirtualInterfaces; isEqualsIgnoreCase(val, "true") ||
                                                        isEqualsIgnoreCase(val, "yes") ||
                                                        atoi(envVirtualInterfaces) != 0)  // NOLINT(cert-err34-c)
      {
        std::visit(::sen::Overloaded {[](auto& value) { value.allowVirtualInterfaces = true; }}, config_.discovery);
        getLogger()->warn("allowing virtual interfaces via environment variable");
      }
      else if (isEqualsIgnoreCase(val, "false") || isEqualsIgnoreCase(val, "no") ||
               atoi(envVirtualInterfaces) == 0)  // NOLINT(cert-err34-c)
      {
        std::visit(::sen::Overloaded {[](auto& value) { value.allowVirtualInterfaces = false; }}, config_.discovery);
        getLogger()->warn("disallowing virtual interfaces via environment variable");
      }
    }

    return done();
  }

private:
  asio::io_context io_;
  std::shared_ptr<DiscoverySystem> discovery_;
  std::unique_ptr<TcpDiscoveryHub> discoveryHub_;
  Configuration config_ {};
};

}  // namespace sen::components::ether

SEN_COMPONENT(sen::components::ether::EtherComponent)
