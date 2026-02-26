// === process_handler.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "process_handler.h"

// component
#include "ether_transport.h"
#include "shared_buffer_sequence.h"
#include "stats.h"
#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/memory_block.h"
#include "sen/core/base/span.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/obj/object_provider.h"
#include "sen/kernel/tracer.h"
#include "sen/kernel/transport.h"

// generated code
#include "stl/runtime.stl.h"

// asio
#include <asio/buffer.hpp>
#include <asio/error.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/basic_endpoint.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <asio/post.hpp>
#include <asio/read.hpp>  // NOLINT(unused-includes): used by async_read
#include <asio/socket_base.hpp>
#include <asio/system_error.hpp>
#include <asio/write.hpp>

// std
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iterator>
#include <memory>
#include <string_view>
#include <system_error>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace sen::components::ether
{
namespace
{
//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

enum class MessageCategory : uint8_t
{
  busMessage = 0U,
  controlMessage = 1U,
};
}  // namespace

//--------------------------------------------------------------------------------------------------------------
// ProcessHandler
//--------------------------------------------------------------------------------------------------------------

ProcessHandler::ProcessHandler(EtherTransport* transport,
                               kernel::TransportListener* transportListener,
                               asio::ip::tcp::socket socket,
                               asio::io_context& io,
                               sen::kernel::Tracer& tracer)
  : transport_(transport)
  , transportListener_(transportListener)
  , transportListenerBufferManager_(&transportListener->getBufferManager())
  , tcpSocket_(std::move(socket))
  , udpSocket_(io)
  , io_(io)
  , busHeaderPool_(BusHeaderPool::make())
  , controlHeaderPool_(ControlHeaderPool::make())
  , confirmedHeaderPool_(ConfirmedHeaderPool::make())
  , logger_(getLogger())
  , tcpOutQueue_(transport->getConfig().processTcpOutQueue)
  , udpOutQueue_(transport->getConfig().processUdpOutQueue)
  , tracer_(tracer)
{
  configureTcpSocket(tcpSocket_, transport->getConfig());
}

ProcessHandler::ProcessHandler(EtherTransport* transport,
                               kernel::TransportListener* transportListener,
                               asio::io_context& io,
                               sen::kernel::Tracer& tracer)
  : transport_(transport)
  , transportListener_(transportListener)
  , transportListenerBufferManager_(&transportListener->getBufferManager())
  , tcpSocket_(io)
  , udpSocket_(io)
  , io_(io)
  , busHeaderPool_(BusHeaderPool::make())
  , controlHeaderPool_(ControlHeaderPool::make())
  , confirmedHeaderPool_(ConfirmedHeaderPool::make())
  , logger_(getLogger())
  , tcpOutQueue_(transport->getConfig().processTcpOutQueue)
  , udpOutQueue_(transport->getConfig().processUdpOutQueue)
  , tracer_(tracer)
{
  tcpSocket_.open(asio::ip::tcp::v4());
  configureTcpSocket(tcpSocket_, transport->getConfig());
}

void ProcessHandler::startHandlingIncomingConnection()
{
  onConnected();  // we have just been connected
}

void ProcessHandler::startConnection(std::vector<asio::ip::basic_endpoint<asio::ip::tcp>> endpoints)
{
  auto endpoint = endpoints.back();
  endpoints.pop_back();

  auto callback = [us = shared_from_this(), points = std::move(endpoints), endpoint](const auto& err)
  {
    if (err)
    {
      if (points.empty())
      {
        us->logger_->error("could not connect o any of the available endpoints: {}", err.message());
        us->onDisconnected(err);
      }
      else
      {
        us->tcpSocket_.close();
        us->startConnection(points);
      }
    }
    else
    {
      us->onConnected();
    }
  };

  getLogger()->debug("Will start connection to {} at port {}", endpoint.address().to_string(), endpoint.port());
  tcpSocket_.async_connect(endpoint, std::move(callback));
}

ProcessHandler::~ProcessHandler() { logger_->debug("process handler: deleted"); }

void ProcessHandler::stop() noexcept
{
  try
  {
    logger_->debug("process handler: shutting down sockets");
    shutdownSockets();
    logger_->debug("process handler: shutting down sockets -> done");
  }
  catch (const std::exception& err)
  {
    logger_->warn("process handler: error shutting down sockets: {}", err.what());
  }
  catch (...)
  {
    logger_->warn("process handler: unknown error shutting down sockets");
  }
}

//--------------------------------------------------------------------------------------------------------------
// Interface towards transport
//--------------------------------------------------------------------------------------------------------------

void ProcessHandler::lostBeam() { onDisconnected({}); }

void ProcessHandler::shutdownSockets()
{
  asio::error_code ec;
  std::ignore = udpSocket_.cancel(ec);
  std::ignore = tcpSocket_.cancel(ec);

  std::ignore = udpSocket_.shutdown(asio::socket_base::shutdown_both, ec);
  std::ignore = tcpSocket_.shutdown(asio::socket_base::shutdown_both, ec);

  std::ignore = tcpSocket_.close(ec);
  std::ignore = udpSocket_.close(ec);
}

void ProcessHandler::localParticipantJoinedBus(ObjectOwnerId participantId,
                                               kernel::BusId busId,
                                               std::string_view busName)
{
  BusJoined msg {};
  msg.participantId = participantId.get();
  msg.busId = busId.get();
  msg.busName = busName;

  tcpSendControlMessage(std::move(msg));
}

void ProcessHandler::localParticipantLeftBus(ObjectOwnerId participantId, kernel::BusId busId, std::string_view busName)
{
  BusLeft msg {};
  msg.participantId = participantId.get();
  msg.busId = busId.get();
  msg.busName = busName;

  tcpSendControlMessage(std::move(msg));
}

//--------------------------------------------------------------------------------------------------------------
// ControlMessage reception
//--------------------------------------------------------------------------------------------------------------

void ProcessHandler::operator()(Hello&& msg)
{
  // check if the other was mistakenly connecting to another session
  if (msg.info.sessionId != transport_->getOwnInfo().sessionId)
  {
    tcpSocket_.close();
  }

  // kernel protocol version 9 is not retro-compatible
  if (msg.version.kernel != kernel::getKernelProtocolVersion())
  {
    getLogger()->error(
      "Other Sen instance (host: {}, app: {}, pid: {}) is using version {} of the kernel protocol. We are using "
      "version {}, which is not retro-compatible. Aborting communication.",
      msg.info.hostName,
      msg.info.appName,
      msg.info.processId,
      msg.version.kernel,
      kernel::getKernelProtocolVersion());

    shutdownSockets();
    transport_->processDisconnected(this);  // this will delete us
    return;
  }

  // the ether protocol 2 is not retro-compatible
  if (msg.version.ether < 2)
  {
    getLogger()->warn(
      "Other Sen instance (host: {}, app: {}, pid: {}) is using ether protocol version {}. We are "
      "using version {}, which is not retro-compatible. Aborting communication.",
      msg.info.hostName,
      msg.info.appName,
      msg.info.processId,
      msg.version.ether,
      etherProtocolVersion);

    shutdownSockets();
    transport_->processDisconnected(this);  // this will delete us
    return;
  }

  // handle differences in ether protocol version
  if (msg.version.ether != etherProtocolVersion)
  {
    getLogger()->warn(
      "Other Sen instance (host: {}, app: {}, pid: {}) is using ether protocol version {}. We are "
      "using version {}. Will try to adapt.",
      msg.info.hostName,
      msg.info.appName,
      msg.info.processId,
      msg.version.ether,
      etherProtocolVersion);
  }

  info_ = std::move(msg.info);

  // configure tracing
  tcpOutQueue_.configureTracing(info_.hostName + "." + info_.appName + ".tcp.out", &tracer_);
  udpOutQueue_.configureTracing(info_.hostName + "." + info_.appName + ".udp.out", &tracer_);

  // store the udp endpoint for non-reliable comms
  udpEndpoint_.address(tcpSocket_.remote_endpoint().address());
  udpEndpoint_.port(msg.udpPort);

  logger_->debug("{} → hello → {}", getInfo().appName, transport_->getOwnInfo().appName);

  tcpSendControlMessage(Ready {});
}

void ProcessHandler::operator()(Ready&& msg)
{
  std::ignore = msg;

  // notify the session we are ready, get the ptr
  logger_->debug("{} → ready → {}", getInfo().appName, transport_->getOwnInfo().appName);

  procId_ = transport_->processReady(this);
}

void ProcessHandler::operator()(BusJoined&& msg)
{
  kernel::ParticipantAddr addr;
  addr.proc = procId_;
  addr.id = msg.participantId;

  logger_->debug("{} → participant {} joined bus {} → {}",
                 getInfo().appName,
                 msg.participantId,
                 msg.busName,
                 transport_->getOwnInfo().appName);

  transportListener_->remoteParticipantJoinedBus(addr, info_, kernel::BusId(msg.busId), msg.busName);
}

void ProcessHandler::operator()(BusLeft&& msg)
{
  kernel::ParticipantAddr addr;
  addr.proc = procId_;
  addr.id = msg.participantId;

  logger_->debug("{} → participant {} left bus {} → {}",
                 getInfo().appName,
                 msg.participantId,
                 msg.busName,
                 transport_->getOwnInfo().appName);

  transportListener_->remoteParticipantLeftBus(addr, kernel::BusId(msg.busId), msg.busName);
}

void ProcessHandler::busMessageReceived(Span<const uint8_t> receivedData,
                                        kernel::TransportListener::ByteBufferHandle buffer,
                                        bool ensureNotDropped)
{
  InputStream in(receivedData);

  uint32_t to = 0;
  uint32_t bus = 0;

  in.readUInt32(to);
  in.readUInt32(bus);

  const auto payloadSize = receivedData.size() - busHeaderSize;

  auto span = makeConstSpan(in.advance(payloadSize), payloadSize);
  transportListener_->remoteMessageReceived(to, bus, span, std::move(buffer), ensureNotDropped);
}

//--------------------------------------------------------------------------------------------------------------
// Connection management
//--------------------------------------------------------------------------------------------------------------

void ProcessHandler::onConnected()
{
  logger_->debug("ProcessHandler: onConnected()");

  udpSocket_ = asio::ip::udp::socket(io_, asio::ip::udp::endpoint(asio::ip::address_v4::any(), 0));

  Hello hello {};
  hello.info = transport_->getOwnInfo();
  hello.udpPort = udpSocket_.local_endpoint().port();
  hello.version.kernel = kernel::getKernelProtocolVersion();
  hello.version.ether = etherProtocolVersion;

  tcpSendControlMessage(std::move(hello));
  readTcpHeader();

  // start receiving udp messages
  readUdpMessage();
}

void ProcessHandler::onDisconnected(const asio::error_code& err)
{
  logger_->debug("ProcessHandler: onDisconnected() {}", err.message());

  std::ignore = err;
  shutdownSockets();
  transport_->processDisconnected(this);  // this will delete us
}

//--------------------------------------------------------------------------------------------------------------
// Emission
//--------------------------------------------------------------------------------------------------------------

void ProcessHandler::sendBusMessageUnicast(const kernel::ParticipantAddr& to, kernel::BusId busId, MemBlockPtr data)
{
  if (const auto dataSpan = data->getConstSpan(); dataSpan.size() > kernel::maxBestEffortMessageSize)
  {
    throwRuntimeError("ProcessHandler: cannot fit data into a best-effort bus message");
  }

  udpOutQueue_.push(SharedBufferSequence(makeBestEffortBusMessageHeader(to, busId), std::move(data)));

  asio::post(io_, [us = shared_from_this()]() { us->maybeSendUdp(); });
}

void ProcessHandler::sendBusMessageUnicast(const kernel::ParticipantAddr& to,
                                           kernel::BusId busId,
                                           MemBlockPtr data1,
                                           MemBlockPtr data2)
{
  const auto data1Span = data1->getConstSpan();
  const auto data2Span = data2->getConstSpan();

  if (data1Span.size() + data2Span.size() > kernel::maxBestEffortMessageSize)
  {
    throwRuntimeError("ProcessHandler: cannot fit data into a best-effort bus message");
  }

  udpOutQueue_.push(
    SharedBufferSequence(makeBestEffortBusMessageHeader(to, busId), std::move(data1), std::move(data2)));

  asio::post(io_, [us = shared_from_this()]() { us->maybeSendUdp(); });
}

void ProcessHandler::sendBusMessageConfirmed(const kernel::ParticipantAddr& to, kernel::BusId busId, MemBlockPtr data)
{
  auto span = data->getConstSpan();
  auto hdr = makeConfirmedBusMessageHeader(to, busId, static_cast<uint32_t>(span.size()));

  tcpOutQueue_.push(SharedBufferSequence(std::move(hdr), std::move(data)));

  asio::post(io_, [us = shared_from_this()]() { us->maybeSendTcp(); });
}

void ProcessHandler::sendBusMessageConfirmed(const kernel::ParticipantAddr& to,
                                             kernel::BusId busId,
                                             MemBlockPtr data1,
                                             MemBlockPtr data2)
{
  auto span1 = data1->getConstSpan();
  auto span2 = data2->getConstSpan();
  auto hdr = makeConfirmedBusMessageHeader(to, busId, static_cast<uint32_t>(span1.size() + span2.size()));

  tcpOutQueue_.push(SharedBufferSequence(std::move(hdr), std::move(data1), std::move(data2)));
  asio::post(io_, [us = shared_from_this()]() { us->maybeSendTcp(); });
}

template <typename T>
void ProcessHandler::tcpSendControlMessage(T&& message)
{
  ControlMessage ctrl(std::forward<T>(message));

  const auto ctrlSize = SerializationTraits<ControlMessage>::serializedSize(ctrl);

  auto data = std::make_shared<ResizableHeapBlock>();
  {
    data->resize(ctrlSize);
    BufferWriter writer(*data);
    OutputStream out(writer);
    SerializationTraits<ControlMessage>::write(out, ctrl);
  }

  auto hdr = makeControlMessageHeader(ctrlSize);

  tcpOutQueue_.push(SharedBufferSequence(std::move(hdr), std::move(data)));
  asio::post(io_, [us = shared_from_this()]() { us->maybeSendTcp(); });
}

void ProcessHandler::maybeSendUdp()
{
  auto count = udpOutQueue_.dequeueBulk(udpBulkBuffer_.begin(), udpBulkBuffer_.size());
  if (count != 0U)
  {
    for (std::size_t i = 0; i < count; ++i)
    {
      auto& bufferSeq = udpBulkBuffer_[i];  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
      for (const auto& block: bufferSeq)
      {
        udpSentBytes.fetch_add(block.size());
      }

      try
      {
        udpSocket_.send_to(bufferSeq, udpEndpoint_);
      }
      catch (const asio::system_error& err)
      {
        logger_->trace("error sending udp packet to {} in {}: {}", info_.appName, info_.hostName, err.what());
      }
    }

    asio::post(io_, [us = shared_from_this()]() { us->maybeSendUdp(); });
  }
}

void ProcessHandler::maybeSendTcp()
{
  // wait if there's an ongoing tcp transfer.
  // once finished, the transfer will call this function anyway.
  if (sendingTcp_)
  {
    return;
  }

  if (!tcpSocket_.is_open())
  {
    return;
  }

  auto count = tcpOutQueue_.dequeueBulk(tcpBulkBuffer_.begin(), tcpBulkBuffer_.size());

  if (count == 0U)
  {
    return;
  }

  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  sendTcp(tcpBulkBuffer_.begin(), tcpBulkBuffer_.begin() + count);
}

void ProcessHandler::sendTcp(TcpBufferItr begin, TcpBufferItr end)
{
  sendingTcp_ = true;

  if (begin == end)
  {
    sendingTcp_ = false;
    maybeSendTcp();
    return;
  }

  try
  {
    asio::async_write(tcpSocket_,
                      *begin,
                      [us = shared_from_this(), begin, end](std::error_code ec, std::size_t length) mutable
                      {
                        if (!ec)
                        {
                          tcpSentBytes.fetch_add(length);
                          us->sendTcp(std::next(begin), end);
                        }
                        else
                        {
                          us->onDisconnected(ec);
                        }
                      });
  }
  catch (const asio::system_error& err)
  {
    onDisconnected(err.code());
  }
}

//--------------------------------------------------------------------------------------------------------------
// Async I/O
//--------------------------------------------------------------------------------------------------------------

void ProcessHandler::readTcpHeader()
{
  auto cb = [us = shared_from_this()](const auto& err, auto bytesReceived) mutable
  {
    if (err)
    {
      us->onDisconnected(err);
      return;
    }

    uint8_t category = 0;
    uint32_t size = 0;

    InputStream in(us->tcpHeaderBuffer_);
    in.readUInt8(category);
    in.readUInt32(size);

    std::ignore = bytesReceived;
    tcpReceivedBytes.fetch_add(5);

    us->readTcpPayload(category, size);
  };

  asio::async_read(tcpSocket_, asio::buffer(tcpHeaderBuffer_.data(), tcpHeaderBuffer_.size()), std::move(cb));
}

void ProcessHandler::readTcpPayload(uint8_t category, uint32_t payloadSize)
{
  auto buffer = transportListenerBufferManager_->getBuffer(payloadSize);
  auto rawBufferData = buffer->data();
  auto rawBufferSize = buffer->size();

  auto cb = [us = shared_from_this(), category, buffer = std::move(buffer)](const auto& err, auto bytesReceived) mutable
  {
    std::ignore = bytesReceived;

    if (err)
    {
      us->onDisconnected(err);
      return;
    }

    tcpReceivedBytes.fetch_add(buffer->size());

    if (category == static_cast<uint8_t>(MessageCategory::controlMessage))
    {
      std::visit(*us, readFromBuffer<ControlMessage>(*buffer));
    }
    else
    {
      Span<const uint8_t> span(*buffer);
      us->busMessageReceived(span, std::move(buffer), /*ensureNotDropped=*/true);
    }

    us->readTcpHeader();
  };

  asio::async_read(tcpSocket_, asio::buffer(rawBufferData, rawBufferSize), std::move(cb));
}

void ProcessHandler::readUdpMessage()
{
  auto buffer = transportListenerBufferManager_->getBuffer(kernel::maxBestEffortMessageSize);
  auto rawBufferData = buffer->data();
  auto rawBufferSize = buffer->size();

  auto cb = [us = shared_from_this(), buffer = std::move(buffer)](const auto& err, auto bytesReceived) mutable
  {
    if (err)
    {
      if (err != asio::error::operation_aborted)
      {
        us->logger_->error("transport error on UDP read: {}", err.message());
      }
      return;
    }

    if (bytesReceived != 0)
    {
      udpReceivedBytes.fetch_add(bytesReceived);

      auto span = makeConstSpan(buffer->data(), bytesReceived);
      us->busMessageReceived(span, std::move(buffer), /*ensureNotDropped=*/false);
    }

    us->readUdpMessage();
  };

  udpSocket_.async_receive(asio::buffer(rawBufferData, rawBufferSize), 0, std::move(cb));
}

MemBlockPtr ProcessHandler::makeBestEffortBusMessageHeader(const kernel::ParticipantAddr& to, kernel::BusId busId) const
{
  auto block = busHeaderPool_->getBlockPtr();

  ResizableBufferWriter writer(*block);
  OutputStream out(writer);

  out.writeUInt32(to.id.get());  // to
  out.writeUInt32(busId.get());  // bus

  return block;
}

MemBlockPtr ProcessHandler::makeConfirmedBusMessageHeader(const kernel::ParticipantAddr& to,
                                                          kernel::BusId busId,
                                                          uint32_t payloadSize) const
{
  auto block = confirmedHeaderPool_->getBlockPtr();
  ResizableBufferWriter writer(*block);
  OutputStream out(writer);

  out.writeUInt8(static_cast<uint8_t>(MessageCategory::busMessage));  // category
  out.writeUInt32(busHeaderSize + payloadSize);                       // size
  out.writeUInt32(to.id.get());                                       // to
  out.writeUInt32(busId.get());                                       // bus

  return block;
}

MemBlockPtr ProcessHandler::makeControlMessageHeader(uint32_t payloadSize) const
{
  auto block = controlHeaderPool_->getBlockPtr();
  ResizableBufferWriter writer(*block);
  OutputStream out(writer);

  out.writeUInt8(static_cast<uint8_t>(MessageCategory::controlMessage));  // category
  out.writeUInt32(payloadSize);                                           // size
  return block;
}

}  // namespace sen::components::ether
