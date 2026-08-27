// === network_footprint.h =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_NETWORK_FOOTPRINT_H
#define SEN_COMPONENTS_ETHER_SRC_NETWORK_FOOTPRINT_H

// component
#include "network_exclusion.h"

// sen
#include "sen/core/base/span.h"

// generated code
#include "stl/configuration.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/network_footprint.stl.h"

namespace sen::components::ether
{

/// Builds the network footprint
///
/// @param configuredBusAddresses: bus addresses configured
/// @param suppliedBusAddresses: additional bus addresses supplied
/// @param config: ether configuration used to calculate the footprint
/// @param exclusions: multicast and port exclusions
/// @return the network footprint
[[nodiscard]] kernel::NetworkFootprint makeNetworkFootprint(Span<const kernel::BusAddress> configuredBusAddresses,
                                                            Span<const kernel::BusAddress> suppliedBusAddresses,
                                                            const Configuration& config,
                                                            const NetworkExclusions& exclusions);

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_NETWORK_FOOTPRINT_H
