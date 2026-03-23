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

/// Uniquely identifies a remote participant (process + owner ID pair).
struct ParticipantAddr
{
  ProcessId proc;    ///< Identifier of the remote process.
  ObjectOwnerId id;  ///< Identifier of the object owner within that process.

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

  /// Returns a pooled buffer of exactly @p size bytes, creating one if none is available.
  /// The buffer is automatically returned to the pool when the handle is destroyed.
  /// @param size  Required buffer capacity in bytes.
  /// @return RAII handle owning a `std::vector<uint8_t>` of at least @p size bytes.
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

  /// Returns the buffer manager that provides pooled memory for inbound message handling.
  /// @return Mutable reference to the `ByteBufferManager`; valid for the lifetime of this listener.
  virtual ByteBufferManager& getBufferManager() = 0;

public:
  /// Called when a remote process can no longer be reached.
  /// @param who  Identifier of the process that disconnected.
  virtual void remoteProcessLost(ProcessId who) = 0;

  /// Called when a remote participant joins (or is discovered on) a bus.
  /// @param addr         Network address of the participant.
  /// @param processInfo  Build and identity metadata of the remote process.
  /// @param bus          Opaque bus identifier.
  /// @param busName      Human-readable name of the bus.
  virtual void remoteParticipantJoinedBus(ParticipantAddr addr,
                                          const ProcessInfo& processInfo,
                                          BusId bus,
                                          std::string busName) = 0;

  /// Called when a remote participant leaves a bus.
  /// @param addr     Network address of the participant that left.
  /// @param bus      Opaque bus identifier.
  /// @param busName  Human-readable name of the bus.
  virtual void remoteParticipantLeftBus(ParticipantAddr addr, BusId bus, std::string busName) = 0;

  /// Called when a broadcast message is received on a bus.
  /// @param busId   Identifier of the bus the message arrived on.
  /// @param msg     View over the raw message bytes.
  /// @param buffer  RAII handle keeping the underlying memory alive.
  virtual void remoteBroadcastMessageReceived(BusId busId, Span<const uint8_t> msg, ByteBufferHandle buffer) = 0;

  /// Called when a unicast (direct) message is received.
  /// @param to              Recipient owner identifier.
  /// @param busId           Bus the message was delivered on.
  /// @param msg             View over the raw message bytes.
  /// @param buffer          RAII handle keeping the underlying memory alive.
  /// @param ensureNotDropped If `true`, the message must not be silently discarded.
  virtual void remoteMessageReceived(ObjectOwnerId to,
                                     BusId busId,
                                     Span<const uint8_t> msg,
                                     ByteBufferHandle buffer,
                                     bool ensureNotDropped) = 0;
};

/// Global transport statistics.
struct TransportStats
{
  std::size_t udpSentBytes = 0;      ///< Total bytes sent via UDP (best-effort) since the transport started.
  std::size_t udpReceivedBytes = 0;  ///< Total bytes received via UDP since the transport started.
  std::size_t tcpSentBytes = 0;      ///< Total bytes sent via TCP (reliable) since the transport started.
  std::size_t tcpReceivedBytes = 0;  ///< Total bytes received via TCP since the transport started.
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
  /// Starts the message exchange and registers the listener that handles incoming events.
  /// This method is non-blocking.
  /// @param listener  Callback sink that receives all transport events; must outlive this transport.
  virtual void start(TransportListener* listener) = 0;

  /// Sends a single-buffer message to a remote participant on the given bus.
  /// @param to     Destination participant address.
  /// @param busId  Bus to send on.
  /// @param mode   Transport QoS mode (unicast / multicast / confirmed).
  /// @param data   Memory block containing the serialised message.
  virtual void sendTo(ParticipantAddr& to, BusId& busId, TransportMode mode, MemBlockPtr data) = 0;

  /// Sends a two-buffer (header + payload) message to a remote participant.
  /// @param to     Destination participant address.
  /// @param busId  Bus to send on.
  /// @param mode   Transport QoS mode.
  /// @param data1  First memory block (typically a header).
  /// @param data2  Second memory block (typically the payload).
  virtual void sendTo(ParticipantAddr& to, BusId& busId, TransportMode mode, MemBlockPtr data1, MemBlockPtr data2) = 0;

  /// Notifies remote participants that a local owner has joined a bus.
  /// @param participant  Local owner identifier.
  /// @param bus          Opaque bus identifier.
  /// @param busName      Human-readable bus name.
  virtual void localParticipantJoinedBus(ObjectOwnerId participant, BusId bus, const std::string& busName) = 0;

  /// Notifies remote participants that a local owner has left a bus.
  /// @param participant  Local owner identifier.
  /// @param bus          Opaque bus identifier.
  /// @param busName      Human-readable bus name.
  virtual void localParticipantLeftBus(ObjectOwnerId participant, BusId bus, const std::string& busName) = 0;

  /// Returns the `ProcessInfo` of the local process as seen by this transport.
  /// @return Const reference to the local `ProcessInfo`; valid for the lifetime of this transport.
  [[nodiscard]] virtual const ProcessInfo& getOwnInfo() const noexcept = 0;

  /// Stops the message exchange and releases all network resources.
  virtual void stop() noexcept = 0;

  virtual void stopIO() noexcept {}

  /// Returns a snapshot of transport-layer statistics (bytes sent/received).
  /// @return `TransportStats` populated at the time of the call.
  [[nodiscard]] virtual TransportStats fetchStats() const = 0;

public:  // timers
  /// Starts an asynchronous timer that fires after @p timeout and invokes @p timeoutCallback.
  /// @param timeout          Duration to wait before firing.
  /// @param timeoutCallback  Callable invoked when the timer expires.
  /// @return Opaque `TimerId` that can be passed to `cancelTimer()`.
  virtual TimerId startTimer(std::chrono::steady_clock::duration timeout, std::function<void()>&& timeoutCallback) = 0;

  /// Cancels a previously started timer, preventing its callback from firing.
  /// @param id  Timer identifier returned by `startTimer()`.
  /// @return `true` if the timer was successfully cancelled before expiry; `false` otherwise.
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
