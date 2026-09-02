// === port_binding.cpp ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "port_binding.h"

// component
#include "network_exclusion.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"

// generated code
#include "stl/configuration.stl.h"

// asio
#include <asio/error.hpp>
#include <asio/error_code.hpp>

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace sen::components::ether
{

namespace
{

[[nodiscard]] const char* toString(PortKind kind) noexcept
{
  switch (kind)
  {
    case PortKind::tcpAcceptor:
      return "TCP acceptor";
    case PortKind::udpUnicast:
      return "UDP unicast";
    case PortKind::tcpSource:
      return "TCP source";
  }
  return "unknown";
}

[[nodiscard]] const PortBinding& getPortBinding(const Configuration& config, PortKind kind)
{
  static const PortBinding ephemeral = Ephemeral {};

  if (!config.portConfig)
  {
    return ephemeral;
  }

  switch (kind)
  {
    case PortKind::tcpAcceptor:
      return config.portConfig->tcpAcceptor;
    case PortKind::udpUnicast:
      return config.portConfig->udpUnicast;
    case PortKind::tcpSource:
      return config.portConfig->tcpSource;
  }
  return ephemeral;
}

[[nodiscard]] std::string excludedByToString(uint16_t port, const PortExclusionSources& exclusions)
{
  std::ostringstream stream;
  bool first = true;
  const auto appendSource = [&](const char* source)
  {
    if (!first)
    {
      stream << ", ";
    }
    stream << source;
    first = false;
  };

  if (exclusions.builtIn.isExcluded(port))
  {
    appendSource("built-in exclusions");
  }
  if (exclusions.configured.isExcluded(port))
  {
    appendSource("configured exclusions");
  }
  if (exclusions.os.isExcluded(port))
  {
    appendSource("OS exclusions");
  }
  return stream.str();
}

void validateProbeRange(PortKind kind, const ProbePortRange& config)
{
  if (config.min > config.max)
  {
    std::ostringstream oss;
    oss << "Invalid probe port range for " << toString(kind) << " (" << config.min << '-' << config.max
        << "): min is greater than max.";
    sen::throwRuntimeError(oss.str());
  }
}

[[nodiscard]] std::size_t pickProbeStartIndex(std::size_t candidateCount)
{
  SEN_ASSERT(candidateCount > 0U);

  std::random_device randomDevice;
  std::uniform_int_distribution<std::size_t> distribution(0U, candidateCount - 1U);
  return distribution(randomDevice);
}

[[nodiscard]] std::string rangeToString(const ProbePortRange& config)
{
  return std::to_string(config.min) + "-" + std::to_string(config.max);
}

void bindEphemeral(PortKind kind, const BindPort& bind)
{
  const auto error = bind(0);
  if (error)
  {
    sen::throwRuntimeError(std::string("Failed to bind ephemeral port for ") + toString(kind) + ": " + error.message());
  }
}

void bindPinned(PortKind kind, const PinnedPort& config, const PortExclusionSources& exclusions, const BindPort& bind)
{
  const auto excludedBy = excludedByToString(config.port, exclusions);
  if (!excludedBy.empty())
  {
    sen::throwRuntimeError(std::string("Cannot bind pinned port for ") + toString(kind) + " (" +
                           std::to_string(config.port) + "): excluded by " + excludedBy);
  }

  const auto error = bind(config.port);
  if (error)
  {
    sen::throwRuntimeError(std::string("Failed to bind pinned port for ") + toString(kind) + " (" +
                           std::to_string(config.port) + "): " + error.message());
  }
}

/// Returns every port between config.min and config.max that is not excluded
[[nodiscard]] std::vector<uint16_t> getUsablePorts(const ProbePortRange& config, const PortExclusionSources& exclusions)
{
  std::vector<uint16_t> usablePorts;
  usablePorts.reserve(static_cast<std::size_t>(config.max) - static_cast<std::size_t>(config.min) + 1U);

  for (uint32_t port = config.min; port <= config.max; ++port)
  {
    const auto candidate = static_cast<uint16_t>(port);
    if (!isPortExcluded(candidate, exclusions))
    {
      usablePorts.push_back(candidate);
    }
  }

  return usablePorts;
}

/// Builds the diagnostic message fragment with the excluded port count and the exclusion source range
[[nodiscard]] std::string getProbeExclusionDetails(const ProbePortRange& config,
                                                   const PortExclusionSources& exclusions,
                                                   std::size_t excludedPortCount)
{
  const auto portRangeToString = [](uint16_t min, uint16_t max)
  {
    if (min == max)
    {
      return std::to_string(min);
    }
    return std::to_string(min) + "-" + std::to_string(max);
  };

  std::ostringstream exclusionSources;
  bool hasExclusionSource = false;
  const auto appendExclusionSource = [&](const char* source, const auto& sourceExclusions)
  {
    std::ostringstream sourceRanges;
    bool hasSourceRange = false;
    for (const auto& range: sourceExclusions.ranges())
    {
      const auto min = std::max(config.min, range.min);
      const auto max = std::min(config.max, range.max);
      if (min > max)
      {
        continue;
      }

      if (hasSourceRange)
      {
        sourceRanges << ", ";
      }
      sourceRanges << portRangeToString(min, max);
      hasSourceRange = true;
    }

    if (!hasSourceRange)
    {
      return;
    }

    if (hasExclusionSource)
    {
      exclusionSources << "; ";
    }
    exclusionSources << source << ": " << sourceRanges.str();
    hasExclusionSource = true;
  };

  appendExclusionSource("built-in", exclusions.builtIn);
  appendExclusionSource("configured", exclusions.configured);
  appendExclusionSource("OS", exclusions.os);

  std::string exclusionDetails = "excluded ports: " + std::to_string(excludedPortCount);
  if (hasExclusionSource)
  {
    exclusionDetails += " (" + exclusionSources.str() + ")";
  }

  return exclusionDetails;
}

/// Attempts to bind for each usable port, starting at a random index and wrapping around the vector.
/// If a port is already in use, the next usable port is tried.
void bindUsableProbePorts(PortKind kind,
                          const ProbePortRange& config,
                          const std::vector<uint16_t>& usablePorts,
                          const std::string& exclusionDetails,
                          const BindPort& bind)
{
  SEN_ASSERT(!usablePorts.empty());
  const auto startIndex = pickProbeStartIndex(usablePorts.size());
  asio::error_code lastError;
  uint16_t lastAttemptedPort = 0;

  for (std::size_t attempt = 0; attempt < usablePorts.size(); ++attempt)
  {
    const auto index = (startIndex + attempt) % usablePorts.size();
    const auto port = usablePorts.at(index);
    lastAttemptedPort = port;
    const auto error = bind(port);
    if (!error)
    {
      return;
    }

    lastError = error;
    if (error != asio::error::address_in_use)
    {
      sen::throwRuntimeError(std::string("Failed to bind probe port for ") + toString(kind) + " (" +
                             std::to_string(port) + " in range " + rangeToString(config) + "): " + error.message() +
                             "; bind attempts: " + std::to_string(attempt + 1U) + "; " + exclusionDetails);
    }
  }

  std::string reason = "no port was available";
  SEN_ASSERT(static_cast<bool>(lastError));
  reason += "; bind attempts: " + std::to_string(usablePorts.size());
  reason += "; " + exclusionDetails;
  reason += "; last attempted port: " + std::to_string(lastAttemptedPort);
  reason += "; last error: " + lastError.message();

  sen::throwRuntimeError(std::string("Failed to bind probe range for ") + toString(kind) + " (" +
                         rangeToString(config) + "): " + reason);
}

/// Validates the configured range, filters excluded ports and binds one usable port.
void bindProbe(PortKind kind,
               const ProbePortRange& config,
               const PortExclusionSources& exclusions,
               const BindPort& bind)
{
  validateProbeRange(kind, config);

  const auto usablePorts = getUsablePorts(config, exclusions);
  const auto portCount = static_cast<std::size_t>(config.max) - static_cast<std::size_t>(config.min) + 1U;
  const auto excludedPortCount = portCount - usablePorts.size();
  const auto exclusionDetails = getProbeExclusionDetails(config, exclusions, excludedPortCount);

  if (usablePorts.empty())
  {
    sen::throwRuntimeError(std::string("Failed to bind probe range for ") + toString(kind) + " (" +
                           rangeToString(config) + "): all ports are excluded; " + exclusionDetails +
                           "; bind attempts: 0.");
  }

  bindUsableProbePorts(kind, config, usablePorts, exclusionDetails, bind);
}

}  // namespace

void bindConfiguredPort(const Configuration& config,
                        const PortExclusionSources& exclusions,
                        PortKind kind,
                        const BindPort& bind)
{
  std::visit(
    ::sen::Overloaded {
      [&](const Ephemeral&) { bindEphemeral(kind, bind); },
      [&](const PinnedPort& pinnedPort) { bindPinned(kind, pinnedPort, exclusions, bind); },
      [&](const ProbePortRange& probeRange) { bindProbe(kind, probeRange, exclusions, bind); },
    },
    getPortBinding(config, kind));
}

}  // namespace sen::components::ether
