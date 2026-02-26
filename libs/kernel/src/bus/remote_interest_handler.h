// === remote_interest_handler.h =======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_LIBS_KERNEL_SRC_BUS_REMOTE_INTEREST_HANDLER_H
#define SEN_LIBS_KERNEL_SRC_BUS_REMOTE_INTEREST_HANDLER_H

// kernel
#include "containers.h"

// sen
#include "sen/core/obj/interest.h"

// std
#include <unordered_map>
#include <vector>

namespace sen::kernel::impl
{

// forward declarations
class RemoteParticipant;
class ObjectUpdate;

/// The interest handler manages the local and remote interests registration
class InterestsHandler
{
  SEN_MOVE_ONLY(InterestsHandler)

public:
  InterestsHandler() = default;
  ~InterestsHandler() = default;

public:
  void addInterest(InterestId interestId) { interestToCounterMap_[interestId]++; }
  void removeInterest(InterestId interestId);
  [[nodiscard]] bool isRegistered(InterestId interestId) const noexcept
  {
    return interestToCounterMap_.find(interestId) != interestToCounterMap_.end();
  }

  void onInterestRemoved(std::function<void(InterestId)>&& callback) { onRemovedCallback_ = std::move(callback); }

private:
  std::unordered_map<InterestId, u32> interestToCounterMap_;
  std::function<void(InterestId)> onRemovedCallback_;
};

struct RemoteInterestsHandler
{
  BiMultiMap<RemoteParticipant*, ObjectUpdate*> remotesUpdatesBMMap;                                           // NOLINT
  std::mutex remotesUpdatesBMMapMutex_;                                                                        // NOLINT
  std::unordered_map<RemoteParticipant*, BiMultiMap<InterestId, ObjectUpdate*>> remotesToInterestsUpdatesMap;  // NOLINT
  BiMultiMap<RemoteParticipant*, InterestId> remotesInterestsBMMap;                                            // NOLINT
  BiMultiMap<InterestId, ObjectUpdate*> interestsUpdatesBMMap;                                                 // NOLINT

  void addSubscriber(InterestId interestId, RemoteParticipant* remote, bool newInterest = false);
  std::vector<InterestId>& removeSubscriber(InterestId interestId, RemoteParticipant* remote);
  std::vector<InterestId>& removeSubscriber(RemoteParticipant* remote);
  void addObject(InterestId interestId, ObjectUpdate* update);
  void removeObject(ObjectUpdate* update);
  void clear();
};

//----------------------------------------------------------------------------------------------------------------------
// Inline implementation
//----------------------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------------------------------------//
// InterestsHandler
//---------------------------------------------------------------------------------------------//

inline void InterestsHandler::removeInterest(InterestId interestId)
{
  if (!isRegistered(interestId))
  {
    return;
  }

  if (interestToCounterMap_[interestId] == 1)
  {
    interestToCounterMap_.erase(interestId);
    onRemovedCallback_(interestId);
  }
  else
  {
    interestToCounterMap_[interestId]--;
  }
}

//---------------------------------------------------------------------------------------------//
// RemoteInterestsHandler
//---------------------------------------------------------------------------------------------//

inline void RemoteInterestsHandler::addSubscriber(InterestId interestId, RemoteParticipant* remote, bool newInterest)
{
  remotesInterestsBMMap.add(interestId, remote);

  if (newInterest)
  {
    return;
  }

  auto& remoteUpdateInterests = remotesToInterestsUpdatesMap[remote];
  interestsUpdatesBMMap.forEach(interestId,
                                [this, &remoteUpdateInterests, interestId, remote](ObjectUpdate* update)
                                {
                                  {
                                    const std::lock_guard lock(remotesUpdatesBMMapMutex_);
                                    remotesUpdatesBMMap.add(remote, update);
                                  }
                                  remoteUpdateInterests.add(update, interestId);
                                });
}

inline std::vector<InterestId>& RemoteInterestsHandler::removeSubscriber(InterestId interestId,
                                                                         RemoteParticipant* remote)
{
  auto [orphanRemotes, orphanInterests] = remotesInterestsBMMap.remove(remote, interestId);
  if (!orphanRemotes.empty())
  {
    remotesUpdatesBMMap.remove(remote);
    remotesToInterestsUpdatesMap.erase(remote);
  }
  else
  {
    auto& orphanUpdates = remotesToInterestsUpdatesMap[remote].remove(interestId).second;
    for (auto* update: orphanUpdates)
    {
      remotesUpdatesBMMap.remove(update);
    }
  }
  return orphanInterests;
}

inline std::vector<InterestId>& RemoteInterestsHandler::removeSubscriber(RemoteParticipant* remote)
{
  auto& orphanInterests = remotesInterestsBMMap.remove(remote).second;
  const std::lock_guard lock(remotesUpdatesBMMapMutex_);
  remotesUpdatesBMMap.remove(remote);
  remotesToInterestsUpdatesMap.erase(remote);
  return orphanInterests;
}

inline void RemoteInterestsHandler::addObject(InterestId interestId, ObjectUpdate* update)
{
  interestsUpdatesBMMap.add(interestId, update);
  remotesInterestsBMMap.forEach(interestId,
                                [this, interestId, update](RemoteParticipant* remote)
                                {
                                  const std::lock_guard lock(remotesUpdatesBMMapMutex_);
                                  remotesUpdatesBMMap.add(remote, update);
                                  remotesToInterestsUpdatesMap[remote].add(interestId, update);
                                });
}

inline void RemoteInterestsHandler::removeObject(ObjectUpdate* update)
{
  interestsUpdatesBMMap.remove(update);
  remotesUpdatesBMMap.forEach(update,
                              [this, update](RemoteParticipant* remote)
                              {
                                auto& remoteInterestsUpdates = remotesToInterestsUpdatesMap[remote];
                                remoteInterestsUpdates.remove(update);
                                if (remoteInterestsUpdates.empty())
                                {
                                  remotesToInterestsUpdatesMap.erase(remote);
                                }
                              });
  remotesUpdatesBMMap.remove(update);
}

inline void RemoteInterestsHandler::clear()
{
  remotesUpdatesBMMap.clear();
  remotesToInterestsUpdatesMap.clear();
  remotesInterestsBMMap.clear();
  interestsUpdatesBMMap.clear();
}

}  // namespace sen::kernel::impl

#endif  // SEN_LIBS_KERNEL_SRC_BUS_REMOTE_INTEREST_HANDLER_H
