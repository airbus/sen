// === remote_participant.h ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_BUS_REMOTE_PARTICIPANT_H
#define SEN_LIBS_KERNEL_SRC_BUS_REMOTE_PARTICIPANT_H

// implementation
#include "bus/bus.h"
#include "bus/session.h"
#include "bus_protocol.stl.h"
#include "containers.h"
#include "participant.h"
#include "remote_interest_manager.h"

// sen
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/memory_block.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/io/input_stream.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/obj/detail/remote_object.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_filter.h"
#include "sen/core/obj/object_provider.h"

// kernel
#include "sen/kernel/transport.h"

// spdlog
#include <spdlog/fwd.h>

// generated code
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/type_specs.stl.h"

// std
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace sen::kernel::impl
{
class RemoteParticipant;

class RemoteProvider final: public ObjectProvider
{
public:
  SEN_NOCOPY_NOMOVE(RemoteProvider)

public:
  explicit RemoteProvider(std::shared_ptr<Interest> interest): interest_(std::move(interest)) {}

  ~RemoteProvider() override { notifyRemovedOnExistingObjectsForAllListeners(); }

public:
  [[nodiscard]] const Interest& getInterest() const noexcept { return *interest_; }

  /// Given a list of additions, returns the list of object IDs that are already stored in the current additions
  [[nodiscard]] ObjectIdList getRepeatedAdditions(const ObjectAdditionList& additions);

public:  // implements ObjectProvider
  void notifyObjectsAdded(const ObjectAdditionList& additions) override;
  void notifyObjectsRemoved(const ObjectRemovalList& removals) override;

protected:  // implements ObjectProvider
  void notifyAddedOnExistingObjects(ObjectProviderListener* listener) override;
  void notifyRemovedOnExistingObjects(ObjectProviderListener* listener) override;

private:
  std::shared_ptr<Interest> interest_;
  std::unordered_map<ObjectId, ObjectAddition> currentAdditions_;
};

/// Object Filter customized for RemoteParticipants.
class RemoteObjectFilter final: public ObjectFilterBase
{
public:
  SEN_NOCOPY_NOMOVE(RemoteObjectFilter)

public:
  explicit RemoteObjectFilter(RemoteParticipant* owner);
  ~RemoteObjectFilter() override;

public:
  void addSubscriber(std::shared_ptr<Interest> interest,
                     ObjectProviderListener* listener,
                     bool notifyAboutExisting) override;
  void removeSubscriber(std::shared_ptr<Interest> interest,
                        ObjectProviderListener* listener,
                        bool notifyAboutExisting) override;
  void removeSubscriber(ObjectProviderListener* listener, bool notifyAboutExisting) override;

public:
  void remoteObjectsAdded(InterestId interestId, const ObjectAdditionList& additions);
  void remoteObjectsRemoved(InterestId interestId, const ObjectRemovalList& removals);

  /// Given a list of additions, commands the remote provider to return a list with the ids of the repeated additions
  [[nodiscard]] ObjectIdList getRepeatedAdditions(InterestId interestId, const ObjectAdditionList& additions);

private:
  RemoteParticipant* owner_;
  std::unordered_map<InterestId, std::unique_ptr<RemoteProvider>> providers_;
};

/// Storage for object updates
class UpdatesBufferStorage
{
  SEN_NOCOPY_NOMOVE(UpdatesBufferStorage)

public:
  UpdatesBufferStorage() = default;
  ~UpdatesBufferStorage() = default;

public:
  struct BufferedObjectUpdate
  {
    TimeStamp time;               /// The Sen kernel time when produced.
    std::vector<uint8_t> buffer;  /// Serialized properties.
  };

  struct BufferedObjectData
  {
    /// Unique id for the transport timer tracking the expiration of the buffered object data
    TimerId timerId;

    /// Static state (static properties) of a received object addition
    ObjectAdded staticState;

    /// Full dynamic state for the object requested by the participant (updates will be
    /// added only if they are newer than it)
    ObjectState dynamicState;

    /// Buffered updates for the object
    std::vector<BufferedObjectUpdate> updates;
  };

  using ObjectIdToBufferedData = std::unordered_map<ObjectId, BufferedObjectData>;

public:
  /// If needed, it adds a new entry to the buffer storage by copying the data.
  void addUpdatesIfNeeded(const ObjectUpdateMessage& message);

  /// Stores the initial state of the object and enables its monitoring.
  void startMonitoring(const ObjectAdded& addition, Transport& transport, std::function<void()>&& timeoutCallback);

  /// Stores the dynamic state of the object in the buffer
  void setDynamicProperties(const ObjectState& objState);

  /// Erases the associated data and stops monitoring the object.
  void stopMonitoring(ObjectId objectId, Transport& transport);

  /// Applies any stored initial state and received updates, if any.
  /// Erases the associated data and stops monitoring the object.
  void applyStateAndStopMonitoring(sen::impl::RemoteObject& proxy, Transport& transport);

  /// Returns the static state of the object, finding it using its object id. Returns nullptr if the object has not been
  /// found among the monitored objects
  [[nodiscard]] const ObjectAdded* getStaticState(const ObjectId& id) const noexcept;

private:
  ObjectIdToBufferedData data_;
};

/// Represents a remote participant.
class RemoteParticipant: public Participant,
                         public ObjectProviderListener,
                         public std::enable_shared_from_this<RemoteParticipant>
{
public:
  SEN_NOCOPY_NOMOVE(RemoteParticipant)

public:
  RemoteParticipant(ProcessInfo processInfo, ParticipantAddr address, Bus* bus, Session* session);
  ~RemoteParticipant() override;

public:
  [[nodiscard]] const ParticipantAddr& getAddress() const noexcept { return ownerAddress_; }
  [[nodiscard]] const ProcessInfo& getProcessInfo() const noexcept { return processInfo_; }
  void markRemoved() noexcept { removed_ = true; }
  [[nodiscard]] const std::string& getDebugName() const noexcept { return debugName_; }

  /// Searches for a type with a certain hash in the remote registry and returns it. Also returns a boolean flag set
  /// to true if the remote type is runtime-incompatible with its local version
  [[nodiscard]] std::pair<MaybeConstTypeHandle<>, bool> searchForRemoteType(uint32_t typeHash) const;

public:
  /// Sends a hello message to the participant being represented by this remote participant
  void sendRemoteParticipantReady(ObjectOwnerId ownerId);
  void sendObjectUpdate(ObjectOwnerId ownerId, ObjectUpdate* update);
  void sendEventsUnicast(BestEffortBufferList&& events, ObjectOwnerId owner);
  void sendEventsMulticast(BestEffortBufferList&& events, ObjectOwnerId owner);
  void sendEventsConfirmed(MemBlockPtr events, ObjectOwnerId owner);

public:  // implements Participant
  [[nodiscard]] ObjectOwnerId getId() const noexcept override { return ownerAddress_.id; }
  [[nodiscard]] BusId getBusId() const noexcept override { return busId_; }
  [[nodiscard]] ObjectFilterBase& getIncomingInterestManager() noexcept override { return incomingInterestsManager_; }

public:  // implements ObjectProviderListener
  [[nodiscard]] RemoteParticipant* isRemoteParticipant() noexcept override { return this; }

public:  // these methods are called by the bus
  void rcvControlMessage(InputStream& in);
  void rcvObjectUpdate(const ObjectUpdateMessage& message);
  void rcvMethodCall(TransportMode mode, InputStream& in);
  void rcvMethodResponse(InputStream& in);
  void rcvEvents(InputStream& in);

protected:  // implements ObjectProviderListener
  void onObjectsAdded(const ObjectAdditionList& additions) override;
  void onObjectsRemoved(const ObjectRemovalList& removals) override;

private:
  friend class RemoteObjectFilter;
  void localInterestStarted(const LocalParticipant& source, std::shared_ptr<Interest> interest);
  void localInterestStopped(const LocalParticipant& source, InterestId interestId);

private:
  std::shared_ptr<::sen::impl::RemoteObject> startTrackingProxy(const ClassType& proxyClass,
                                                                ::sen::impl::RemoteObjectInfo&& info);
  void stopTrackingProxy(ObjectId objectId);
  void proxyAboutToBeDeleted(::sen::impl::RemoteObject* proxy);
  void sendUpdates(ObjectOwnerId ownerId,
                   const NativeObject* object,
                   MemBlockPtr&& propertiesBuffer,
                   TransportMode mode);
  void sendEventsBuffer(MemBlockPtr&& events, ParticipantAddr& owner, TransportMode mode);
  [[nodiscard]] ::sen::impl::SendCallFunc getProxyCallFunc(const ClassType* meta);

private:
  void remoteParticipantReady(const RemoteParticipantReady& msg);
  void remoteInterestStarted(InterestStarted& msg);
  void remoteInterestStopped(InterestStopped& msg);
  void remoteObjectsPublished(ObjectsPublished& msg);
  void remoteObjectsRemoved(const ObjectsRemoved& msg);
  void publicationRejected(const PublicationRejection& msg);
  void typesInfoRequest(const TypesInfoRequest& msg);
  void typesInfoResponse(const TypesInfoResponse& msg);
  void typesInfoRejection(const TypesInfoRejection& msg);
  void processObjectAddition(ObjectIdList& additions,
                             PublicationRejection& rejectionResponse,
                             const ObjectAdded& object,
                             uint32_t interestId,
                             uint32_t ownerId);
  void objectsStateRequest(const ObjectsStateRequest& msg);
  void objectsStateResponse(const ObjectsStateResponse& msg);
  /// Returns true if any of the pending classes was resolved
  [[nodiscard]] bool tryResolvePendingClasses();
  void addOrRejectObjectsOfClass(uint32_t typeHash);
  void registerRemoteType(const CustomTypeSpec& remoteTypeSpec);
  void registerRemoteTypeFoundInBus(ConstTypeHandle<CustomType> type, bool isIncompatible);

private:
  void sendToBus(ParticipantAddr& to, TransportMode mode, MemBlockPtr&& hdr, MemBlockPtr&& data);
  void sendToBus(ParticipantAddr& to, TransportMode mode, MemBlockPtr&& data);
  template <typename T>
  void sendControlMessage(ParticipantAddr to, T&& message);

private:
  [[nodiscard]] RemoteObjectDiscovery makeRemoteObjectDiscovery(const ObjectAdded& addition, InterestId interestId);
  [[nodiscard]] MemBlockPtr makeObjectUpdateHdr(const NativeObject* object, uint32_t propertiesBufferSize) const;
  [[nodiscard]] MemBlockPtr makeEventsHeader() const;
  [[nodiscard]] MemBlockPtr makeObjectNotFoundResponseHeader(ObjectId objectId, uint32_t ticketId) const;
  [[nodiscard]] MemBlockPtr makeMethodResponseHeader(ObjectId objectId, uint32_t ticketId, uint32_t bufferSize) const;
  [[nodiscard]] MemBlockPtr makeMethodCallHeader(TransportMode& mode,
                                                 ObjectOwnerId ownerId,
                                                 ObjectId objectId,
                                                 MemberHash methodId,
                                                 ::sen::impl::CallId callId,
                                                 uint32_t argsBufferSize,
                                                 const ClassType* classType) const;
  [[nodiscard]] static MemBlockPtr makeRuntimeErrorResponseHeader(ObjectId objectId,
                                                                  uint32_t ticketId,
                                                                  const std::string& errorMsg);
  [[nodiscard]] static MemBlockPtr makeLogicErrorResponseHeader(ObjectId objectId,
                                                                uint32_t ticketId,
                                                                const std::string& errorMsg);
  [[nodiscard]] MemBlockPtr makeUnknownErrorResponseHeader(ObjectId objectId, uint32_t ticketId) const;
  [[nodiscard]] ParticipantAddr getRemoteAddress(ObjectOwnerId ownerId) const noexcept;
  void tryReadControlMsg(InputStream& in, ControlMessage& msg) const;
  void sendPendingInterestStartedMessages();

  struct PropertyCount
  {
    uint32_t staticProps;
    uint32_t dynamicProps;
  };

  [[nodiscard]] const PropertyCount& getOrComputePropertyCount(const ClassType& meta) noexcept;

private:
  using Lock = std::scoped_lock<std::recursive_mutex>;
  static constexpr std::size_t smallBufferSize = 64U;
  using SmallPool = FixedMemoryBlockPool<smallBufferSize>;
  using RemoteObjectListPtr = std::shared_ptr<std::vector<std::shared_ptr<::sen::impl::RemoteObject>>>;
  using InterestIdToObjectAdditionsMap = std::unordered_map<InterestId, ObjectAddedList>;
  using ParticipantIdToPendingAdditionsMap = std::unordered_map<ObjectOwnerId, InterestIdToObjectAdditionsMap>;
  using TypeHashToPendingParticipantsAdditionsMap = std::unordered_map<MemberHash, ParticipantIdToPendingAdditionsMap>;
  using PendingClassesList = std::list<ClassSpecResponse>;

private:
  ProcessInfo processInfo_;
  ParticipantAddr ownerAddress_;
  BusId busId_;
  Bus* bus_;
  Session* session_;
  RemoteObjectFilter incomingInterestsManager_;
  ::sen::impl::CallId nextCallId_ = 0U;
  std::unordered_map<InterestId, std::shared_ptr<Interest>> remoteInterests_;
  std::recursive_mutex usageMutex_;
  std::unordered_map<ObjectId, RemoteObjectListPtr> trackedProxies_;
  std::unordered_map<ObjectId, NativeObject*> knownLocalObjects_;
  std::shared_ptr<SmallPool> smallBufferPool_;
  CustomTypeRegistry remoteTypesRegistry_;
  std::vector<ConstTypeHandle<CustomType>> remoteTypesStorage_;
  bool removed_ = false;
  std::shared_ptr<spdlog::logger> logger_;
  std::string debugName_;

  /// Additions that will be requested until their types are received if not already present. Additions are stored by
  /// meta-type, owner id and interest id
  TypeHashToPendingParticipantsAdditionsMap pendingAdditions_;
  PendingClassesList pendingClassSpecsList_;
  std::unordered_map<MemberHash, PendingClassesList::iterator> pendingClassSpecsMap_;
  std::unordered_set<MemberHash> incompatibleClassHashes_;

  /// Marks the remote participant as ready for receiving interest started messages and the subsequent object additions
  bool participantReady_ = false;

  /// Local interest started messages that are pending to be sent to the remote process
  std::unordered_map<ObjectOwnerId, std::vector<InterestStarted>> pendingInterestStartedMessages_;

  /// Objects whose updates are monitored until their addition is fully resolved
  UpdatesBufferStorage monitoredObjects_;

  /// Caches property counts per meta-type. Used when transferring object states between remote participants
  std::unordered_map<MemberHash, PropertyCount> classIdToPropertyCountMap_;
};
}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_BUS_REMOTE_PARTICIPANT_H
