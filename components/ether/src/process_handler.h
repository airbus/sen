// === process_handler.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_ETHER_SRC_PROCESS_HANDLER_H
#define SEN_COMPONENTS_ETHER_SRC_PROCESS_HANDLER_H

// component
#include "output_queue.h"
#include "shared_buffer_sequence.h"
#include "util.h"

// sen
#include "sen/core/base/span.h"
#include "sen/kernel/tracer.h"
#include "sen/kernel/transport.h"

// asio
#include <asio/io_context.hpp>

// std
#include <atomic>
#include <cstdint>
#include <utility>

namespace sen::components::ether
{

class ProcessHandler final: public std::enable_shared_from_this<ProcessHandler>
{
public:
  SEN_NOCOPY_NOMOVE(ProcessHandler)

public:
  /// Takes an already-connected socket
  [[nodiscard]] static std::shared_ptr<ProcessHandler> make(EtherTransport* transport,
                                                            kernel::TransportListener* transportListener,
                                                            asio::ip::tcp::socket socket,
                                                            asio::io_context& io,
                                                            sen::kernel::Tracer& tracer)
  {
    return std::shared_ptr<ProcessHandler>(
      new ProcessHandler(transport, transportListener, std::move(socket), io, tracer));
  }

  [[nodiscard]] static std::shared_ptr<ProcessHandler> make(EtherTransport* transport,
                                                            kernel::TransportListener* transportListener,
                                                            asio::io_context& io,
                                                            sen::kernel::Tracer& tracer)
  {
    return std::shared_ptr<ProcessHandler>(new ProcessHandler(transport, transportListener, io, tracer));
  }

  ~ProcessHandler();

public:
  void stop() noexcept;
  void startHandlingIncomingConnection();
  void startConnection(std::vector<asio::ip::basic_endpoint<asio::ip::tcp>> endpoints);
  [[nodiscard]] const kernel::ProcessInfo& getInfo() const noexcept { return info_; }
  [[nodiscard]] const kernel::ProcessId& getId() const noexcept { return procId_; }

public:
  void lostBeam();
  void localParticipantJoinedBus(ObjectOwnerId participantId, kernel::BusId busId, std::string_view busName);
  void localParticipantLeftBus(ObjectOwnerId participantId, kernel::BusId busId, std::string_view busName);
  void sendBusMessageUnicast(const kernel::ParticipantAddr& to, kernel::BusId busId, MemBlockPtr data);
  void sendBusMessageUnicast(const kernel::ParticipantAddr& to,
                             kernel::BusId busId,
                             MemBlockPtr data1,
                             MemBlockPtr data2);
  void sendBusMessageConfirmed(const kernel::ParticipantAddr& to, kernel::BusId busId, MemBlockPtr data);
  void sendBusMessageConfirmed(const kernel::ParticipantAddr& to,
                               kernel::BusId busId,
                               MemBlockPtr data1,
                               MemBlockPtr data2);

public:
  void operator()(Hello&& msg);
  void operator()(Ready&& msg);
  void operator()(BusJoined&& msg);
  void operator()(BusLeft&& msg);

private:
  /// Takes an already-connected socket
  ProcessHandler(EtherTransport* transport,
                 kernel::TransportListener* transportListener,
                 asio::ip::tcp::socket socket,
                 asio::io_context& io,
                 sen::kernel::Tracer& tracer);

  /// Connects to an endpoint.
  ProcessHandler(EtherTransport* transport,
                 kernel::TransportListener* transportListener,
                 asio::io_context& io,
                 sen::kernel::Tracer& tracer);

private:
  void onConnected();
  void onDisconnected(const asio::error_code& err);

private:
  void readTcpHeader();
  void readTcpPayload(uint8_t category, uint32_t payloadSize);

private:
  void readUdpMessage();
  void busMessageReceived(Span<const uint8_t> receivedData,
                          kernel::TransportListener::ByteBufferHandle buffer,
                          bool ensureNotDropped);

private:
  template <typename T>
  inline void tcpSendControlMessage(T&& message);

private:
  void shutdownSockets();
  MemBlockPtr makeBestEffortBusMessageHeader(const kernel::ParticipantAddr& to, kernel::BusId busId) const;
  MemBlockPtr makeConfirmedBusMessageHeader(const kernel::ParticipantAddr& to,
                                            kernel::BusId busId,
                                            uint32_t payloadSize) const;
  MemBlockPtr makeControlMessageHeader(uint32_t payloadSize) const;

private:
  void maybeSendUdp();
  void maybeSendTcp();

private:
  static constexpr uint32_t headerSize = 5U;          // category (uint8_t) + size (uint32_t)
  static constexpr uint32_t busHeaderSize = 4U + 4U;  // to (uint32_t) + bus (uint32_t)
  static constexpr uint32_t tcpHeaderSize = headerSize + busHeaderSize;
  static constexpr uint32_t tcpBulkBufferSize = 10U;
  static constexpr uint32_t udpBulkBufferSize = 10U;

  // these pools have some extra space in case serialization takes a bit more
  using BusHeaderPool = FixedMemoryBlockPool<FixedMemoryBlockPoolBase::minBlockSize<busHeaderSize>() + 4U>;
  using ControlHeaderPool = FixedMemoryBlockPool<FixedMemoryBlockPoolBase::minBlockSize<headerSize>() + 4U>;
  using ConfirmedHeaderPool = FixedMemoryBlockPool<FixedMemoryBlockPoolBase::minBlockSize<tcpHeaderSize>() + 4U>;
  using OutMessage = SharedBufferSequence;
  using TcpBufferItr = std::array<OutMessage, tcpBulkBufferSize>::iterator;

private:
  void sendTcp(TcpBufferItr begin, TcpBufferItr end);

private:
  EtherTransport* transport_;
  kernel::TransportListener* transportListener_;
  kernel::TransportListener::ByteBufferManager* transportListenerBufferManager_;
  kernel::ProcessInfo info_;
  kernel::ProcessId procId_;
  asio::ip::tcp::socket tcpSocket_;
  std::array<uint8_t, headerSize> tcpHeaderBuffer_;
  asio::ip::udp::socket udpSocket_;
  asio::ip::udp::endpoint udpEndpoint_;
  asio::io_context& io_;
  std::shared_ptr<BusHeaderPool> busHeaderPool_;
  std::shared_ptr<ControlHeaderPool> controlHeaderPool_;
  std::shared_ptr<ConfirmedHeaderPool> confirmedHeaderPool_;
  std::shared_ptr<spdlog::logger> logger_;
  OutputQueue<SharedBufferSequence> tcpOutQueue_;
  OutputQueue<SharedBufferSequence> udpOutQueue_;
  std::array<OutMessage, tcpBulkBufferSize> tcpBulkBuffer_ {};
  std::array<OutMessage, udpBulkBufferSize> udpBulkBuffer_ {};
  std::atomic_bool sendingTcp_ {false};
  sen::kernel::Tracer& tracer_;
};

}  // namespace sen::components::ether

#endif  // SEN_COMPONENTS_ETHER_SRC_PROCESS_HANDLER_H
