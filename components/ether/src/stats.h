// === stats.h =========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_STATS_H
#define SEN_COMPONENTS_ETHER_SRC_STATS_H

// std
#include <atomic>
#include <cstddef>

namespace sen::components::ether
{

extern std::atomic<std::size_t> udpSentBytes;
extern std::atomic<std::size_t> udpReceivedBytes;
extern std::atomic<std::size_t> tcpSentBytes;
extern std::atomic<std::size_t> tcpReceivedBytes;

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_STATS_H
