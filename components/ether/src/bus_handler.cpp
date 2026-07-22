// === bus_handler.cpp =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "bus_handler.h"

// component
#include "network_exclusion.h"
#include "shared_buffer_sequence.h"
#include "stats.h"
#include "util.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/hash32.h"
#include "sen/core/base/memory_block.h"
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
#include "sen/core/io/buffer_writer.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/obj/object_provider.h"
#include "sen/kernel/tracer.h"
#include "sen/kernel/transport.h"

// generated code
#include "stl/configuration.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

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
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
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

[[nodiscard]] std::string formatBusAddress(const kernel::BusAddress& address)
{
  std::string formattedAddress = address.sessionName;
  formattedAddress.push_back('.');
  formattedAddress.append(address.busName);
  return formattedAddress;
}

/// Estimates collision probability using the birthday problem
[[nodiscard]] double computeBirthdayCollisionProbability(std::size_t busCount, uint64_t usableAddressCount) noexcept
{
  if (busCount < 2U || usableAddressCount == 0U)
  {
    return 0.0;
  }

  const auto n = sen::std_util::checkedConversion<double>(busCount);
  const auto usableSpace = sen::std_util::checkedConversion<double>(usableAddressCount);
  return 1.0 - std::exp(-(n * (n - 1.0)) / (2.0 * usableSpace));
}

}  // namespace

asio::ip::address computeMulticastAddress(uint32_t sessionId,
                                          uint32_t busId,
                                          uint16_t discoveryPort,
                                          const MulticastRange& range,
                                          const MulticastExclusions& exclusions)
{
  const auto hash = hashCombine(busHashSeed, sessionId, busId, discoveryPort);
  const auto usableAddressCount = usableMulticastAddressCount(range, exclusions);
  if (usableAddressCount == 0U)
  {
    throwRuntimeError("multicast address range has no usable addresses after applying exclusions");
  }

  const auto usableAddressIndex = hash % usableAddressCount;
  const auto address = getUsableMulticastAddressAtIndex(usableAddressIndex, range, exclusions);
  SEN_ENSURE(address.has_value());
  SEN_ENSURE(address->is_multicast());
  return *address;
}

Result<MulticastCollisionAnalysis, std::string> analyzeConfiguredMulticastBuses(
  const std::vector<kernel::BusAddress>& configuredBusAddresses,
  uint16_t discoveryPort,
  const MulticastRange& range,
  const MulticastExclusions& exclusions)
{
  MulticastCollisionAnalysis analysis;
  analysis.usableAddressCount = usableMulticastAddressCount(range, exclusions);
  if (analysis.usableAddressCount == 0U)
  {
    return Err(std::string("multicast address range has no usable addresses after applying exclusions"));
  }

  analysis.allocations.reserve(configuredBusAddresses.size());
  std::unordered_map<uint32_t, std::size_t> allocationIndexByGroupAddress;
  allocationIndexByGroupAddress.reserve(configuredBusAddresses.size());

  for (const auto& busAddress: configuredBusAddresses)
  {
    const auto sessionId = crc32(busAddress.sessionName);
    const auto busId = crc32(busAddress.busName);
    const auto groupAddress = computeMulticastAddress(sessionId, busId, discoveryPort, range, exclusions).to_v4();
    ConfiguredBusMulticastAllocation allocation {busAddress, sessionId, busId, groupAddress};

    const auto [groupItr, inserted] =
      allocationIndexByGroupAddress.try_emplace(groupAddress.to_uint(), analysis.allocations.size());
    if (!inserted)
    {
      const auto& existingAllocation = analysis.allocations.at(groupItr->second);
      if (existingAllocation.busAddress == busAddress)
      {
        continue;
      }
      analysis.selfCollisions.push_back(MulticastSelfCollision {existingAllocation, allocation});
    }

    analysis.allocations.push_back(std::move(allocation));
  }

  analysis.collisionProbability =
    computeBirthdayCollisionProbability(analysis.allocations.size(), analysis.usableAddressCount);
  return Ok(std::move(analysis));
}

std::string formatMulticastSelfCollision(const MulticastSelfCollision& collision)
{
  std::string error = "multicast self-collision: buses '";
  error.append(formatBusAddress(collision.firstAllocation.busAddress));
  error.append("' (sessionId=");
  error.append(std::to_string(collision.firstAllocation.sessionId));
  error.append(", busId=");
  error.append(std::to_string(collision.firstAllocation.busId));
  error.append(") and '");
  error.append(formatBusAddress(collision.secondAllocation.busAddress));
  error.append("' (sessionId=");
  error.append(std::to_string(collision.secondAllocation.sessionId));
  error.append(", busId=");
  error.append(std::to_string(collision.secondAllocation.busId));
  error.append(") both map to ");
  error.append(collision.firstAllocation.groupAddress.to_string());
  return error;
}

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
                                             sen::kernel::Tracer& tracer,
                                             TransportCounters& counters,
                                             const MulticastExclusions& exclusions)
{
  return std::shared_ptr<BusHandler>(
    new BusHandler(sessionId, busId, name, procId, listener, discoveryPort, io, config, tracer, counters, exclusions));
}

BusHandler::BusHandler(uint32_t sessionId,
                       kernel::BusId busId,
                       std::string name,
                       kernel::ProcessId procId,
                       kernel::TransportListener* listener,
                       uint16_t discoveryPort,
                       asio::io_context& io,
                       const Configuration& config,
                       sen::kernel::Tracer& tracer,
                       TransportCounters& counters,
                       const MulticastExclusions& exclusions)
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
  , counters_(counters)
{
  const auto group =
    computeMulticastAddress(sessionId, busId.get(), discoveryPort, config.busConfig.multicastRange, exclusions);

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
        us->counters_.udpReceivedBytes.fetch_add(payloadSize, std::memory_order_relaxed);

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
