// === bus.h ===========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_BUS_BUS_H
#define SEN_LIBS_KERNEL_SRC_BUS_BUS_H

#include "local_participant.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/meta/type.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_provider.h"
#include "sen/kernel/transport.h"

// spdlog
#include <spdlog/fwd.h>

// generated code
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/type_specs.stl.h"

// std
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace sen::kernel::impl
{

class Session;

/// Identifies what to expect in our message buffers
enum class MessageCategory : uint8_t
{
  controlMessage = 0U,
  runtimeObjectUpdate = 1U,
  runtimeMethodCallBestEffort = 2U,
  runtimeMethodCallConfirmed = 3U,
  runtimeMethodResponse = 4U,
  runtimeEvents = 5U,
};

/// The header and payload reference for an object update message.
struct ObjectUpdateMessage
{
  ObjectId objectId;
  TimeStamp time;
  Span<const uint8_t> buffer;
};

/// Internal representation of a bus.
/// Owns the RemoteParticipant objects and holds any LocalParticipant.
class Bus
{
public:
  SEN_NOCOPY_NOMOVE(Bus)

public:
  Bus(std::string_view name, BusId id, Session* session, CustomTypeRegistry& localTypes);

  ~Bus();

public:
  [[nodiscard]] const BusAddress& getAddress() const noexcept { return address_; }

  [[nodiscard]] const BusId& getId() const noexcept { return id_; }

public:
  void connect(LocalParticipant* participant);

  void disconnect(LocalParticipant* participant);

public:
  void remoteParticipantJoined(ParticipantAddr address, ProcessInfo processInfo);

  void remoteParticipantLeft(const ParticipantAddr& addr);

  void remoteParticipantRemoved(RemoteParticipant* remote);

  void removeRemotesFromProcess(ProcessId processId);

  void remoteMessageReceived(ObjectOwnerId to, Span<const uint8_t> msg);

  void remoteBroadcastMessageReceived(Span<const uint8_t> msg);

  void remoteSubscriberAdded(std::shared_ptr<Interest> interest, RemoteParticipant* listener, bool notifyAboutExisting);

  void remoteSubscriberRemoved(std::shared_ptr<Interest> interest,
                               RemoteParticipant* listener,
                               bool notifyAboutExisting);

  void remoteSubscriberRemoved(RemoteParticipant* listener, bool notifyAboutExisting);

  void publicationsRejected(const PublicationRejection& rejections, RemoteParticipant* whom);

  /// Finds a remote type stored in the remote participants in the bus using the typeHash. Also returns a boolean flag
  /// set to true if the remote type is runtime-incompatible with its local version
  [[nodiscard]] std::pair<MaybeConstTypeHandle<>, bool> searchForTypeInRemoteParticipants(
    const RemoteParticipant* requestor,
    uint32_t typeHash);

  /// If any of the objects published in the bus has the input name, the registration time of that object is returned
  [[nodiscard]] std::optional<TimeStamp> isObjectNameUsedLocally(std::string_view name);

private:
  bool updateLocalTypesSpecs();
  void sendMessageToParticipant(MessageCategory category, InputStream& in, RemoteParticipant& participant);
  [[nodiscard]] static ObjectUpdateMessage readObjectUpdateData(InputStream& in);

private:
  using Lock = std::scoped_lock<std::recursive_mutex>;

private:
  BusAddress address_;
  BusId id_;
  Session* session_;
  std::vector<LocalParticipant*> locals_;
  std::recursive_mutex localsMutex_;
  std::unordered_map<ObjectOwnerId, std::shared_ptr<RemoteParticipant>> remotes_;
  std::recursive_mutex remotesMutex_;
  CustomTypeRegistry& localTypes_;
  std::vector<CustomTypeSpec> localTypesSpecs_;
  std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_BUS_BUS_H
