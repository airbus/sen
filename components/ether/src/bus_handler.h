// === bus_handler.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_BUS_HANDLER_H
#define SEN_COMPONENTS_ETHER_SRC_BUS_HANDLER_H

#include "network_exclusion.h"
#include "stats.h"
#include "util.h"

// component
#include "output_queue.h"
#include "shared_buffer_sequence.h"

// sen
#include "sen/core/base/result.h"
#include "sen/kernel/tracer.h"

// generated code
#include "stl/configuration.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// asio
#include <asio/ip/address.hpp>
#include <asio/ip/address_v4.hpp>

// std
#include <cstdint>
#include <string>
#include <vector>

namespace sen::components::ether
{

/// Computes the multicast group for a bus.
///
/// Throws if the configured range has no usable address
/// @param sessionId: session identifier
/// @param busId: bus identifier
/// @param discoveryPort: discovery port used in the address hash
/// @param range: allowed multicast address range
/// @param exclusions: addresses excluded from allocation
/// @return multicast group assigned to the bus
[[nodiscard]] asio::ip::address computeMulticastAddress(uint32_t sessionId,
                                                        uint32_t busId,
                                                        uint16_t discoveryPort,
                                                        const MulticastRange& range,
                                                        const MulticastExclusions& exclusions);

/// Multicast allocation for a configured bus.
struct ConfiguredBusMulticastAllocation
{
  kernel::BusAddress busAddress;
  uint32_t sessionId = 0;
  uint32_t busId = 0;
  asio::ip::address_v4 groupAddress;
};

/// Two different configured buses allocated to the same multicast group.
struct MulticastSelfCollision
{
  ConfiguredBusMulticastAllocation firstAllocation;
  ConfiguredBusMulticastAllocation secondAllocation;
};

/// Multicast allocation and collision information for the configured buses.
struct MulticastCollisionAnalysis
{
  std::vector<ConfiguredBusMulticastAllocation> allocations;
  std::vector<MulticastSelfCollision> selfCollisions;
  uint64_t usableAddressCount = 0;
  double collisionProbability = 0.0;
};

/// Analyzes multicast allocations for configured buses.
///
/// @param configuredBusAddresses: bus addresses to analyze
/// @param discoveryPort: discovery port used to allocate addresses
/// @param range: allowed multicast address range
/// @param exclusions: addresses excluded from allocation
/// @return allocation analysis or an error if no address is usable
[[nodiscard]] Result<MulticastCollisionAnalysis, std::string> analyzeConfiguredMulticastBuses(
  const std::vector<kernel::BusAddress>& configuredBusAddresses,
  uint16_t discoveryPort,
  const MulticastRange& range,
  const MulticastExclusions& exclusions);

/// Formats a multicast self-collision error.
///
/// @param collision: collision to format
/// @return actionable error message
[[nodiscard]] std::string formatMulticastSelfCollision(const MulticastSelfCollision& collision);

class BusHandler final: public std::enable_shared_from_this<BusHandler>
{
  SEN_NOCOPY_NOMOVE(BusHandler)

public:
  [[nodiscard]] static std::shared_ptr<BusHandler> make(uint32_t sessionId,
                                                        kernel::BusId busId,
                                                        const std::string& name,
                                                        kernel::ProcessId procId,
                                                        kernel::TransportListener* listener,
                                                        uint16_t discoveryPort,
                                                        asio::io_context& io,
                                                        const Configuration& config,
                                                        sen::kernel::Tracer& tracer,
                                                        TransportCounters& counters,
                                                        const MulticastExclusions& exclusions);

  ~BusHandler();

public:
  void stop() noexcept;
  void startReading();
  void broadcast(MemBlockPtr&& data);
  void broadcast(MemBlockPtr&& data1, MemBlockPtr&& data2);
  void saveLocalParticipantId(ObjectOwnerId id);
  void removeLocalParticipantId(ObjectOwnerId id);
  [[nodiscard]] bool hasLocalParticipants() const noexcept;
  [[nodiscard]] const std::vector<ObjectOwnerId>& getLocalParticipants() const noexcept;
  [[nodiscard]] const std::string& getName() const noexcept;

private:
  BusHandler(uint32_t sessionId,
             kernel::BusId busId,
             std::string name,
             kernel::ProcessId procId,
             kernel::TransportListener* listener,
             uint16_t discoveryPort,
             asio::io_context& io,
             const Configuration& config,
             sen::kernel::Tracer& tracer,
             TransportCounters& counters,
             const MulticastExclusions& exclusions);

private:
  void readMessage();
  [[nodiscard]] MemBlockPtr makeHeaderBuffer(kernel::ProcessId procId, uint32_t payloadSize) const;
  void maybeSend();

private:
  static constexpr size_t headerSize = 4U + 4U;  // processId + payloadSize
  static constexpr size_t bulkBufferSize = 50;
  using HeaderPool = FixedMemoryBlockPool<headerSize + 4U>;
  using OutMessage = SharedBufferSequence;

private:
  asio::ip::udp::endpoint endpoint_;
  kernel::BusId busId_;
  kernel::ProcessId procId_;
  std::string name_;
  kernel::TransportListener* listener_;
  kernel::TransportListener::ByteBufferManager* transportListenerBufferManager_;
  asio::ip::udp::socket socket_;
  std::vector<uint8_t> readBuffer_;
  std::vector<ObjectOwnerId> localParticipants_;
  std::shared_ptr<HeaderPool> headerPool_;
  std::shared_ptr<spdlog::logger> logger_;
  OutputQueue<OutMessage> outQueue_;
  asio::io_context& io_;
  std::array<OutMessage, bulkBufferSize> bulkBuffer_ {};
  TransportCounters& counters_;
};

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_BUS_HANDLER_H
