// === local_participant.h =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_BUS_LOCAL_PARTICIPANT_H
#define SEN_LIBS_KERNEL_SRC_BUS_LOCAL_PARTICIPANT_H

// generated
#include "bus_protocol.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/type_specs.stl.h"

// kernel
#include "participant.h"
#include "proxy_manager.h"
#include "remote_interest_manager.h"
#include "session.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/span.h"
#include "sen/core/obj/detail/native_object_impl.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object_filter.h"
#include "sen/core/obj/object_provider.h"
#include "sen/core/obj/object_source.h"
#include "sen/kernel/transport.h"

// std
#include <list>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

// spdlog
#include <spdlog/fwd.h>

namespace sen::kernel::impl
{

class Bus;
class SessionManager;
class RemoteParticipant;
class Session;

/// A local participant of a bus. There can be multiple LocalParticipants in the same component.
class LocalParticipant: public Participant, public ObjectSource, public std::enable_shared_from_this<LocalParticipant>
{
public:
  SEN_NOCOPY_NOMOVE(LocalParticipant)

public:
  LocalParticipant(ObjectOwnerId id,
                   const BusAddress& busAddress,
                   std::shared_ptr<impl::Session> session,
                   impl::Runner* owner,
                   ::sen::impl::WorkQueue& workQueue);
  ~LocalParticipant() override;

public:
  [[nodiscard]] impl::Runner* getOwner() noexcept { return owner_; }
  [[nodiscard]] const std::string& getDebugName() const noexcept { return debugName_; }
  [[nodiscard]] spdlog::logger& getLogger() const noexcept { return *logger_; }

public:
  void connect();

public:  // implements Participant
  [[nodiscard]] ObjectFilterBase& getIncomingInterestManager() noexcept final { return remoteInterestsManager_; }
  [[nodiscard]] ObjectOwnerId getId() const noexcept final { return id_; }
  [[nodiscard]] BusId getBusId() const noexcept final;

  /// If any of the local objects managed by this local participant has the input name, the function returns the
  /// registration timestamp of that object
  [[nodiscard]] std::optional<TimeStamp> objectNameUsed(std::string_view name) const;

public:  // implements ObjectSource
  bool add(const Span<NativeObjectPtr>& instances) final;
  void remove(const Span<NativeObjectPtr>& instances) final;
  void flushOutputs() override;
  void drainInputs() override;

protected:  // implements ObjectFilter
  void subscriberAdded(std::shared_ptr<Interest> interest,
                       ObjectProviderListener* listener,
                       bool notifyAboutExisting) final;
  void subscriberRemoved(std::shared_ptr<Interest> interest,
                         ObjectProviderListener* listener,
                         bool notifyAboutExisting) final;
  void subscriberRemoved(ObjectProviderListener* listener, bool notifyAboutExisting) final;

public:
  void remoteParticipantAdded(RemoteParticipant* other, bool notifyAboutExisting);
  void remoteParticipantRemoved(RemoteParticipant* other, bool notifyAboutExisting);
  void localParticipantAdded(LocalParticipant* other, bool notifyAboutExisting);
  void localParticipantRemoved(LocalParticipant* other, bool notifyAboutExisting);
  [[nodiscard]] BusAddress getBusAddress() const noexcept;
  [[nodiscard]] const std::string& getObjectsNamePrefix() const noexcept { return objectsNamePrefix_; }
  [[nodiscard]] bool handleRejectedPublications(const PublicationRejection& rejections, RemoteParticipant* whom);

public:  // forwarding of local interests
  void localSubscriberAdded(const std::shared_ptr<Interest>& interest,
                            ObjectProviderListener* listener,
                            bool notifyAboutExisting);
  void localSubscriberRemoved(const std::shared_ptr<Interest>& interest,
                              ObjectProviderListener* listener,
                              bool notifyAboutExisting);
  void localSubscriberRemoved(ObjectProviderListener* listener, bool notifyAboutExisting);

public:  // remote interests
  void remoteSubscriberAdded(const std::shared_ptr<Interest>& interest,
                             RemoteParticipant* listener,
                             bool notifyAboutExisting);
  void remoteSubscriberRemoved(const std::shared_ptr<Interest>& interest,
                               RemoteParticipant* listener,
                               bool notifyAboutExisting);
  void remoteSubscriberRemoved(RemoteParticipant* listener, bool notifyAboutExisting);

private:
  void evaluateLocalObjects(const ObjectSet& objects);

private:
  using Mutex = std::recursive_mutex;
  using Lock = std::scoped_lock<Mutex>;

private:
  void flushLocalAdditionsAndRemovals();
  void flushLocalObjectsState(const std::list<::sen::impl::SerializableEvent>& events);
  void removeSingleObject(Object* instance);

private:
  impl::Runner* owner_;
  std::shared_ptr<Session> session_;
  Bus* bus_;
  ObjectOwnerId id_;
  mutable Mutex busMutex_;
  ObjectFilter::ObjectSet localObjects_;
  RemoteInterestsManager remoteInterestsManager_;
  ::sen::impl::WorkQueue& workQueue_;
  std::shared_mutex interestsOnOthersMutex_;
  std::vector<std::shared_ptr<ProxyManager>> interestsOnOthers_;
  std::mutex participantsMutex_;
  std::vector<Participant*> participants_;
  std::string objectsNamePrefix_;
  std::shared_ptr<spdlog::logger> logger_;
  std::string debugName_;
  std::set<std::string> localObjectNames_;
  std::unordered_map<ObjectId, std::shared_ptr<Object>> tempNewObjects_;
  std::unordered_map<ObjectId, std::shared_ptr<Object>>* targetNewObjects_;
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------
// Helper functions
//--------------------------------------------------------------------------------------------------------------

[[noreturn]] inline void throwRepeatedName(std::string_view name)
{
  std::string err;
  err.append("repeated local object name '");
  err.append(name);
  err.append("'");
  throwRuntimeError(err);
}

inline void throwIfNamePresent(std::string_view name, const std::vector<Object*>& list)
{
  for (const auto* obj: list)
  {
    if (obj->getName() == name)
    {
      throwRepeatedName(name);
    }
  }
}

//--------------------------------------------------------------------------------------------------------------
// LocalParticipant
//--------------------------------------------------------------------------------------------------------------

inline void LocalParticipant::evaluateLocalObjects(const ObjectSet& objects)
{
  // redirect the new objects that may be created during evaluation
  targetNewObjects_ = &tempNewObjects_;
  ObjectFilter::evaluate(objects);
  targetNewObjects_ = &localObjects_.newObjects;
}

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_BUS_LOCAL_PARTICIPANT_H
