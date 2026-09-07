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
#include "sen/core/base/class_helpers.h"

// generated code
#include "stl/configuration.stl.h"

// std
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

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

void bindEphemeral(PortKind kind, const BindPort& bind)
{
  const auto error = bind(0);
  if (error)
  {
    throw std::runtime_error(std::string("Failed to bind ephemeral port for ") + toString(kind) + ": " +
                             error.message());
  }
}

void bindPinned(PortKind kind, const PinnedPort& config, const PortExclusionSources& exclusions, const BindPort& bind)
{
  const auto excludedBy = excludedByToString(config.port, exclusions);
  if (!excludedBy.empty())
  {
    throw std::runtime_error(std::string("Cannot bind pinned port for ") + toString(kind) + " (" +
                             std::to_string(config.port) + "): excluded by " + excludedBy +
                             ". No fallback is allowed.");
  }

  const auto error = bind(config.port);
  if (error)
  {
    throw std::runtime_error(std::string("Failed to bind pinned port for ") + toString(kind) + " (" +
                             std::to_string(config.port) + "): " + error.message() + ". No fallback is allowed.");
  }
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
    },
    getPortBinding(config, kind));
}

}  // namespace sen::components::ether
