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

void bindConfiguredPort(const Configuration& config,
                        const PortExclusionSources& exclusions,
                        PortKind kind,
                        const BindPort& bind);

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_PORT_BINDING_H
