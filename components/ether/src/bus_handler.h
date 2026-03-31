// === bus_handler.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_BUS_HANDLER_H
#define SEN_COMPONENTS_ETHER_SRC_BUS_HANDLER_H

#include "util.h"

// component
#include "output_queue.h"
#include "shared_buffer_sequence.h"

// sen
#include "sen/kernel/tracer.h"

// generated code
#include "stl/configuration.stl.h"

namespace sen::components::ether
{

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
                                                        sen::kernel::Tracer& tracer);

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
             sen::kernel::Tracer& tracer);

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
};

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_BUS_HANDLER_H
