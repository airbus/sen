// === transport.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_KERNEL_TRANSPORT_H
#define SEN_KERNEL_TRANSPORT_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/memory_block.h"
#include "sen/core/base/move_only_function.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/span.h"
#include "sen/core/base/strong_type.h"
#include "sen/core/io/output_stream.h"
#include "sen/core/obj/object_provider.h"
#include "sen/kernel/tracer.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"

// std
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

/// Strong type for the unique identifier for a transport timer
SEN_STRONG_TYPE(TimerId, uint64_t)
SEN_STRONG_TYPE_HASHABLE(TimerId)

namespace sen::kernel
{
/// \addtogroup kernel
/// @{

SEN_STRONG_TYPE(ProcessId, uint32_t)

struct BusId: public StrongType<uint32_t, BusId>
{
  using Base = StrongType<uint32_t, BusId>;
  using Base::Base;
  using Base::ValueType;

  void* userData = nullptr;  ///< Allows transport implementations to inject information.
};

/// To identify remote participants.
struct ParticipantAddr
{
  ProcessId proc;
  ObjectOwnerId id;

  void* userData = nullptr;  ///< Allows transport implementations to inject information.
};

[[nodiscard]] bool operator==(const ParticipantAddr& lhs, const ParticipantAddr& rhs) noexcept;
[[nodiscard]] bool operator!=(const ParticipantAddr& lhs, const ParticipantAddr& rhs) noexcept;

/// The maximum UDP payload size.
constexpr std::size_t maxBestEffortMessageSize = 65527U;

/// Pointer to a memory block of fixed size, used for best effort communication.
using BestEffortBlockPtr = std::shared_ptr<FixedMemoryBlock>;

/// Pointer to a resizable memory block, used for reliable communication.
using ReliableBlockPtr = std::shared_ptr<ResizableHeapBlock>;

/// A list (vector) of fixed-size memory blocks.
using BestEffortBufferList = std::vector<BestEffortBlockPtr>;

/// The protocol version of kernel messaging.
[[nodiscard]] uint32_t getKernelProtocolVersion() noexcept;

class UniqueByteBufferManagerImpl;

class UniqueByteBufferManager final
{
  SEN_MOVE_ONLY(UniqueByteBufferManager)
  using BufferType = std::vector<uint8_t>;

public:
  using ByteBufferHandle = std::unique_ptr<BufferType, sen::std_util::move_only_function<void(BufferType*)>>;

  UniqueByteBufferManager();
  ~UniqueByteBufferManager();

  /// Returns a handle to a buffer that fix size elements.
  ///
  /// @param size: of the buffer
  ByteBufferHandle getBuffer(size_t size);

private:
  std::unique_ptr<UniqueByteBufferManagerImpl> pImpl_;
};

/// Interface for the internal Sen element that reacts to transport events.
class TransportListener
{
  SEN_NOCOPY_NOMOVE(TransportListener)

public:  // special members
  TransportListener() = default;
  virtual ~TransportListener() = default;

public:
  using ByteBufferManager = UniqueByteBufferManager;
  using ByteBufferHandle = ByteBufferManager::ByteBufferHandle;

  /// Returns access to the buffer manager for easy access to memory buffers.
  virtual ByteBufferManager& getBufferManager() = 0;

public:
  /// A process is not reachable anymore.
  virtual void remoteProcessLost(ProcessId who) = 0;

  /// Some participant is part of, or recently joined a bus.
  virtual void remoteParticipantJoinedBus(ParticipantAddr addr,
                                          const ProcessInfo& processInfo,
                                          BusId bus,
                                          std::string busName) = 0;

  /// Some participant left a bus.
  virtual void remoteParticipantLeftBus(ParticipantAddr addr, BusId bus, std::string busName) = 0;

  /// Message received on a bus via broadcast.
  virtual void remoteBroadcastMessageReceived(BusId busId, Span<const uint8_t> msg, ByteBufferHandle buffer) = 0;

  /// Direct message received on a bus (independently of the communication channel).
  virtual void remoteMessageReceived(ObjectOwnerId to,
                                     BusId busId,
                                     Span<const uint8_t> msg,
                                     ByteBufferHandle buffer,
                                     bool ensureNotDropped) = 0;
};

/// Global transport statistics.
struct TransportStats
{
  std::size_t udpSentBytes = 0;
  std::size_t udpReceivedBytes = 0;
  std::size_t tcpSentBytes = 0;
  std::size_t tcpReceivedBytes = 0;
};

/// Interface for implementing a Sen inter-process transport solution.
class Transport
{
  SEN_NOCOPY_NOMOVE(Transport)

public:
  /// A transport instance is created per connected session (which is unique within a kernel).
  explicit Transport(std::string_view sessionName);
  virtual ~Transport() = default;

public:
  /// Start the message exchange, and set the listener that will react to inputs.
  /// This method is non-blocking.
  virtual void start(TransportListener* listener) = 0;

  /// Send a message over a bus to a participant using a given mode.
  virtual void sendTo(ParticipantAddr& to, BusId& busId, TransportMode mode, MemBlockPtr data) = 0;

  /// Send a compound message (typically header and data) over a bus to a participant using a given mode.
  virtual void sendTo(ParticipantAddr& to, BusId& busId, TransportMode mode, MemBlockPtr data1, MemBlockPtr data2) = 0;

  /// Notify other participants that there's a local participant that recently joined a bus.
  virtual void localParticipantJoinedBus(ObjectOwnerId participant, BusId bus, const std::string& busName) = 0;

  /// Notify other participants that there's a local participant that recently left a bus.
  virtual void localParticipantLeftBus(ObjectOwnerId participant, BusId bus, const std::string& busName) = 0;

  /// The process info of this process.
  [[nodiscard]] virtual const ProcessInfo& getOwnInfo() const noexcept = 0;

  /// Stop the message exchange.
  virtual void stop() noexcept = 0;

  virtual void stopIO() noexcept {}

  /// Fetch the current statistics.
  [[nodiscard]] virtual TransportStats fetchStats() const = 0;

public:  // timers
  /// Starts an asio asynchronous timer that executes a callback when the steady_clock timeout is reached
  virtual TimerId startTimer(std::chrono::steady_clock::duration timeout, std::function<void()>&& timeoutCallback) = 0;

  /// Cancels a timer, avoiding its expiration
  virtual bool cancelTimer(TimerId id) = 0;

private:
  std::string name_;
};

/// A function that creates a transport given a session name.
using TransportFactory = std::function<std::unique_ptr<Transport>(const std::string&, std::unique_ptr<Tracer> tracer)>;

/// @}
}  // namespace sen::kernel

namespace sen
{
template <>
struct ShouldBePassedByValue<kernel::ProcessId>: std::true_type
{
};

template <>
struct ShouldBePassedByValue<kernel::BusId>: std::true_type
{
};
}  // namespace sen

SEN_STRONG_TYPE_HASHABLE(sen::kernel::BusId)

SEN_STRONG_TYPE_HASHABLE(sen::kernel::ProcessId)

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

template <>
struct std::hash<sen::kernel::ParticipantAddr>
{
  template <class T>
  void hashCombine(std::size_t& seed, const T& v) const noexcept
  {
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);  // NOLINT
  }

  std::size_t operator()(const sen::kernel::ParticipantAddr& val) const noexcept
  {
    std::size_t seed = 150783;  // NOLINT(readability-magic-numbers)
    hashCombine(seed, val.proc);
    hashCombine(seed, val.id);
    return seed;
  }
};

#endif  // SEN_KERNEL_TRANSPORT_H
