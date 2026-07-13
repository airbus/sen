// === port_binding.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_PORT_BINDING_H
#define SEN_COMPONENTS_ETHER_SRC_PORT_BINDING_H

// component
#include "network_exclusion.h"

// sen
#include "sen/core/base/move_only_function.h"

// generated code
#include "stl/configuration.stl.h"

// asio
#include <asio/error_code.hpp>

// std
#include <cstdint>

namespace sen::components::ether
{

enum class PortKind
{
  tcpAcceptor,
  udpUnicast,
  tcpSource,
};

using BindPort = sen::std_util::move_only_function<asio::error_code(uint16_t) const>;

/// Binds a port using the configured mode for the given port kind.
///
/// Throws if the configuration is invalid, the selected port is excluded or no port can be bound.
///
/// @param config: ether configuration with the port settings.
/// @param exclusions: port exclusions that must not be used.
/// @param kind: port type to bind.
/// @param bind: function called with the port to bind, returning the bind result.
void bindConfiguredPort(const Configuration& config,
                        const PortExclusionSources& exclusions,
                        PortKind kind,
                        const BindPort& bind);

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_PORT_BINDING_H
