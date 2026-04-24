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

/// Per-transport-instance traffic counters. Updated atomically by handlers,
/// read by EtherTransport::fetchStats().
struct TransportCounters
{
  std::atomic<std::size_t> udpSentBytes {0};
  std::atomic<std::size_t> udpReceivedBytes {0};
  std::atomic<std::size_t> tcpSentBytes {0};
  std::atomic<std::size_t> tcpReceivedBytes {0};
};

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_STATS_H
