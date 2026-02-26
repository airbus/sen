// === remote_interest_manager.h =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_BUS_REMOTE_INTEREST_MANAGER_H
#define SEN_LIBS_KERNEL_SRC_BUS_REMOTE_INTEREST_MANAGER_H

#include "remote_interest_handler.h"
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/mutex_utils.h"
#include "sen/core/base/span.h"
#include "sen/core/base/static_vector.h"
#include "sen/core/obj/detail/native_object_impl.h"
#include "sen/core/obj/interest.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object.h"
#include "sen/core/obj/object_filter.h"
#include "sen/core/obj/object_provider.h"
#include "sen/kernel/transport.h"
#include "util.h"

#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

namespace sen::kernel::impl
{
class RemoteParticipant;

/// Stores the information used for updating an object. This includes
/// the different buffers used in the different transport modes.
/// Instances of this class are kept alive together with the object for which updates
/// might be needed.
class ObjectUpdate
{
public:
  SEN_MOVE_ONLY(ObjectUpdate)

public:
  explicit ObjectUpdate(NativeObject* localObject);
  ~ObjectUpdate() = default;

public:
  /// Collects all the property updates from the object into the internal buffers.
  void collect(FixedMemoryBlockPool<maxBestEffortMessageSize>& bestEffortPool);

  /// Applies the function "func" to the associated native object. If the object is
  /// not alive anymore, no function is called.
  template <typename F>
  void applyToNativeObject(F&& func);

  /// The buffer used to store confirmed property updates.
  [[nodiscard]] ReliableBlockPtr getReliableProperties() noexcept { return reliableProperties_; }

  /// The buffer used to store best-effort unicast property updates.
  [[nodiscard]] BestEffortBufferList& getUnicastProperties() noexcept { return unicastProperties_; }

  /// The buffer used to store best-effort multicast property updates.
  [[nodiscard]] BestEffortBufferList& getMulticastProperties() noexcept { return multicastProperties_; }

  /// True if the multicast property updates has been sent.
  [[nodiscard]] bool getMulticastPropsSent() const noexcept { return multicastPropsSent_; }

  /// Sets the flag indicating if the multicast property updates has been sent.
  void setMulticastPropsSent(bool value) noexcept { multicastPropsSent_ = value; }

private:
  /// Resets the update, clearing the internal buffers.
  void reset();

private:
  std::weak_ptr<Object> objectGuard_;
  NativeObject* nativeObject_;
  ReliableBlockPtr reliableProperties_;
  BestEffortBufferList unicastProperties_;
  BestEffortBufferList multicastProperties_;
  std::size_t maxReliableBufferSize_ {0U};
  bool multicastPropsSent_ = false;
};

class LocalParticipant;

/// Manages the interests that a local participant might have
/// on objects coming from remote participants.
class RemoteInterestsManager final: public ObjectFilter
{
public:
  SEN_NOCOPY_NOMOVE(RemoteInterestsManager)

public:
  RemoteInterestsManager(ObjectOwnerId ownerIdRef, LocalParticipant* owner);
  ~RemoteInterestsManager() override;

public:
  using ObjectFilter::evaluate;

public:
  void sendToRelevantRemoteParticipants(const std::list<::sen::impl::SerializableEvent>& events);

  /// Prevents this RemoteInterestManager from notifying updates of a certain object to a certain listener. Called
  /// after receiving a RejectionResponse due to runtime incompatibility after the owner (LocalParticipant) published
  /// an object.
  void markObjectAsRejected(ObjectId objectId, ObjectProviderListener* listener);

protected:  // implements ObjectFilter
  void subscriberAdded(std::shared_ptr<Interest> interest,
                       ObjectProviderListener* listener,
                       bool notifyAboutExisting) override;
  void subscriberRemoved(std::shared_ptr<Interest> interest,
                         ObjectProviderListener* listener,
                         bool notifyAboutExisting) override;
  void subscriberRemoved(ObjectProviderListener* listener, bool notifyAboutExisting) override;
  void objectsAdded(std::shared_ptr<Interest> interest, ObjectAdditionList additions) override;
  void objectsRemoved(std::shared_ptr<Interest> interest, ObjectRemovalList removals) override;

private:
  void removeObjectById(const ObjectId& objectId);
  void sendEvents(const std::list<::sen::impl::SerializableEvent>& events);
  void sendObjectUpdates();
  void updateRemoteReference();

private:
  using BestEffortPool = FixedMemoryBlockPool<maxBestEffortMessageSize>;

private:
  LocalParticipant* owner_;
  RemoteInterestsHandler remoteInterestsHandler_;
  BiMap<ObjectUpdate*, ObjectId> updatesObjectIdsBiMap_;
  InterestsHandler interestsHandler_;
  std::mutex objectsMutex_;
  std::list<ObjectUpdate> objectUpdates_;
  std::unordered_map<ObjectId, decltype(objectUpdates_)::iterator> objectIdToUpdateItr_;
  std::unordered_map<ObjectId, InterestId> objectIdToInterestIdMap_;
  std::shared_ptr<BestEffortPool> bestEffortPool_;
  Guarded<RemoteParticipant*> aRemoteParticipant_ {nullptr};
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

inline ObjectUpdate::ObjectUpdate(NativeObject* localObject)
  : objectGuard_(localObject->weak_from_this()), nativeObject_(localObject)
{
  // compute the maximum size that reliable properties might take. We do this to reserve buffer sizes
  // that would potentially cause memory re-allocations during serialization.
  applyToNativeObject([this](auto* nativeObject)
                      { maxReliableBufferSize_ = nativeObject->senImplComputeMaxReliableSerializedPropertySize(); });

  reset();
}

template <typename F>
inline void ObjectUpdate::applyToNativeObject(F&& func)
{
  if (auto theObject = objectGuard_.lock(); theObject != nullptr)
  {
    func(nativeObject_);
  }
}

inline void ObjectUpdate::reset()
{
  // clear previous buffers
  reliableProperties_ = std::make_shared<ResizableHeapBlock>();
  reliableProperties_->reserve(maxReliableBufferSize_);
  unicastProperties_.clear();
  multicastProperties_.clear();
  multicastPropsSent_ = false;
}

inline void ObjectUpdate::collect(FixedMemoryBlockPool<maxBestEffortMessageSize>& bestEffortPool)
{
  reset();

  ResizableBufferWriter reliableWriter(*reliableProperties_);
  OutputStream reliableStream(reliableWriter);

  BestEffortBlockPtr uniBuffer;
  BestEffortBlockPtr multiBuffer;

  auto uni = [this, &bestEffortPool, &uniBuffer](std::size_t size)
  { return getBestEffortBuffer(bestEffortPool, uniBuffer, unicastProperties_, size); };

  auto multi = [this, &bestEffortPool, &multiBuffer](std::size_t size)
  { return getBestEffortBuffer(bestEffortPool, multiBuffer, multicastProperties_, size); };

  applyToNativeObject([&](auto* nativeObject)
                      { nativeObject->senImplWriteChangedPropertiesToStream(reliableStream, uni, multi); });
}

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_BUS_REMOTE_INTEREST_MANAGER_H
