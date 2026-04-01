// === bus_handler.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "bus_handler.h"

// component
#include "shared_buffer_sequence.h"
#include "stats.h"
#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/memory_block.h"
#include "sen/core/base/span.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/obj/object_provider.h"
#include "sen/kernel/tracer.h"
#include "sen/kernel/transport.h"

// generated code
#include "stl/configuration.stl.h"

// asio
#include <asio/buffer.hpp>
#include <asio/error.hpp>
#include <asio/error_code.hpp>
#include <asio/io_context.hpp>
#include <asio/ip/address.hpp>
#include <asio/ip/address_v4.hpp>
#include <asio/ip/udp.hpp>
#include <asio/post.hpp>
#include <asio/socket_base.hpp>
#include <asio/system_error.hpp>

// std
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace sen::components::ether
{

namespace
{

//--------------------------------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------------------------------

constexpr uint32_t busHashSeed = 15071983;

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

uint8_t clamp(uint8_t val, const ByteRange& range) { return std::clamp(val, range.min, range.max); }

uint8_t computeByte(const ByteRange& byteRange, uint8_t hash)
{
  uint8_t minMulticastRange = byteRange.min;  // valid multicast addresses start here
  uint8_t maxMulticastRange = byteRange.max;  // valid multicast addresses end here
  uint8_t multicastRangeLen = maxMulticastRange - minMulticastRange;
  return (multicastRangeLen != 0) ? minMulticastRange + (hash % multicastRangeLen) : minMulticastRange;
}

asio::ip::address computeMulticastAddress(uint32_t sessionId,
                                          uint32_t busId,
                                          uint16_t discoveryPort,
                                          const MulticastRange& ranges)
{
  const auto hash = hashCombine(busHashSeed, sessionId, busId, discoveryPort);
  const auto hashBytes = reinterpret_cast<const uint8_t*>(&hash);  // NOLINT

  const uint8_t byte0 = computeByte(ranges[0], hashBytes[0]);  // NOLINT
  const uint8_t byte1 = computeByte(ranges[1], hashBytes[1]);  // NOLINT
  const uint8_t byte2 = computeByte(ranges[2], hashBytes[2]);  // NOLINT
  const uint8_t byte3 = computeByte(ranges[3], hashBytes[3]);  // NOLINT

  // NOLINTNEXTLINE
  asio::ip::address_v4::bytes_type bytes = {byte0, byte1, byte2, byte3};

  for (std::size_t i = 0; i < bytes.size(); ++i)
  {
    bytes[i] = clamp(bytes[i], ranges[i]);  // NOLINT
  }

  const auto address = asio::ip::address_v4(bytes);
  SEN_ENSURE(address.is_multicast());
  return address;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// BusHandler
//--------------------------------------------------------------------------------------------------------------

std::shared_ptr<BusHandler> BusHandler::make(uint32_t sessionId,
                                             kernel::BusId busId,
                                             const std::string& name,
                                             kernel::ProcessId procId,
                                             kernel::TransportListener* listener,
                                             uint16_t discoveryPort,
                                             asio::io_context& io,
                                             const Configuration& config,
                                             sen::kernel::Tracer& tracer)
{
  return std::shared_ptr<BusHandler>(
    new BusHandler(sessionId, busId, name, procId, listener, discoveryPort, io, config, tracer));
}

BusHandler::BusHandler(uint32_t sessionId,
                       kernel::BusId busId,
                       std::string name,
                       kernel::ProcessId procId,
                       kernel::TransportListener* listener,
                       uint16_t discoveryPort,
                       asio::io_context& io,
                       const Configuration& config,
                       sen::kernel::Tracer& tracer)
  : busId_(busId)
  , procId_(procId)
  , name_(std::move(name))
  , listener_(listener)
  , transportListenerBufferManager_(&listener->getBufferManager())
  , socket_(io)
  , headerPool_(HeaderPool::make())
  , logger_(getLogger())
  , outQueue_(config.busOutQueue)
  , io_(io)
{
  const auto group = computeMulticastAddress(sessionId, busId.get(), discoveryPort, config.busConfig.multicastRange);

  // configure the tracing
  outQueue_.configureTracing(name + "bus.udp.out", &tracer);

  endpoint_ = asio::ip::udp::endpoint(group, config.busConfig.multicastPort);
  configureMulticastSocket(socket_, endpoint_, config.networkDevice, config, logger_.get());

  // start reading
  readBuffer_.resize(kernel::maxBestEffortMessageSize);
}

BusHandler::~BusHandler() = default;

void BusHandler::stop() noexcept
{
  asio::error_code ec;
  std::ignore = socket_.cancel(ec);
  std::ignore = socket_.shutdown(asio::socket_base::shutdown_both, ec);
  std::ignore = socket_.close(ec);
}

void BusHandler::broadcast(MemBlockPtr&& data)
{
  const auto span = data->getConstSpan();

  if (span.size() > kernel::maxBestEffortMessageSize)
  {
    throwRuntimeError("BusHandler: cannot fit data into a best-effort bus message");
  }

  auto hdr = makeHeaderBuffer(procId_, static_cast<uint32_t>(span.size()));

  outQueue_.push(SharedBufferSequence(std::move(hdr), std::move(data)));
  asio::post(io_, [this]() { maybeSend(); });
}

void BusHandler::broadcast(MemBlockPtr&& data1, MemBlockPtr&& data2)
{
  auto span1 = data1->getConstSpan();
  auto span2 = data2->getConstSpan();

  if (span1.size() + span2.size() > kernel::maxBestEffortMessageSize)
  {
    throwRuntimeError("BusHandler: cannot fit data into a best-effort bus message");
  }

  auto hdr = makeHeaderBuffer(procId_, static_cast<uint32_t>(span1.size() + span2.size()));

  outQueue_.push(SharedBufferSequence(std::move(hdr), std::move(data1), std::move(data2)));
  asio::post(io_, [this]() { maybeSend(); });
}

void BusHandler::maybeSend()
{
  if (!socket_.is_open())
  {
    return;
  }

  auto count = outQueue_.dequeueBulk(bulkBuffer_.begin(), bulkBuffer_.size());
  if (count != 0U)
  {
    for (std::size_t i = 0; i < count; ++i)
    {
      try
      {
        socket_.send_to(bulkBuffer_[i], endpoint_);  // NOLINT(cppcoreguidelines-pro-bounds-constant-array-index)
      }
      catch (const asio::system_error& err)
      {
        logger_->trace("bus {} udp socket: {}", getName(), err.what());
      }
    }
    asio::post(io_, [this]() { maybeSend(); });
  }
}

void BusHandler::readMessage()
{
  auto nextBuffer = transportListenerBufferManager_->getBuffer(kernel::maxBestEffortMessageSize);
  auto rawBufferData = nextBuffer->data();
  auto rawBufferSize = nextBuffer->size();

  auto cb = [us = shared_from_this(), buffer = std::move(nextBuffer)](const auto& err, auto bytesReceived) mutable
  {
    if (err)
    {
      // log only if we get a strange error
      if (err != asio::error::operation_aborted && err != asio::error::shut_down)
      {
        us->logger_->error("transport error on UDP read from bus: {}", err.message());
      }
      return;
    }

    uint32_t processId = 0;
    uint32_t payloadSize = 0;

    InputStream in(*buffer);
    in.readUInt32(processId);
    in.readUInt32(payloadSize);

    if (const auto remainingBytes = bytesReceived - in.getPosition(); SEN_UNLIKELY(payloadSize != remainingBytes))
    {
      getLogger()->error("expected {} bytes, but received {}", payloadSize, remainingBytes);
    }
    else
    {
      // do not process messages sent by us
      if (processId != us->procId_.get())
      {
        udpReceivedBytes.fetch_add(payloadSize);

        auto span = makeConstSpan(in.advance(payloadSize), payloadSize);
        us->listener_->remoteBroadcastMessageReceived(us->busId_, span, std::move(buffer));
      }
    }

    us->readMessage();
  };

  socket_.async_receive(asio::buffer(rawBufferData, rawBufferSize), 0, std::move(cb));
}

void BusHandler::startReading() { readMessage(); }

void BusHandler::saveLocalParticipantId(ObjectOwnerId id) { localParticipants_.push_back(id); }

void BusHandler::removeLocalParticipantId(ObjectOwnerId id)
{
  localParticipants_.erase(std::find(localParticipants_.begin(), localParticipants_.end(), id));
}

bool BusHandler::hasLocalParticipants() const noexcept { return !localParticipants_.empty(); }

const std::vector<ObjectOwnerId>& BusHandler::getLocalParticipants() const noexcept { return localParticipants_; }

const std::string& BusHandler::getName() const noexcept { return name_; }

MemBlockPtr BusHandler::makeHeaderBuffer(kernel::ProcessId procId, uint32_t payloadSize) const
{
  auto hdr = headerPool_->getBlockPtr();

  // using resizable buffer writer as pooled block resizing has nearly zero cost
  ResizableBufferWriter bufferWriter(*hdr);
  OutputStream out(bufferWriter);

  out.writeUInt32(procId.get());
  out.writeUInt32(payloadSize);
  return hdr;
}

}  // namespace sen::components::ether
